#include "viewportwidget.h"
#include "misc.h"
#include "databus.h"
#include "controllayer.h"
#include <functional>
#include <QDebug>
#include <QThread>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

class EngineManager;

ViewportWidget::ViewportWidget(AppCore* core, QWidget* parent)
    : QWidget(parent)
    , core(core)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background-color: #1a1a1a;");
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
    canonical_win_id_ = static_cast<uintptr_t>(winId());
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
        {
            std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
            for (auto& entry : _pendingEngineDeletes) {
                delete entry.engine;
                core->getEventManager().sendMessage(
                    AppMessage("ViewportWidget", "unload_library", entry.path));
            }
            _pendingEngineDeletes.clear();
        }

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
    };

    if (!timer.isActive()) {
        timer.start(16);
    }

    std::cout << "[READY_T2]" << std::endl;
    core->getEventManager().sendMessage(AppMessage(name, "send_vp", this));
}

ViewportWidget::~ViewportWidget() {
    delete clptr;
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

void ViewportWidget::setAfterFrameCallback(std::function<void()> cb)
{
    m_afterFrame = std::move(cb);
}

void ViewportWidget::scheduleEngineDelete(std::pair<IModel*, std::string> info) {
    // Called from the MessageProcessor thread — just store; deletion happens in the timer tick.
    std::lock_guard<std::mutex> lock(_pendingDeleteMutex);
    _pendingEngineDeletes.push_back({info.first, info.second});
    std::cout << "[ViewportWidget] Engine scheduled for main-thread deletion: " << info.second << std::endl;
}
