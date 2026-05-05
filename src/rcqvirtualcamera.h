#ifndef RCQVIRTUALCAMERA_H
#define RCQVIRTUALCAMERA_H

#include <memory>
#include <string>

#include <functional>

class QWidget;

/**
 * Pushes RGB24 frames to akvirtualcamera via vcam C API (libvcam_capi.dll).
 * Register a device with id and description first (e.g. AkVCamManager), keep
 * AkVCamAssistant running, place libvcam_capi.dll next to M3.exe.
 *
 * Capture: WA_PaintOnScreen + bgfx — QWidget::grab() is often empty; we use
 * QScreen::grabWindow(winId()) first. Env RCQ_VCAM_TEST_BARS=1 forces a color
 * test pattern to verify the vcam pipeline (bypasses grab).
 */
class RcqVirtualCamera {
public:
    RcqVirtualCamera();
    ~RcqVirtualCamera();
    RcqVirtualCamera(const RcqVirtualCamera&) = delete;
    RcqVirtualCamera& operator=(const RcqVirtualCamera&) = delete;
    RcqVirtualCamera(RcqVirtualCamera&&) noexcept;
    RcqVirtualCamera& operator=(RcqVirtualCamera&&) noexcept;

    /** Tries to load the DLL. Returns true if vcam entry points are ready. */
    bool loadFromApplicationDir();
    void unload();

    bool isReady() const { return m_impl != nullptr; }
    bool isStreaming() const { return m_streaming; }

    /**
     * vcam_open + vcam_stream_start. deviceId must already exist in driver config
     * (see scripts/register_rcq_camera.bat or AkVCamManager add-device + update).
     */
    bool startStream(const char* deviceId = kDefaultDeviceId);
    void stopStream();

    /** Grabs the widget, converts to RGB24, scales to the virtual camera format, vcam_stream_send. */
    void pushFrameFromWidget(QWidget* viewport);

    static constexpr const char* kDefaultDeviceId = "RcqVirtualDevice";

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_streaming = false;
    int m_frameW = 640;
    int m_frameH = 480;
    std::string m_deviceId;
};

#endif
