#include "obsvirtualcamera.h"

#include <QApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QWidget>
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#    include <shlobj.h>
#    include <wingdi.h>
#    include <windows.h>
#    ifndef PW_RENDERFULLCONTENT
#        define PW_RENDERFULLCONTENT 0x00000002
#    endif
#    ifndef PW_CLIENTONLY
#        define PW_CLIENTONLY 0x00000001
#    endif
#endif

// ---------------------------------------------------------------------------
// OBS virtual camera shared-memory layout (matches OBS shared-memory-queue.c)
// ---------------------------------------------------------------------------

#define OBS_VIDEO_NAME L"OBSVirtualCamVideo"
#define FRAME_HEADER_SIZE 32u
#define ALIGN32(n) (((n) + 31u) & ~31u)

enum obs_queue_state : uint32_t {
    STATE_INVALID  = 0,
    STATE_STARTING = 1,
    STATE_READY    = 2,
    STATE_STOPPING = 3,
};

struct obs_queue_header {
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile uint32_t state;

    uint32_t offsets[3];
    uint32_t type;

    uint32_t cx;
    uint32_t cy;
    uint64_t interval;   // 100ns units

    uint32_t reserved[8];
};

// ---------------------------------------------------------------------------

struct ObsVirtualCamera::Impl {
#ifdef _WIN32
    HANDLE hmap = nullptr;
#endif
    obs_queue_header* header     = nullptr;
    uint64_t*         ts[3]      = {};
    uint8_t*          frame[3]   = {};

    uint32_t                       cx         = 0;
    uint32_t                       cy         = 0;
    uint64_t                       interval   = 0;
    int minCapMs = 50;   // RCQ_OBS_CAP_MIN_MS; 0 = off
    std::chrono::steady_clock::time_point lastCap{std::chrono::steady_clock::time_point::min()};
};

ObsVirtualCamera::ObsVirtualCamera() = default;


ObsVirtualCamera::~ObsVirtualCamera()
{
    stopStream();
}

bool ObsVirtualCamera::isStreaming() const { return m_streaming; }

// ---------------------------------------------------------------------------
// Forward declarations for static helpers used by workerLoop
// ---------------------------------------------------------------------------
static QImage captureViewportRgb888(QWidget* vp, bool mainThreadOnly = true);
static void rgb888_to_nv12(const uint8_t* rgb, int src_stride,
                            uint8_t* dst_y, uint8_t* dst_uv,
                            uint32_t cx, uint32_t cy);

// ---------------------------------------------------------------------------
// Background worker loop
// ---------------------------------------------------------------------------

void ObsVirtualCamera::workerLoop()
{
    while (true) {
        QWidget* vp = nullptr;
        {
            std::unique_lock<std::mutex> lk(m_captureMutex);
            m_captureCV.wait(lk, [this] {
                return m_capturePending.load() || m_captureStop.load();
            });
            if (m_captureStop.load()) break;
            vp = m_captureViewport;
            m_capturePending.store(false);
        }

        if (!m_impl || !m_streaming || !vp) continue;

        const uint32_t cx = m_impl->cx;
        const uint32_t cy = m_impl->cy;

        const bool isTest = (qgetenv("RCQ_VCAM_TEST_BARS") == "1");
        if (!isTest && m_impl->minCapMs > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (m_impl->lastCap != std::chrono::steady_clock::time_point::min()
                && (now - m_impl->lastCap) < std::chrono::milliseconds(m_impl->minCapMs)) {
                continue;
            }
        }

        QImage im;
        if (isTest) {
            static int phase = 0;
            im = QImage(static_cast<int>(cx), static_cast<int>(cy), QImage::Format_RGB888);
            for (uint32_t y = 0; y < cy; ++y) {
                uint8_t* line = im.scanLine(static_cast<int>(y));
                for (uint32_t x = 0; x < cx; ++x) {
                    line[3 * x + 0] = static_cast<uint8_t>((x * 255 / (cx - 1) + phase) % 256);
                    line[3 * x + 1] = static_cast<uint8_t>((y * 255 / (cy - 1)) % 256);
                    line[3 * x + 2] = static_cast<uint8_t>((phase * 2) % 256);
                }
            }
            phase = (phase + 3) % 256;
        } else {
            QImage raw = captureViewportRgb888(vp, /*mainThreadOnly=*/false);
            if (raw.isNull() || raw.width() < 2) continue;
            im = raw.scaled(static_cast<int>(cx), static_cast<int>(cy),
                            Qt::IgnoreAspectRatio, Qt::FastTransformation);
            if (im.format() != QImage::Format_RGB888)
                im = im.convertToFormat(QImage::Format_RGB888);
            if (im.isNull()) continue;
        }

        auto* hdr  = m_impl->header;
        const uint32_t inc = hdr->write_idx + 1U;
        hdr->write_idx     = inc;
        const unsigned idx = static_cast<unsigned>(inc) % 3u;

        uint8_t* dst    = m_impl->frame[idx];
        uint8_t* dst_y  = dst;
        uint8_t* dst_uv = dst + cx * cy;

        rgb888_to_nv12(im.constBits(), static_cast<int>(im.bytesPerLine()),
                       dst_y, dst_uv, cx, cy);

        *m_impl->ts[idx] = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 10000ULL;
        hdr->read_idx = inc;
        hdr->state    = STATE_READY;

        if (!isTest)
            m_impl->lastCap = std::chrono::steady_clock::now();
    }
}

