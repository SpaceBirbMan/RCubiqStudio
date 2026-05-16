#include "viewportwidget.h"
#include "misc.h"
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
#include <iostream>

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

    this->clptr = new ControlLayer(this);

    connect(&timer, &QTimer::timeout, this, [this]() {
        if (m_tickCallback) {
            m_tickCallback();
        }
    });

    core->getEventManager().subscribe(name, "engine_ready", &ViewportWidget::connectToTimer, this);
    core->getEventManager().subscribe(name, "get_win_id", &ViewportWidget::initialize, this);
    core->getEventManager().subscribe(name, "get_rec", &ViewportWidget::setReceiver, this);
    core->getEventManager().subscribe(name, "schedule_engine_delete", &ViewportWidget::scheduleEngineDelete, this);
    core->getEventManager().subscribe(name, "clear_viewport_surface", &ViewportWidget::clearNativeSurface, this);
    canonical_win_id_ = static_cast<uintptr_t>(engine_surface_->winId());
    if (auto* hw = bus_handle_cast<uintptr_t>(core->getEventManager().getBusPtr(), "window_handle"))
        hw->setLive(&canonical_win_id_);
    this->viewport_size[0] = this->size().width();
    this->viewport_size[1] = this->size().height();

    core->getEventManager().getBusPtr()->registerData("viewport_size", &viewport_size);
    core->getEventManager().getBusPtr()->registerData("viewport_commands_deque", &commandQueue);
}

void ViewportWidget::updateViewportSize(int w, int h) {
    this->viewport_size[0] = w;
    this->viewport_size[1] = h;

    ViewportBus stc;
    stc.height = h;
    stc.width = w;
    stc.dpr = devicePixelRatioF();

    if (this->eng_receiver != nullptr) {
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
    layoutEngineSurface();
    if (engine_surface_) {
        engine_surface_->show();
        engine_surface_->raise();
    }
    core->getEventManager().sendMessage(AppMessage(name, "send_win_id", engine_surface_ ? engine_surface_->winId() : winId()));
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
        flushPendingEngineDeletes();

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
            spoutAfterRenderTick(core->getEventManager().getBusPtr(),
                                   reinterpret_cast<void*>(static_cast<quintptr>(engine_surface_->winId())),
                                   viewport_size[0],
                                   viewport_size[1]);
        }
#endif
    };

    if (!timer.isActive()) {
        timer.start(16);
    }
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
    if (!engine_surface_)
        return;
    engine_surface_->setGeometry(0, 0, width(), height());
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
        commandQueue.push_back(cmd);
        return false;
    }
    if (t == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        ViewportCommand cmd;
        cmd.mouseX = we->position().toPoint().x();
        cmd.mouseY = we->position().toPoint().y();
        cmd.scroll = we->angleDelta().y();
        commandQueue.push_back(cmd);
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

    commandQueue.push_back(cmd);
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

    commandQueue.push_back(cmd);
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

    commandQueue.push_back(cmd);
    QWidget::mouseMoveEvent(event);
}

void ViewportWidget::wheelEvent(QWheelEvent* event) {
    ViewportCommand cmd;
    cmd.mouseX = event->position().toPoint().x();
    cmd.mouseY = event->position().toPoint().y();
    cmd.scroll = event->angleDelta().y();

    commandQueue.push_back(cmd);
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

void ViewportWidget::clearNativeSurface() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { clearNativeSurface(); }, Qt::QueuedConnection);
        return;
    }
    if (engine_surface_)
        engine_surface_->hide();
    update();
}

void ViewportWidget::flushPendingEngineDeletes() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this,
                                  [this]() { flushPendingEngineDeletes(); },
                                  Qt::BlockingQueuedConnection);
        return;
    }

    std::vector<PendingEngineDelete> batch;
    {
        std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
        batch.swap(_pendingEngineDeletes);
    }

    for (auto& entry : batch) {
#ifdef _WIN32
        spoutNotifyEngineDeviceReset();
#endif
        delete entry.engine;
        core->getEventManager().sendMessage(
            AppMessage("ViewportWidget", "unload_library", entry.path));
    }
}

void ViewportWidget::scheduleEngineDelete(std::pair<IModel*, std::string> info) {
    {
        std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
        _pendingEngineDeletes.push_back({info.first, info.second});
    }

    QMetaObject::invokeMethod(this,
                              [this]() { flushPendingEngineDeletes(); },
                              Qt::QueuedConnection);

    std::cout << "[ViewportWidget] Engine scheduled for main-thread deletion: " << info.second << std::endl;
}
