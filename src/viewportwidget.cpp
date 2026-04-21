#include "viewportwidget.h"
#include "controllayer.h"
#include <QDebug>
#include <QThread>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include "virtualcamera.h"
#include <QShowEvent>

// FIXME: Глючный ресайз на старте, возможно нужен форс-ресайз
class EngineManager;

ViewportWidget::ViewportWidget(AppCore* core, QWidget* parent)
    : QWidget(parent)
    , core(core)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background-color: black;");
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    this->clptr = new ControlLayer(this);
    winId();

    std::cout << "WHI:" << winId() << std::endl;
    std::cout << "WHIP:" << (void*)winId() << std::endl;

    connect(&timer, &QTimer::timeout, this, [this]() {
        if (m_tickCallback) {
            m_tickCallback();
        }
    });

    core->getEventManager().subscribe(name, "engine_ready", &ViewportWidget::connectToTimer, this);
    core->getEventManager().subscribe(name, "get_win_id", &ViewportWidget::initialize, this);
    core->getEventManager().subscribe(name, "get_rec", &ViewportWidget::setReceiver, this);
    core->getEventManager().subscribe(name, "schedule_engine_delete", &ViewportWidget::scheduleEngineDelete, this);
    core->getEventManager().getBusPtr()->registerData("window_handle", winId());
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

    // Update VirtualCamera on resize
    if (!m_vcam) {
        m_vcam = new VirtualCamera();
    } else {
        m_vcam->shutdown();
    }
    m_vcam->init("M3_VirtualCam", w, h);
}

void ViewportWidget::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "send_win_id", winId()));
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
        // ── 1. Delete any engines that were removed from the MessageProcessor thread ──
        // bgfx::shutdown (inside ~ModelEngine) MUST run on this (main) thread.
        {
            std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
            for (auto& entry : _pendingEngineDeletes) {
                delete entry.engine;   // calls ~ModelEngine() → bgfx::shutdown safely
                // Now DLL code is no longer needed; tell DataManager to free the library.
                core->getEventManager().sendMessage(
                    AppMessage("ViewportWidget", "unload_library", entry.path));
            }
            _pendingEngineDeletes.clear();
        }

        // ── 2. Run render pipeline ──
        if (this->ren_pip_ptr) {
            for (auto& func : *this->ren_pip_ptr) {
                if (func) {
                    func();
                }
            }
        }
    };

    if (!timer.isActive()) {
        timer.start(16);
    }

    std::cout << "[READY_T2]" << std::endl;
    core->getEventManager().sendMessage(AppMessage(name, "send_vp", this));
}

ViewportWidget::~ViewportWidget() {
    delete clptr;
    if (m_vcam) {
        m_vcam->shutdown();
        delete m_vcam;
    }
}

void ViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateViewportSize(width(), height());
}

void ViewportWidget::setReceiver(EngineManager* r) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, r]() { setReceiver(r); }, Qt::QueuedConnection);
        return;
    }
    this->eng_receiver = r;
    if (r && isVisible()) {
        updateViewportSize(width(), height());
    }
}

void ViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateViewportSize(event->size().width(), event->size().height());
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

QPaintEngine* ViewportWidget::paintEngine() const {
    return nullptr;
}

void ViewportWidget::paintEvent(QPaintEvent*) {
    //void
}

void ViewportWidget::paintFrame() {
}

void ViewportWidget::scheduleEngineDelete(std::pair<IModel*, std::string> info) {
    // Called from the MessageProcessor thread — just store; deletion happens in the timer tick.
    std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
    _pendingEngineDeletes.push_back({info.first, info.second});
    std::cout << "[ViewportWidget] Engine scheduled for main-thread deletion: " << info.second << std::endl;
}
