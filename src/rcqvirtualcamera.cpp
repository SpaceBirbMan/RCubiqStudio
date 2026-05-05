#include "rcqvirtualcamera.h"

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QWidget>
#include <QtGlobal>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#    include <windows.h>
#endif

struct RcqVirtualCamera::Impl {
#ifdef _WIN32
    HMODULE hlib = nullptr;
#endif
    void* vcam = nullptr;
    void* (*p_open)() = nullptr;
    void (*p_close)(void*) = nullptr;
    int (*p_stream_start)(void*, const char*) = nullptr;
    int (*p_stream_send)(void*,
                         const char*,
                         const char*,
                         int,
                         int,
                         const char**,
                         size_t*) = nullptr;
    int (*p_stream_stop)(void*, const char*) = nullptr;
    int (*p_format)(void*,
                    const char*,
                    int,
                    char*,
                    size_t*,
                    int*,
                    int*,
                    int*,
                    int*) = nullptr;
    int (*p_data_mode)(void*, char*, size_t) = nullptr;
};

RcqVirtualCamera::RcqVirtualCamera() = default;

RcqVirtualCamera::RcqVirtualCamera(RcqVirtualCamera&&) noexcept = default;

RcqVirtualCamera& RcqVirtualCamera::operator=(RcqVirtualCamera&&) noexcept = default;

RcqVirtualCamera::~RcqVirtualCamera()
{
    stopStream();
    unload();
}

static QImage makeTestPatternRgb(int w, int h, int phase)
{
    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        auto* line = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            line[3 * x + 0] = static_cast<uchar>((x * 255 / std::max(1, w - 1) + phase) % 256);
            line[3 * x + 1] = static_cast<uchar>((y * 255 / std::max(1, h - 1)) % 256);
            line[3 * x + 2] = static_cast<uchar>((phase * 2) % 256);
        }
    }
    return img;
}

static QPixmap captureViewportForDriver(QWidget* viewport)
{
    WId win = viewport->winId();
    if (!win) {
        return {};
    }

    QScreen* screen = viewport->screen();
    if (!screen) {
        const QPoint c = viewport->mapToGlobal(QPoint(viewport->width() / 2, viewport->height() / 2));
        screen = QGuiApplication::screenAt(c);
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        QPixmap pm = screen->grabWindow(win, 0, 0, -1, -1);
        if (!pm.isNull() && pm.width() >= 2 && pm.height() >= 2) {
            return pm;
        }
    }

    return viewport->grab();
}

#    ifdef _WIN32
static FARPROC vcamProc(HMODULE h, const char* name)
{
    return GetProcAddress(h, name);
}
#    endif

bool RcqVirtualCamera::loadFromApplicationDir()
{
    if (m_impl) {
        return true;
    }

#ifdef _WIN32
    m_impl = std::make_unique<Impl>();
    const std::string base = QApplication::applicationDirPath().toLocal8Bit().toStdString();
    HMODULE h = nullptr;
    for (const char* name : { "libvcam_capi.dll", "vcam_capi.dll" }) {
        std::string p = base + "\\" + name;
        h = LoadLibraryA(p.c_str());
        if (h) {
            break;
        }
    }
    if (!h) {
        std::cerr << "[RcqVirtualCamera] libvcam_capi.dll not found in " << base << "\n";
        m_impl.reset();
        return false;
    }
    m_impl->hlib = h;

    m_impl->p_open = (void* (*)())(void *) vcamProc(h, "vcam_open");
    m_impl->p_close = (void (*)(void*))(void *) vcamProc(h, "vcam_close");
    m_impl->p_stream_start = (int (*)(void*, const char*))(void *) vcamProc(h, "vcam_stream_start");
    m_impl->p_stream_send =
        (int (*)(void*, const char*, const char*, int, int, const char**, size_t*))(void *) vcamProc(
            h, "vcam_stream_send");
    m_impl->p_stream_stop = (int (*)(void*, const char*))(void *) vcamProc(h, "vcam_stream_stop");
    m_impl->p_format = (int (*)(void*,
                                 const char*,
                                 int,
                                 char*,
                                 size_t*,
                                 int*,
                                 int*,
                                 int*,
                                 int*))(void *) vcamProc(h, "vcam_format");
    m_impl->p_data_mode = (int (*)(void*, char*, size_t))(void *) vcamProc(h, "vcam_data_mode");
    if (!m_impl->p_open || !m_impl->p_close || !m_impl->p_stream_start || !m_impl->p_stream_send
        || !m_impl->p_stream_stop || !m_impl->p_format) {
        std::cerr << "[RcqVirtualCamera] missing vcam exports\n";
        unload();
        return false;
    }
    return true;
#else
    std::cerr << "[RcqVirtualCamera] only implemented on Windows\n";
    return false;
#endif
}

