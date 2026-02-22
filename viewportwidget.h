#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#define NOBYTE
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include <QWidget>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>

#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/math.h>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

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

static bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName) {
    bgfx::ShaderHandle vsh = loadShader(vsName);
    bgfx::ShaderHandle fsh = loadShader(fsName);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::createProgram(vsh, fsh, true);
}

class ViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_initialized(false)
        , m_socket(INVALID_SOCKET)
        , m_running(true)
        , m_pitch(0.0f)
        , m_yaw(0.0f)
        , m_roll(0.0f)
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setFocusPolicy(Qt::StrongFocus);
        setStyleSheet("background-color: black;");

        m_udpThread = std::thread(&ViewportWidget::udpListenerLoop, this);

        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (m_initialized) paint();
        });
        m_timer.start(16);
    }

    ~ViewportWidget() {
        m_running = false;
        if (m_udpThread.joinable()) {
            m_udpThread.join();
        }

        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            WSACleanup();
        }

        if (m_initialized) {
            bgfx::shutdown();
        }
    }

    void initialize() {
        if (m_initialized || width() <= 0 || height() <= 0) return;

        bgfx::Init init;
        init.type = bgfx::RendererType::OpenGL;
        init.platformData.nwh = reinterpret_cast<void*>(winId());

        QSize size = this->size();
        qreal dpr = devicePixelRatioF();
        init.resolution.width = uint32_t(size.width() * dpr);
        init.resolution.height = uint32_t(size.height() * dpr);
        init.resolution.reset = BGFX_RESET_VSYNC;

        if (!bgfx::init(init)) {
            qFatal("Failed to initialize bgfx");
            return;
        }

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
            bgfx::reset(uint32_t(newSize.width() * dpr),
                        uint32_t(newSize.height() * dpr),
                        BGFX_RESET_VSYNC);
            bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
            bgfx::touch(0);
            update();
        }
    }

    QPaintEngine* paintEngine() const override {
        return nullptr;
    }

    void paintEvent(QPaintEvent*) override {
    }

private:
    void udpListenerLoop() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return;
        }

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            WSACleanup();
            return;
        }

        int opt = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(11573);

        if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(m_socket);
            WSACleanup();
            return;
        }

        char buffer[2048];
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);

        while (m_running.load()) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(m_socket, &readfds);
            timeval timeout = { 0, 50000 };

            if (select(0, &readfds, NULL, NULL, &timeout) > 0) {
                int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer), 0, (sockaddr*)&fromAddr, &fromLen);
                if (bytesReceived > 100) {
                    parseHeadRotation(buffer, bytesReceived);
                }
            }
        }

        closesocket(m_socket);
        WSACleanup();
    }

    void parseHeadRotation(const char* buffer, int size) {
        if (size < 61) return;

        size_t offset = 0;
        offset += 8; // Пропуск заголовков согласно вашей логике
        offset += 4;
        offset += 8;
        offset += 8;
        offset += 1;

        uint8_t success = 0;
        memcpy(&success, buffer + offset - 1, 1);

        offset += 4;
        offset += 16;

        float eulerDeg[3];
        memcpy(eulerDeg, buffer + offset, sizeof(float) * 3);

        // 1. Исправление скачков через 180 градусов (Unwrapping)
        // Сравниваем с предыдущим кадром. Если разница > 180, значит произошел переход через границу.
        auto fixJump = [](float current, float previous) -> float {
            float diff = current - previous;
            if (diff > 180.0f) {
                current -= 360.0f;
            } else if (diff < -180.0f) {
                current += 360.0f;
            }
            return current;
        };

        float pitch = fixJump(eulerDeg[0], m_prevPitch);
        float yaw   = fixJump(eulerDeg[1], m_prevYaw);
        float roll  = fixJump(eulerDeg[2], m_prevRoll);

        // Сохраняем "сырые" (но уже исправленные от скачков) значения для следующего кадра
        // Важно: сохраняем именно то, что использовали для расчета, чтобы следующая итерация работала корректно
        m_prevPitch = pitch;
        m_prevYaw   = yaw;
        m_prevRoll  = roll;

        // 2. Конвертация в радианы
        m_pitch = bx::toRad(pitch);
        m_yaw   = bx::toRad(yaw);
        m_roll  = bx::toRad(roll);
    }

    void paint() {
        if (!m_initialized) return;

        bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
        bgfx::touch(0);

        const bx::Vec3 eye = { 0.0f, 0.0f, -5.0f };
        const bx::Vec3 at  = { 0.0f, 0.0f,  0.0f };
        const bx::Vec3 up  = { 0.0f, 1.0f,  0.0f };

        float view[16];
        bx::mtxLookAt(view, eye, at, up);

        QSize size = this->size();
        qreal dpr = devicePixelRatioF();
        float width = size.width() * dpr;
        float height = size.height() * dpr;
        float aspectRatio = (height > 0) ? (width / height) : 1.0f;

        float proj[16];
        bx::mtxProj(proj, 60.0f, aspectRatio, 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

        bgfx::setViewTransform(0, view, proj);


        float model[16];


        float rotMat[16];
        bx::mtxRotateXYZ(rotMat, -m_yaw, m_pitch, m_roll);
        std::cout<< std::to_string(m_pitch) << std::endl;


        memcpy(model, rotMat, sizeof(model));

        bgfx::setTransform(model);

        if (bgfx::isValid(m_vbh) && bgfx::isValid(m_ibh) && bgfx::isValid(m_program)) {
            bgfx::setVertexBuffer(0, m_vbh);
            bgfx::setIndexBuffer(m_ibh);
            bgfx::submit(0, m_program);
        }

        bgfx::frame();
    }

    void setupCube() {
        m_program = loadProgram("cube_vertex", "cube_fragment");
        if (!bgfx::isValid(m_program)) return;

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
    QTimer m_timer;

    std::thread m_udpThread;
    std::atomic<bool> m_running;
    SOCKET m_socket;

    float m_pitch;
    float m_yaw;
    float m_roll;

    float m_prevPitch = 0.0f;
    float m_prevYaw = 0.0f;
    float m_prevRoll = 0.0f;

    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
};

#endif // VIEWPORTWIDGET_H
