#include "viewportwidget.h"
#include "controllayer.h"
#include <QDebug>
#include <qthread.h>

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
    core->getEventManager().getBusPtr()->registerData("window_handle", winId());
}

void ViewportWidget::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "send_win_id",winId()));
}

void ViewportWidget::connectToTimer(std::function<void()> fn) {
    std::cout << "[READY_T]" << std::endl;
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, fn]() { this->connectToTimer(fn); }, Qt::QueuedConnection);
            return;
        }

        // тут надо вызывать по очереди коллбеки из вектора
        auto a_ren_pip = &core->getEventManager().getBusPtr()->getData("render_pipeline");
        this->ren_pip_ptr = std::any_cast<std::vector<std::function<void()>>>(a_ren_pip);

        m_tickCallback = [this]() {
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

void ViewportWidget::timerStart(std::function<void()> fn) {

}

ViewportWidget::~ViewportWidget() {
    delete clptr;
}

void ViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

void ViewportWidget::setReceiver(EngineManager* r) {
    this->eng_receiver = r;
}

void ViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ViewportBus stc;
    stc.height = event->size().height();
    stc.width = event->size().width();
    stc.dpr = devicePixelRatioF();

    if (this->eng_receiver != nullptr) {
        core->getEventManager().getDirectSender().send(this->eng_receiver, stc);
    }
}

QPaintEngine* ViewportWidget::paintEngine() const {
    return nullptr;
}

void ViewportWidget::paintEvent(QPaintEvent*) {
    //void
}



void ViewportWidget::paintFrame() {

}