void RcqVirtualCamera::unload()
{
    stopStream();
    if (m_impl) {
#ifdef _WIN32
        if (m_impl->hlib) {
            FreeLibrary(m_impl->hlib);
        }
#endif
        m_impl.reset();
    }
}

bool RcqVirtualCamera::startStream(const char* deviceId)
{
    if (!m_impl || m_streaming) {
        return m_streaming;
    }

    m_deviceId = deviceId ? deviceId : kDefaultDeviceId;
    m_impl->vcam = m_impl->p_open();
    if (!m_impl->vcam) {
        std::cerr << "[RcqVirtualCamera] vcam_open failed (is AkVCamAssistant running?)\n";
        return false;
    }

    int w = 0;
    int h = 0;
    const int nfmt = m_impl->p_format(
        m_impl->vcam, m_deviceId.c_str(), 0, nullptr, nullptr, &w, &h, nullptr, nullptr);
    if (nfmt > 0 && w > 0 && h > 0) {
        m_frameW = w;
        m_frameH = h;
    }

    if (m_impl->p_stream_start(m_impl->vcam, m_deviceId.c_str()) != 0) {
        std::cerr << "[RcqVirtualCamera] vcam_stream_start failed. Register: " << m_deviceId
                  << " and AkVCamManager update (admin)\n";
        m_impl->p_close(m_impl->vcam);
        m_impl->vcam = nullptr;
        return false;
    }

    if (m_impl->p_data_mode) {
        char mode[32] = {};
        if (m_impl->p_data_mode(m_impl->vcam, mode, sizeof(mode)) == 0) {
            std::cerr << "[RcqVirtualCamera] AkVCam data mode: " << mode << "\n";
            if (std::strcmp(mode, "sockets") == 0) {
                std::cerr
                    << "[RcqVirtualCamera] WARNING: data mode is \"sockets\"; frames are not in "
                       "shared memory, so Zoom/MF may show no video. Run as admin: AkVCamManager "
                       "set-data-mode mmap (see register_rcq_camera.bat)\n";
            }
        }
    }

    m_streaming = true;
    return true;
}

void RcqVirtualCamera::stopStream()
{
    if (!m_impl || !m_streaming) {
        return;
    }
    m_impl->p_stream_stop(m_impl->vcam, m_deviceId.c_str());
    m_streaming = false;
    if (m_impl->vcam) {
        m_impl->p_close(m_impl->vcam);
        m_impl->vcam = nullptr;
    }
}

void RcqVirtualCamera::pushFrameFromWidget(QWidget* viewport)
{
    if (!m_impl || !m_streaming || !m_impl->vcam || !viewport) {
        return;
    }

    QImage im;

    if (qgetenv("RCQ_VCAM_TEST_BARS") == "1") {
        static int patternPhase = 0;
        im = makeTestPatternRgb(m_frameW, m_frameH, (patternPhase++ * 3) % 256);
    } else {
        const QPixmap cap = captureViewportForDriver(viewport);
        if (cap.isNull() || cap.width() < 2) {
            return;
        }
        im = cap.toImage();
        if (im.format() != QImage::Format_RGB888) {
            im = im.convertToFormat(QImage::Format_RGB888);
        }
        if (im.isNull()) {
            return;
        }
    }

    QImage scaled = im.scaled(m_frameW, m_frameH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (scaled.format() != QImage::Format_RGB888) {
        scaled = scaled.convertToFormat(QImage::Format_RGB888);
    }
    if (scaled.isNull() || scaled.width() != m_frameW || scaled.height() != m_frameH) {
        return;
    }

    const int bpl = static_cast<int>(scaled.bytesPerLine());
    if (bpl < m_frameW * 3) {
        return;
    }

    const char* planes[1] = { reinterpret_cast<const char*>(scaled.constBits()) };
    size_t lineSizes[1] = { static_cast<size_t>(bpl) };

    m_impl->p_stream_send(
        m_impl->vcam,
        m_deviceId.c_str(),
        "RGB24",
        m_frameW,
        m_frameH,
        const_cast<const char**>(planes),
        lineSizes);
}
