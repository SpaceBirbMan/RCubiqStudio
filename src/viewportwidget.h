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

private:
    void initializeBgfx();
    void shutdownBgfx();
    void setupTexturePipeline();

    AppCore* core;
    QTimer timer;
    ControlLayer* clptr = nullptr;
    std::deque<std::shared_ptr<void>> *texture_handlers = new std::deque<std::shared_ptr<void>>(); // тут надо хранить указатели на дескрипторы текстур

    // TODO: заменить на вектор
    std::shared_ptr<void> renderContext;
    bool m_initialized = false;

    const std::string name = "ViewportWidget";

    bgfx::ProgramHandle m_textureProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_vbhQuad = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibhQuad = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_currentTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texColor;

};

#endif // VIEWPORTWIDGET_H
