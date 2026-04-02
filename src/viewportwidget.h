#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#define NOBYTE
#define WIN32_LEAN_AND_MEAN

#include <QWidget>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <memory>

#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/math.h>

#include "appcore.h"

class ControlLayer;
class EngineManager;

class ViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(AppCore* core, QWidget* parent = nullptr);
    ~ViewportWidget() override;

    void startStreaming(std::shared_ptr<void> ctx);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent*) override;
    void sendHandlers();

private slots:
    void paintFrame();
    void connectToTimer(std::function<void()> fn);

private:

    AppCore* core;
    QTimer timer;
    ControlLayer* clptr = nullptr;
    std::function<void()> m_tickCallback;
    std::vector<std::function<void()>>* ren_pip_ptr;

    std::shared_ptr<void> renderContext;
    bool m_initialized = false;
    void* eng_receiver = nullptr;

    const std::string name = "ViewportWidget";

    void timerStart(std::function<void()> fn);
    void initialize();
    void setReceiver(EngineManager* r);

};

#endif // VIEWPORTWIDGET_H
