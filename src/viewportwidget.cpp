#include "viewportwidget.h"
#include <QCoreApplication>
#include "rcqapi.h"
#include "logger.h"
#include "consts.h"
#include "viewportplaceholderoverlay.h"
#include "databus.h"
#include "controllayer.h"
#include <functional>
#include <QPalette>
#include <QColor>
#include <QThread>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QEvent>
#include <QCoreApplication>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include "spoutsender.h"
#endif

class EngineManager;

/// Child surface that forwards HWND to bgfx; the parent ViewportWidget draws a normal Qt background when this is hidden.
class EngineSurfaceWidget final : public QWidget {
public:
    explicit EngineSurfaceWidget(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setFocusPolicy(Qt::StrongFocus);
    }
protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent*) override {}
};

ViewportWidget::ViewportWidget(AppCore* core, QWidget* parent)
    : QWidget(parent)
    , core(core)
{
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(0x1a, 0x1a, 0x1a));
        setPalette(pal);
    }
    setFocusPolicy(Qt::StrongFocus);

    engine_surface_ = new EngineSurfaceWidget(this);
    engine_surface_->setObjectName(QStringLiteral("viewport_engine_surface"));
    engine_surface_->installEventFilter(this);
    engine_surface_->hide();
    engine_surface_->winId();

    m_placeholder = new ViewportPlaceholder(this);
    m_placeholder->setGeometry(0, 0, width(), height());
    m_placeholder->show();
    QTimer::singleShot(150, this, [this]() {
        if (m_placeholder && m_placeholder->isVisible() && !m_tickCallback)
            m_placeholder->setAnimating(true);
    });

    this->clptr = new ControlLayer(this);

    connect(&timer, &QTimer::timeout, this, [this]() {
        if (m_tickCallback) {
            m_tickCallback();
        }
    });

    core->getEventManager().subscribe(name, "engine_ready", &ViewportWidget::connectToTimer, this);
    core->getEventManager().subscribe(name, "engine_init_render", &ViewportWidget::runEngineInitRender, this);
    core->getEventManager().subscribe(name, "get_win_id", &ViewportWidget::initialize, this);
    core->getEventManager().subscribe(name, "get_rec", &ViewportWidget::setReceiver, this);
    core->getEventManager().subscribe(name, "schedule_engine_delete", &ViewportWidget::scheduleEngineDelete, this);
    core->getEventManager().subscribe(name, "flush_engine_deletes", &ViewportWidget::flushPendingEngineDeletes, this);
    core->getEventManager().subscribe(name, "clear_viewport_surface", &ViewportWidget::clearNativeSurface, this);
    core->getEventManager().subscribe(name, AppShutdownEvents::kStopEngineTick, &ViewportWidget::stopRenderTick, this);
    core->getEventManager().subscribe<std::string>(
        name, AppRenderingEvents::kRenderingInactive, &ViewportWidget::onRenderingInactive, this);
    core->getEventManager().subscribe<int>(
        name, AppRenderingEvents::kRenderingActive,   &ViewportWidget::onRenderingActive, this);
    canonical_win_id_ = static_cast<uintptr_t>(engine_surface_->winId());
    if (auto* hw = bus_handle_cast<uintptr_t>(core->getEventManager().getBusPtr(), "window_handle"))
        hw->setLive(&canonical_win_id_);
    this->viewport_size[0] = this->size().width();
    this->viewport_size[1] = this->size().height();
    viewport_dpr_ = devicePixelRatioF();

    core->getEventManager().getBusPtr()->registerData("viewport_size", &viewport_size);
    core->getEventManager().getBusPtr()->registerData("viewport_dpr", &viewport_dpr_);
    core->getEventManager().getBusPtr()->registerData("viewport_commands_deque", &commandQueue);
}

std::string ViewportWidget::mouseButtonName(Qt::MouseButton btn)
{
    switch (btn) {
    case Qt::LeftButton:   return "Left";
    case Qt::RightButton:  return "Right";
    case Qt::MiddleButton: return "Middle";
    case Qt::BackButton:   return "Back";
    case Qt::ForwardButton: return "Forward";
    default: return {};
    }
}

void ViewportWidget::setMouseButtonHeld(const std::string& btn, bool held)
{
    if (btn.empty() || !keyboardState_)
        return;
    std::lock_guard<std::mutex> lk(keyboardState_->mutex);
    if (held)
        keyboardState_->mouseButtonsHeld.insert(btn);
    else
        keyboardState_->mouseButtonsHeld.erase(btn);
}