// ---------------------------------------------------------------------------
// Capture helpers
// ---------------------------------------------------------------------------

static int obsCaptureMinIntervalMs()
{
    const QByteArray s = qgetenv("RCQ_OBS_CAP_MIN_MS");
    if (s.isEmpty()) {
        return 50;   // ~20 fps max; 60x/sec grabWindow / BitBlt causes system cursor flicker
    }
    bool ok  = false;
    const int v = s.toInt(&ok);
    return (ok && v >= 0) ? v : 50;   // 0 = no throttle
}

static QImage captureViewportRgb888(QWidget* vp, bool mainThreadOnly)
{
    if (!vp) {
        return {};
    }
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(vp->winId());
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return {};
    }

    RECT rc {};
    if (!GetClientRect(hwnd, &rc)) {
        return {};
    }
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w < 2 || h < 2) {
        return {};
    }

    HDC     wdc  = GetDC(hwnd);
    HDC     mdc  = nullptr;
    HBITMAP hbmp = nullptr;
    if (!wdc) {
        return {};
    }
    mdc = CreateCompatibleDC(wdc);
    if (!mdc) {
        ReleaseDC(hwnd, wdc);
        return {};
    }
    hbmp = CreateCompatibleBitmap(wdc, w, h);
    if (!hbmp) {
        DeleteDC(mdc);
        ReleaseDC(hwnd, wdc);
        return {};
    }
    HGDIOBJ old = SelectObject(mdc, hbmp);

    if (!::PrintWindow(hwnd, mdc, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
        if (!::PrintWindow(hwnd, mdc, 0)) {
            (void) ::BitBlt(mdc, 0, 0, w, h, wdc, 0, 0, SRCCOPY);
        }
    }

    QImage   img(w, h, QImage::Format_ARGB32);
    BITMAPINFO bi {};
    bi.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth        = w;
    bi.bmiHeader.biHeight       = -h;   // top-down
    bi.bmiHeader.biPlanes       = 1;
    bi.bmiHeader.biBitCount     = 32;
    bi.bmiHeader.biCompression  = BI_RGB;
    if (::GetDIBits(mdc, hbmp, 0, h, img.bits(), &bi, DIB_RGB_COLORS) == 0) {
        SelectObject(mdc, old);
        DeleteObject(hbmp);
        DeleteDC(mdc);
        ReleaseDC(hwnd, wdc);
    } else {
        SelectObject(mdc, old);
        DeleteObject(hbmp);
        DeleteDC(mdc);
        ReleaseDC(hwnd, wdc);
        return img.convertToFormat(QImage::Format_RGB888);
    }
#endif

    // Qt fallbacks: only safe to call from the main (GUI) thread.
    if (!mainThreadOnly)
        return {};

    {
        const QPixmap g = vp->grab();
        if (!g.isNull() && g.width() >= 2) {
            QImage raw = g.toImage();
            if (raw.format() != QImage::Format_RGB888) {
                raw = raw.convertToFormat(QImage::Format_RGB888);
            }
            if (!raw.isNull()) {
                return raw;
            }
        }
    }
    const WId     win   = vp->winId();
    QScreen*      sc    = vp->screen();
    if (!sc) {
        sc = QGuiApplication::screenAt(vp->mapToGlobal(QPoint(vp->width() / 2, vp->height() / 2)));
    }
    if (!sc) {
        sc = QGuiApplication::primaryScreen();
    }
    if (sc) {
        const QPixmap pm = sc->grabWindow(win, 0, 0, -1, -1);
        if (!pm.isNull() && pm.width() >= 2) {
            QImage raw = pm.toImage();
            if (raw.format() != QImage::Format_RGB888) {
                raw = raw.convertToFormat(QImage::Format_RGB888);
            }
            if (!raw.isNull()) {
                return raw;
            }
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// RGB888 → NV12  (BT.601 limited range)
// ---------------------------------------------------------------------------

static inline uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

// Writes NV12 into dst buffer:
//   Y plane  : cx * cy bytes
//   UV plane : cx * (cy/2) bytes  (interleaved U0,V0,U1,V1,...)
static void rgb888_to_nv12(const uint8_t* rgb,   // RGB888 row-major
                            int           src_stride,
                            uint8_t*      dst_y,
                            uint8_t*      dst_uv,
                            uint32_t      cx,
                            uint32_t      cy)
{
    for (uint32_t y = 0; y < cy; ++y) {
        const uint8_t* row = rgb + y * static_cast<size_t>(src_stride);
        uint8_t* out_y = dst_y + y * cx;

        for (uint32_t x = 0; x < cx; ++x) {
            int r = row[3 * x + 0];
            int g = row[3 * x + 1];
            int b = row[3 * x + 2];
            out_y[x] = clamp8(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
    }

    // UV: one pair per 2×2 block
    uint32_t uv_rows = cy / 2;
    uint32_t uv_cols = cx / 2;
    for (uint32_t uy = 0; uy < uv_rows; ++uy) {
        const uint8_t* row0 = rgb + (uy * 2)     * static_cast<size_t>(src_stride);
        const uint8_t* row1 = rgb + (uy * 2 + 1) * static_cast<size_t>(src_stride);
        uint8_t* out_uv = dst_uv + uy * cx;

        for (uint32_t ux = 0; ux < uv_cols; ++ux) {
            // Average R,G,B over 2×2 block
            int r = (row0[6 * ux + 0] + row0[6 * ux + 3] + row1[6 * ux + 0] + row1[6 * ux + 3]) >> 2;
            int g = (row0[6 * ux + 1] + row0[6 * ux + 4] + row1[6 * ux + 1] + row1[6 * ux + 4]) >> 2;
            int b = (row0[6 * ux + 2] + row0[6 * ux + 5] + row1[6 * ux + 2] + row1[6 * ux + 5]) >> 2;
            out_uv[2 * ux + 0] = clamp8(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128); // U
            out_uv[2 * ux + 1] = clamp8(((112 * r - 94 * g -  18 * b + 128) >> 8) + 128); // V
        }
    }
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------

bool ObsVirtualCamera::startStream(uint32_t cx, uint32_t cy, uint32_t fps)
{
    if (m_streaming)
        return true;

#ifdef _WIN32
    if (cx < 2 || cy < 2 || fps == 0)
        return false;

    uint64_t interval = 10000000ULL / fps;  // 100ns units

    // Write obs-virtualcam.txt so the DS filter initialises with the right format.
    // Path: %APPDATA%\obs-virtualcam.txt
    wchar_t appdata[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata);
    wchar_t txtPath[MAX_PATH] = {};
    wcscpy_s(txtPath, appdata);
    wcscat_s(txtPath, L"\\obs-virtualcam.txt");

    {
        HANDLE fh = CreateFileW(txtPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fh != INVALID_HANDLE_VALUE) {
            char buf[64];
            int len = snprintf(buf, sizeof(buf), "%" PRIu32 "x%" PRIu32 "x%" PRIu64,
                               cx, cy, static_cast<unsigned long long>(interval));
            DWORD written;
            WriteFile(fh, buf, static_cast<DWORD>(len), &written, nullptr);
            CloseHandle(fh);
        } else {
            std::cerr << "[ObsVCam] WARNING: could not write obs-virtualcam.txt\n";
        }
    }

    // Calculate shared memory size (same formula as OBS)
    uint32_t frame_size = cx * cy * 3 / 2;  // NV12
    uint32_t offset_frame[3];

    uint32_t size = ALIGN32(static_cast<uint32_t>(sizeof(obs_queue_header)));
    offset_frame[0] = size;
    size = ALIGN32(size + frame_size + FRAME_HEADER_SIZE);
    offset_frame[1] = size;
    size = ALIGN32(size + frame_size + FRAME_HEADER_SIZE);
    offset_frame[2] = size;
    size = ALIGN32(size + frame_size + FRAME_HEADER_SIZE);

    // Fail gracefully if already in use (OBS is running and streaming)
    {
        HANDLE probe = OpenFileMappingW(FILE_MAP_READ, FALSE, OBS_VIDEO_NAME);
        if (probe) {
            CloseHandle(probe);
            std::cerr << "[ObsVCam] OBSVirtualCamVideo already in use (is OBS streaming?)\n";
            return false;
        }
    }

    HANDLE hmap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                     PAGE_READWRITE, 0, size, OBS_VIDEO_NAME);
    if (!hmap) {
        std::cerr << "[ObsVCam] CreateFileMapping failed: " << GetLastError() << "\n";
        return false;
    }

    auto* hdr = reinterpret_cast<obs_queue_header*>(
        MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!hdr) {
        std::cerr << "[ObsVCam] MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(hmap);
        return false;
    }

    memset(hdr, 0, size);
    hdr->cx       = cx;
    hdr->cy       = cy;
    hdr->interval = interval;
    hdr->state    = STATE_STARTING;
    for (int i = 0; i < 3; ++i)
        hdr->offsets[i] = offset_frame[i];

    m_impl = std::make_unique<Impl>();
    m_impl->hmap     = hmap;
    m_impl->header   = hdr;
    m_impl->cx       = cx;
    m_impl->cy       = cy;
    m_impl->interval = interval;
    for (int i = 0; i < 3; ++i) {
        auto* base = reinterpret_cast<uint8_t*>(hdr) + offset_frame[i];
        m_impl->ts[i]    = reinterpret_cast<uint64_t*>(base);
        m_impl->frame[i] = base + FRAME_HEADER_SIZE;
    }

    m_impl->minCapMs  = obsCaptureMinIntervalMs();
    m_impl->lastCap   = std::chrono::steady_clock::time_point::min();
    m_streaming = true;

    // Start background capture thread
    m_captureStop.store(false);
    m_capturePending.store(false);
    m_captureThread = std::thread(&ObsVirtualCamera::workerLoop, this);

    std::cerr << "[ObsVCam] started " << cx << "x" << cy << "@" << fps << "fps, interval=" << interval
              << " (capture min " << m_impl->minCapMs
              << " ms, RCQ_OBS_CAP_MIN_MS=0 to disable)\n";
    return true;

#else
    (void)cx; (void)cy; (void)fps;
    std::cerr << "[ObsVCam] only implemented on Windows\n";
    return false;
#endif
}

void ObsVirtualCamera::stopStream()
{
    if (!m_impl && !m_captureThread.joinable())
        return;

    // Stop background thread first
    {
        std::lock_guard<std::mutex> lk(m_captureMutex);
        m_captureStop.store(true);
    }
    m_captureCV.notify_one();
    if (m_captureThread.joinable())
        m_captureThread.join();

    if (!m_impl) return;

#ifdef _WIN32
    if (m_impl->header)
        m_impl->header->state = STATE_STOPPING;

    if (m_impl->header)
        UnmapViewOfFile(m_impl->header);
    if (m_impl->hmap)
        CloseHandle(m_impl->hmap);
#endif

    m_impl.reset();
    m_streaming = false;
}

// ---------------------------------------------------------------------------
// pushFrameFromWidget
// ---------------------------------------------------------------------------

// Non-blocking: wake up the background thread to do the actual capture.
void ObsVirtualCamera::pushFrameFromWidget(QWidget* viewport)
{
    if (!m_impl || !m_streaming || !viewport)
        return;
    // Skip if previous frame is still being processed (drop frame, don't block).
    if (m_capturePending.load())
        return;
    {
        std::lock_guard<std::mutex> lk(m_captureMutex);
        m_captureViewport = viewport;
        m_capturePending.store(true);
    }
    m_captureCV.notify_one();
}
