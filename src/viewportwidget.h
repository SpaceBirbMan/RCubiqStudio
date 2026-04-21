#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#define NOBYTE
#define WIN32_LEAN_AND_MEAN

#include <QWidget>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <utility>

#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/math.h>

#include "appcore.h"

class ControlLayer;
class EngineManager;
class VirtualCamera;

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
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void paintFrame();
    void connectToTimer(std::function<void()> fn);

private:

    AppCore* core;
    QTimer timer;
    ControlLayer* clptr = nullptr;
    std::function<void()> m_tickCallback;
    std::vector<std::function<void()>>* ren_pip_ptr;
    std::deque<ViewportCommand> commandQueue {};

    std::shared_ptr<void> renderContext;
    bool m_initialized = false;
    void* eng_receiver = nullptr;

    // Pending engine deletes: engine must be destroyed on main thread because bgfx::shutdown
    // must be called from the thread that called bgfx::init (the render/main thread).
    struct PendingEngineDelete {
        IModel*     engine;
        std::string path;
    };
    std::vector<PendingEngineDelete> _pendingEngineDeletes;
    std::mutex                       _pendingDeleteMutex;

    const std::string name = "ViewportWidget";

    void timerStart(std::function<void()> fn);
    void initialize();
    void setReceiver(EngineManager* r);
    void updateViewportSize(int w, int h);
    void scheduleEngineDelete(std::pair<IModel*, std::string> info);

    int viewport_size[2] {0, 0};
    VirtualCamera* m_vcam = nullptr;

};

#endif // VIEWPORTWIDGET_H
