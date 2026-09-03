#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

class QWidget;
class IDataBus;

class ObsVirtualCamera {
public:
    ObsVirtualCamera();
    ObsVirtualCamera(const ObsVirtualCamera&) = delete;
    ObsVirtualCamera& operator=(const ObsVirtualCamera&) = delete;
    ~ObsVirtualCamera();

    // Start pushing to OBS Virtual Camera shared memory.
    // cx/cy: frame size that Zoom will see. fps: target rate.
    // Also writes %APPDATA%\obs-virtualcam.txt so the DS filter knows the format.
    // Call BEFORE opening the camera in Zoom (the filter reads the txt at init time).
    bool startStream(uint32_t cx = 640, uint32_t cy = 480, uint32_t fps = 30);

    void stopStream();

    /// Preferred path: latest GPU frame from engine `frames_buffer` (same bus as Spout).
    void pushFrameFromBus(IDataBus* bus);

    /// Fallback: HWND / Qt grab (slow). Env RCQ_OBS_CAP_MIN_MS throttles this path.
    void pushFrameFromWidget(QWidget* viewport);

    bool isStreaming() const;

private:
    void workerLoop();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_streaming = false;

    // Async capture thread
    std::thread             m_captureThread;
    std::mutex              m_captureMutex;
    std::condition_variable m_captureCV;
    std::atomic<bool>       m_captureStop{false};
    std::atomic<bool>       m_capturePending{false};
    std::atomic<void*>      m_pendingTexture{nullptr}; // ID3D11Texture2D*, latest-wins
    QWidget*                m_captureViewport{nullptr}; // written under m_captureMutex
};