void ViewportWidget::pushViewportCommand(ViewportCommand cmd)
{
    if (!hostInput_) {
        try {
            auto& hi = core->getEventManager().getBusPtr()->getData("host_input");
            hostInput_ = std::any_cast<HostInputControllers*>(hi);
        } catch (...) {
            hostInput_ = nullptr;
        }
    }
    if (!keyboardState_) {
        try {
            auto& kb = core->getEventManager().getBusPtr()->getData("keyboard_state");
            keyboardState_ = std::any_cast<std::shared_ptr<KeyboardKeysState>>(kb);
        } catch (...) {
            keyboardState_.reset();
        }
    }

    const int vw = std::max(1, viewport_size[0]);
    const int vh = std::max(1, viewport_size[1]);
    if (hostInput_) {
        hostInput_->updateMouseNormalized(
            std::clamp(cmd.mouseX / float(vw), 0.0f, 1.0f),
            std::clamp(cmd.mouseY / float(vh), 0.0f, 1.0f));
        if (cmd.scroll != 0)
            hostInput_->addWheelDelta(float(cmd.scroll));
    }

    for (const auto& c : cmd.currentCommand) {
        if (c == "left_down")   setMouseButtonHeld("Left", true);
        else if (c == "left_up")     setMouseButtonHeld("Left", false);
        else if (c == "right_down")  setMouseButtonHeld("Right", true);
        else if (c == "right_up")    setMouseButtonHeld("Right", false);
        else if (c == "middle_down") setMouseButtonHeld("Middle", true);
        else if (c == "middle_up")   setMouseButtonHeld("Middle", false);
    }

    commandQueue.push_back(std::move(cmd));
}

void ViewportWidget::updateViewportSize(int w, int h) {
    this->viewport_size[0] = w;
    this->viewport_size[1] = h;
    viewport_dpr_ = devicePixelRatioF();

    ViewportBus stc;
    stc.height = h;
    stc.width = w;
    stc.dpr = viewport_dpr_;

    if (this->eng_receiver != nullptr && m_acceptViewportResize) {
        core->getEventManager().getDirectSender().send(this->eng_receiver, stc);
    }

    if (w < 2 || h < 2)
        return;
}

void ViewportWidget::initialize() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { initialize(); }, Qt::QueuedConnection);
        return;
    }
    recreateEngineSurface();
    hidePlaceholder();
    layoutEngineSurface();
    updateViewportSize(width(), height());
    if (engine_surface_) {
        engine_surface_->show();
        engine_surface_->raise();
        (void)engine_surface_->winId();
    }
    canonical_win_id_ = engine_surface_
        ? static_cast<uintptr_t>(engine_surface_->winId())
        : static_cast<uintptr_t>(winId());
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    core->getEventManager().getLogger().module(name).info(
        "initialize: engine surface HWND=" + std::to_string(canonical_win_id_));
    core->getEventManager().sendMessage(AppMessage(name, "send_win_id", canonical_win_id_));
}

void ViewportWidget::connectToTimer(std::function<void()> fn) {
    std::cout << "[READY_T]" << std::endl;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, fn]() { this->connectToTimer(fn); }, Qt::QueuedConnection);
        return;
    }

    auto a_ren_pip = &core->getEventManager().getBusPtr()->getData("render_pipeline");
    try {
        this->ren_pip_ptr = std::any_cast<std::vector<std::function<void()>>>(a_ren_pip);
    } catch (...) {
        this->ren_pip_ptr = nullptr;
    }

    m_tickCallback = [this]() {
        if (this->ren_pip_ptr) {
            std::lock_guard<std::mutex> pipLock{HostInterop::renderPipelineMutex()};
            for (auto& func : *this->ren_pip_ptr) {
                if (func) {
                    func();
                }
            }
        }

        if (m_afterFrame) {
            m_afterFrame();
        }
#ifdef _WIN32
        if (engine_surface_) {
            spoutAfterRenderTick(core->getEventManager().getBusPtr(), // тут так нельзя, спаут - не постоянная обозначенная функиця
                                   reinterpret_cast<void*>(static_cast<quintptr>(engine_surface_->winId())),
                                   viewport_size[0],
                                   viewport_size[1]); // А, так они пустые вообще, bruh
        }
#endif
    };

    if (!timer.isActive()) {
        timer.start(16);
    }
    m_acceptViewportResize = true;
    QTimer::singleShot(0, this, [this]() {
        layoutEngineSurface();
        updateViewportSize(width(), height());
    });

    std::cout << "[READY_T2]" << std::endl;
    core->getEventManager().sendMessage(AppMessage(name, "send_vp", this));
}

ViewportWidget::~ViewportWidget() {
    delete clptr;
}

void ViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    layoutEngineSurface();
    updateViewportSize(width(), height());
    // After layout/dock placement, size can still be stale until the next event loop tick.
    QTimer::singleShot(0, this, [this]() {
        layoutEngineSurface();
        updateViewportSize(width(), height());
    });
}

