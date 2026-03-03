#include "viewportwidget.h"
#include "controllayer.h"
#include <QDebug>
#include <fstream>

static bgfx::ShaderHandle loadShader(const char* name) {
    std::string path = "shaders/" + std::string(name) + ".bin";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return BGFX_INVALID_HANDLE;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    const bgfx::Memory* mem = bgfx::copy(data.data(), uint32_t(size));
    return bgfx::createShader(mem);
}

ViewportWidget::ViewportWidget(AppCore* core, QWidget* parent)
    : QWidget(parent)
    , core(core)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background-color: black;");

    this->clptr = new ControlLayer(this);

    core->getEventManager().subscribe(name, "start_streaming", &ViewportWidget::startStreaming, this);
    core->getEventManager().subscribe(name, "get_handlers_queue", &ViewportWidget::sendHandlers, this);

    // Таймер отрисовки (60 FPS)
    connect(&timer, &QTimer::timeout, this, &ViewportWidget::paintFrame);
    timer.start(16);

}

ViewportWidget::~ViewportWidget() {
    if (bgfx::isValid(u_texColor)) {
        bgfx::destroy(u_texColor);
    }
    shutdownBgfx();
    delete clptr;
}

void ViewportWidget::sendHandlers() {
    std::cout << texture_handlers << std::endl;
    core->getEventManager().sendMessage(AppMessage(name, "send_texture_handlers_queue", this->texture_handlers));
}

void ViewportWidget::startStreaming(std::shared_ptr<void> ctx) {


}

void ViewportWidget::setupTexturePipeline() {
    bgfx::ShaderHandle vsh = loadShader("texture.vs");
    bgfx::ShaderHandle fsh = loadShader("texture.fs");

    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        qWarning() << "Shaders not loaded, texture rendering disabled.";
        return;
    }

    m_textureProgram = bgfx::createProgram(vsh, fsh, true);

    static const float quadVertices[] = {
        // X, Y, Z, U, V
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
        1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
        1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
    };

    static const uint16_t quadIndices[] = {
        0, 1, 2, 0, 2, 3
    };

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0,  2, bgfx::AttribType::Float)
        .end();

    m_vbhQuad = bgfx::createVertexBuffer(
        bgfx::makeRef(quadVertices, sizeof(quadVertices)),
        layout
        );

    m_ibhQuad = bgfx::createIndexBuffer(
        bgfx::makeRef(quadIndices, sizeof(quadIndices))
        );

    u_texColor = bgfx::createUniform("u_texColor", bgfx::UniformType::Sampler);
}

void ViewportWidget::initializeBgfx() {
    if (m_initialized) return;

    bgfx::Init init;
    init.type = bgfx::RendererType::OpenGL;
    init.platformData.nwh = reinterpret_cast<void*>(winId());

    QSize size = this->size();
    qreal dpr = devicePixelRatioF();
    init.resolution.width = uint32_t(size.width() * dpr);
    init.resolution.height = uint32_t(size.height() * dpr);
    init.resolution.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(init)) {
        qCritical() << "Failed to initialize bgfx";
        return;
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101010ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);

    setupTexturePipeline();
    m_initialized = true;
}

void ViewportWidget::shutdownBgfx() {
    if (m_initialized) {
        bgfx::shutdown();
        m_initialized = false;
    }
}

void ViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!m_initialized) {
        initializeBgfx();
    }
}

void ViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_initialized) {
        QSize newSize = event->size();
        qreal dpr = devicePixelRatioF();
        bgfx::reset(uint32_t(newSize.width() * dpr), uint32_t(newSize.height() * dpr), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
    }
}

QPaintEngine* ViewportWidget::paintEngine() const {
    return nullptr;
}

void ViewportWidget::paintEvent(QPaintEvent*) {
    //void
}

void ViewportWidget::paintFrame() {
    if (!m_initialized) {
        if (isVisible() && width() > 0) {
            initializeBgfx();
        }
        return;
    }

    if (texture_handlers != nullptr) {
    if (!texture_handlers->empty()) {
        auto tptr = texture_handlers->front();
        texture_handlers->pop_front();
        std::cout << "RER2" << std::endl;
        auto typedPtr = std::static_pointer_cast<bgfx::TextureHandle>(tptr);
        m_currentTexture = *typedPtr;

        // нужна синхронизация

    }
    }

    bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::touch(0);

    if (bgfx::isValid(m_currentTexture) && bgfx::isValid(m_textureProgram)) {
        float identity[16];
        bx::mtxIdentity(identity);
        bgfx::setTransform(identity);

        bgfx::setVertexBuffer(0, m_vbhQuad);
        bgfx::setIndexBuffer(m_ibhQuad);

        bgfx::setTexture(0, u_texColor, m_currentTexture);

        bgfx::submit(0, m_textureProgram);
    }

    bgfx::frame();
}


