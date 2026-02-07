#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimerEvent>
#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/math.h>
#include <fstream>
#include <vector>
#include <iostream>

static bgfx::ShaderHandle loadShader(const char* name) {
    std::string path = "shaders/" + std::string(name) + ".bin";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        qFatal("Cannot open shader: %s", path.c_str());
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    const bgfx::Memory* mem = bgfx::copy(data.data(), uint32_t(size));
    return bgfx::createShader(mem);
}

static bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName) {
    bgfx::ShaderHandle vsh = loadShader(vsName);
    bgfx::ShaderHandle fsh = loadShader(fsName);
    return bgfx::createProgram(vsh, fsh, true);
}

class ViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_initialized(false)
        , m_angle(0.0f)
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setFocusPolicy(Qt::StrongFocus);

        // Обеспечиваем черный фон для виджета
        setStyleSheet("background-color: black;");

        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (m_initialized) paint();
        });
        m_timer.start(16);
    }

    ~ViewportWidget() {
        if (m_initialized) {
            bgfx::shutdown();
        }
    }

    void initialize() {
        if (m_initialized || width() <= 0 || height() <= 0) return;

        bgfx::Init init;
        init.type = bgfx::RendererType::OpenGL;
        init.platformData.nwh = reinterpret_cast<void*>(winId());

        // Получаем реальные размеры с учетом DPI
        QSize size = this->size();
        qreal dpr = devicePixelRatioF();
        init.resolution.width = uint32_t(size.width() * dpr);
        init.resolution.height = uint32_t(size.height() * dpr);
        init.resolution.reset = BGFX_RESET_VSYNC;

        if (!bgfx::init(init)) {
            qFatal("Failed to initialize bgfx");
        }

        // Явно устанавливаем область отображения на весь виджет
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
        setupCube();
        m_initialized = true;
    }


protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        QTimer::singleShot(0, this, [this]() {
            if (!m_initialized) {
                initialize();
            }
        });
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);

        if (m_initialized && event->size().isValid()) {
            QSize newSize = event->size();
            qreal dpr = devicePixelRatioF();

            // Обновляем размеры кадра
            bgfx::reset(uint32_t(newSize.width() * dpr),
                        uint32_t(newSize.height() * dpr),
                        BGFX_RESET_VSYNC);

            // ОЧЕНЬ ВАЖНО: устанавливаем viewport на весь backbuffer
            bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);

            // Принудительно обновляем
            bgfx::touch(0);
            update();
        }
    }

    // Добавьте этот метод для правильной обработки native события
    QPaintEngine* paintEngine() const override {
        return nullptr; // Отключаем стандартный paint engine Qt
    }

    void paintEvent(QPaintEvent*) override {
        // Пустая реализация, так как рендеринг через BGFX
    }

private:
    void paint() {
        if (!m_initialized) return;

        m_angle += 0.01f;

        // Всегда обновляем viewport перед рендерингом
        bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
        bgfx::touch(0);

        const bx::Vec3 eye = { 0.0f, 0.0f, -5.0f };
        const bx::Vec3 at  = { 0.0f, 0.0f,  0.0f };
        const bx::Vec3 up  = { 0.0f, 1.0f,  0.0f };

        float view[16];
        bx::mtxLookAt(view, eye, at, up);

        // Получаем актуальные размеры с учетом DPI
        QSize size = this->size();
        qreal dpr = devicePixelRatioF();
        float width = size.width() * dpr;
        float height = size.height() * dpr;

        // Защита от деления на ноль
        float aspectRatio = (height > 0) ? (width / height) : 1.0f;

        float proj[16];
        bx::mtxProj(proj, 60.0f, aspectRatio, 0.1f, 100.0f,
                    bgfx::getCaps()->homogeneousDepth);

        bgfx::setViewTransform(0, view, proj);

        float model[16];
        bx::mtxRotateXY(model, m_angle, m_angle * 0.7f);
        bgfx::setTransform(model);

        bgfx::setVertexBuffer(0, m_vbh);
        bgfx::setIndexBuffer(m_ibh);
        bgfx::submit(0, m_program);

        bgfx::frame();
    }

    void setupCube() {
        m_program = loadProgram("cube_vertex", "cube_fragment");

        static const float cubeVertices[] = {
            -1.0f,-1.0f,-1.0f,  1.0f,0.0f,0.0f,
            1.0f,-1.0f,-1.0f,  0.0f,1.0f,0.0f,
            1.0f, 1.0f,-1.0f,  0.0f,0.0f,1.0f,
            -1.0f, 1.0f,-1.0f,  1.0f,1.0f,0.0f,
            -1.0f,-1.0f, 1.0f,  1.0f,0.0f,1.0f,
            1.0f,-1.0f, 1.0f,  0.0f,1.0f,1.0f,
            1.0f, 1.0f, 1.0f,  1.0f,1.0f,1.0f,
            -1.0f, 1.0f, 1.0f,  0.5f,0.5f,0.5f,
        };

        static const uint16_t cubeIndices[] = {
            0,1,2, 0,2,3,
            4,6,5, 4,7,6,
            4,5,1, 4,1,0,
            7,2,6, 7,3,2,
            4,0,3, 4,3,7,
            1,5,6, 1,6,2
        };

        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   3, bgfx::AttribType::Float)
            .end();

        m_vbh = bgfx::createVertexBuffer(
            bgfx::makeRef(cubeVertices, sizeof(cubeVertices)),
            layout
            );

        m_ibh = bgfx::createIndexBuffer(
            bgfx::makeRef(cubeIndices, sizeof(cubeIndices))
            );
    }

private:
    bool m_initialized;
    float m_angle;
    QTimer m_timer;

    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_model = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_view = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
};

#endif // VIEWPORTWIDGET_H