void ViewportWidget::setReceiver(EngineManager* r) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, r]() { setReceiver(r); }, Qt::QueuedConnection);
        return;
    }
    this->eng_receiver = r;
    if (r) {
        updateViewportSize(width(), height());
        QTimer::singleShot(0, this, [this]() {
            layoutEngineSurface();
            updateViewportSize(width(), height());
        });
    }
}

void ViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    layoutEngineSurface();
    updateViewportSize(event->size().width(), event->size().height());
}

void ViewportWidget::layoutEngineSurface()
{
    if (engine_surface_)
        engine_surface_->setGeometry(0, 0, width(), height());
    if (m_placeholder)
        m_placeholder->setGeometry(0, 0, width(), height());
}

bool ViewportWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != engine_surface_)
        return QWidget::eventFilter(watched, event);

    const QEvent::Type t = event->type();
    if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease || t == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        ViewportCommand cmd;
        cmd.mouseX = me->position().toPoint().x();
        cmd.mouseY = me->position().toPoint().y();
        if (t == QEvent::MouseButtonPress) {
            if (me->button() == Qt::LeftButton) cmd.currentCommand.insert("left_down");
            else if (me->button() == Qt::RightButton) cmd.currentCommand.insert("right_down");
            else if (me->button() == Qt::MiddleButton) cmd.currentCommand.insert("middle_down");
        } else if (t == QEvent::MouseButtonRelease) {
            if (me->button() == Qt::LeftButton) cmd.currentCommand.insert("left_up");
            else if (me->button() == Qt::RightButton) cmd.currentCommand.insert("right_up");
            else if (me->button() == Qt::MiddleButton) cmd.currentCommand.insert("middle_up");
        } else if (t == QEvent::MouseMove && (me->buttons() & Qt::LeftButton)) {
            cmd.currentCommand.insert("dragging");
        }
        pushViewportCommand(cmd);
        return false;
    }
    if (t == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        ViewportCommand cmd;
        cmd.mouseX = we->position().toPoint().x();
        cmd.mouseY = we->position().toPoint().y();
        cmd.scroll = we->angleDelta().y();
        pushViewportCommand(cmd);
        return false;
    }
    return QWidget::eventFilter(watched, event);
}

void ViewportWidget::mousePressEvent(QMouseEvent* event) {
    QPoint pos = event->pos();
    ViewportCommand cmd;
    cmd.mouseX = pos.x();
    cmd.mouseY = pos.y();

    if (event->button() == Qt::LeftButton) {
        cmd.currentCommand.insert("left_down");
    } else if (event->button() == Qt::RightButton) {
        cmd.currentCommand.insert("right_down");
    } else if (event->button() == Qt::MiddleButton) {
        cmd.currentCommand.insert("middle_down");
    }

    pushViewportCommand(cmd);
    QWidget::mousePressEvent(event);
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* event) {
    QPoint pos = event->pos();
    ViewportCommand cmd;
    cmd.mouseX = pos.x();
    cmd.mouseY = pos.y();

    if (event->button() == Qt::LeftButton) {
        cmd.currentCommand.insert("left_up");
    } else if (event->button() == Qt::RightButton) {
        cmd.currentCommand.insert("right_up");
    } else if (event->button() == Qt::MiddleButton) {
        cmd.currentCommand.insert("middle_up");
    }

    pushViewportCommand(cmd);
    QWidget::mouseReleaseEvent(event);
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint pos = event->pos();
    ViewportCommand cmd;
    cmd.mouseX = pos.x();
    cmd.mouseY = pos.y();

    if (event->buttons() & Qt::LeftButton) {
        cmd.currentCommand.insert("dragging");
    }

    pushViewportCommand(cmd);
    QWidget::mouseMoveEvent(event);
}

void ViewportWidget::wheelEvent(QWheelEvent* event) {
    ViewportCommand cmd;
    cmd.mouseX = event->position().toPoint().x();
    cmd.mouseY = event->position().toPoint().y();
    cmd.scroll = event->angleDelta().y();

    pushViewportCommand(cmd);
    QWidget::wheelEvent(event);
}

void ViewportWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
}

void ViewportWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
}

void ViewportWidget::paintFrame() {
}

void ViewportWidget::setAfterFrameCallback(std::function<void()> cb)
{
    m_afterFrame = std::move(cb);
}

void ViewportWidget::stopRenderTick() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this,
                                  [this]() { stopRenderTick(); },
                                  Qt::BlockingQueuedConnection);
        return;
    }
    m_acceptViewportResize = false;
    timer.stop();
    m_tickCallback = nullptr;
    m_afterFrame = nullptr;
    if (engine_surface_)
        engine_surface_->hide();
    core->getEventManager().getLogger().module(name).info("stop_render_tick: QTimer stopped");
}

void ViewportWidget::runEngineInitRender(std::function<void()> fn) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this,
                                  [this, fn = std::move(fn)]() mutable {
                                      runEngineInitRender(std::move(fn));
                                  },
                                  Qt::QueuedConnection);
        return;
    }
    if (fn) {
        layoutEngineSurface();
        updateViewportSize(width(), height());
        core->getEventManager().getLogger().module(name).info("engine_init_render: on GUI thread");
        fn();
    }
}

void ViewportWidget::recreateEngineSurface() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { recreateEngineSurface(); }, Qt::BlockingQueuedConnection);
        return;
    }
    if (engine_surface_) {
        engine_surface_->hide();
        engine_surface_->removeEventFilter(this);
        delete engine_surface_;
        engine_surface_ = nullptr;
    }
    engine_surface_ = new EngineSurfaceWidget(this);
    engine_surface_->setObjectName(QStringLiteral("viewport_engine_surface"));
    engine_surface_->installEventFilter(this);
    engine_surface_->hide();
    layoutEngineSurface();
    (void)engine_surface_->winId();
    canonical_win_id_ = static_cast<uintptr_t>(engine_surface_->winId());
}

void ViewportWidget::clearNativeSurface() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { clearNativeSurface(); }, Qt::BlockingQueuedConnection);
        return;
    }
    m_acceptViewportResize = false;
    if (engine_surface_)
        engine_surface_->hide();
    showPlaceholder();
    recreateEngineSurface();
    update();
}

void ViewportWidget::flushPendingEngineDeletes() {
    if (QThread::currentThread() != thread()) {
        const auto t0 = std::chrono::steady_clock::now();
        core->getEventManager().getLogger().module(name).info(
            "flush_engine_deletes: cross-thread invoke (caller thread="
            + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ")");
        QMetaObject::invokeMethod(this,
                                  [this]() { flushPendingEngineDeletes(); },
                                  Qt::BlockingQueuedConnection);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        core->getEventManager().getLogger().module(name).info(
            "flush_engine_deletes: cross-thread invoke done (" + std::to_string(ms) + " ms)");
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<PendingEngineDelete> batch;
    {
        std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
        batch.swap(_pendingEngineDeletes);
    }

    for (auto& entry : batch) {
        core->getEventManager().getLogger().module(name).info(
            "flush_engine_deletes: deleting engine " + entry.path);
#ifdef _WIN32
        spoutNotifyEngineDeviceReset();
#endif
        delete entry.engine;
        core->getEventManager().sendMessage(
            AppMessage("ViewportWidget", "unload_library", entry.path));
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (!batch.empty()) {
        core->getEventManager().getLogger().module(name).info(
            "flush_engine_deletes: removed " + std::to_string(batch.size())
            + " engine(s) in " + std::to_string(ms) + " ms");
    }
}

void ViewportWidget::scheduleEngineDelete(std::pair<IModel*, std::string> info) {
    {
        std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
        _pendingEngineDeletes.push_back({info.first, info.second});
    }

    std::cout << "[ViewportWidget] Engine scheduled for main-thread deletion: " << info.second << std::endl;
}

// ── Placeholder helpers ──────────────────────────────────────────────────────

void ViewportWidget::showPlaceholder(const std::string& hint)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, hint]() { showPlaceholder(hint); }, Qt::QueuedConnection);
        return;
    }
    if (!m_placeholder)
        return;

    if (m_placeholder)
        m_placeholder->setAnimating(false);

    if (engine_surface_)
        engine_surface_->hide();
    layoutEngineSurface();

    const QString q = hint.empty()
        ? QCoreApplication::translate("ViewportPlaceholder", "placeholder_hint")
        : QString::fromStdString(hint);
    m_placeholder->setHint(q);
    m_placeholder->raise();
    m_placeholder->show();

    // Defer spinner until engine teardown / tab removal finishes (avoids QFontCache reentrancy).
    QTimer::singleShot(120, this, [this]() {
        if (m_placeholder && m_placeholder->isVisible())
            m_placeholder->setAnimating(true);
    });
}

void ViewportWidget::hidePlaceholder()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { hidePlaceholder(); }, Qt::BlockingQueuedConnection);
        return;
    }
    if (m_placeholder) {
        m_placeholder->setAnimating(false);
        m_placeholder->hide();
    }
    if (engine_surface_) {
        layoutEngineSurface();
        engine_surface_->show();
        engine_surface_->raise();
    }
}

void ViewportWidget::onRenderingInactive(std::string hint)
{
    showPlaceholder(hint.empty() ? std::string{} : hint);
}

void ViewportWidget::onRenderingActive(int)
{
    hidePlaceholder();
}
