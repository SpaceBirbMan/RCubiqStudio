#ifndef DEVICES_H
#define DEVICES_H

#define NOBYTE
#define WIN32_LEAN_AND_MEAN
#include <hidapi/hidapi.h>
#include <opencv2/opencv.hpp>
#define MA_NO_JACK
#include "miniaudio.h"
#include <uiohook.h>

#include <string>
#include <mutex>
#include <memory>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>

class MiniAudioContext;

class Device {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

    virtual ~Device() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual std::string id() const = 0;

    void setDataCallback(DataCallback cb);
    virtual bool sendCommand(const std::vector<uint8_t>& data);

protected:
    void emitData(const void* data, size_t size);

private:
    DataCallback dataCallback_;
};

class MiniAudioContext {
    ma_context ctx_{};
public:
    MiniAudioContext();
    ~MiniAudioContext();
    ma_context* get() noexcept;

    MiniAudioContext(const MiniAudioContext&) = delete;
    MiniAudioContext& operator=(const MiniAudioContext&) = delete;
};

// === Devices ===

class VideoDevice : public Device {
public:
    explicit VideoDevice(int index);
    ~VideoDevice() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    std::string id() const override;

private:
    void captureLoop();

    int index_;
    std::string id_;
    mutable std::mutex mutex_;
    cv::VideoCapture cap_;
    std::atomic<bool> isOpen_{false};
    std::thread captureThread_;
};

class AudioDevice : public Device {
public:
    struct Params {
        std::string name = "default";
        ma_uint32 channels = 2;
        ma_uint32 sampleRate = 48000;
        ma_format format = ma_format_f32;
    };

    AudioDevice(std::shared_ptr<MiniAudioContext> ctx, Params params);
    ~AudioDevice() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    std::string id() const override;

private:
    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);

    std::shared_ptr<MiniAudioContext> ctx_;
    mutable std::mutex mutex_;
    Params params_;
    std::string id_;
    ma_device device_{};
    ma_device_config config_{};
    bool isOpen_ = false;
};

class HidHandle {
    hid_device* dev_ = nullptr;

public:
    HidHandle() noexcept = default;
    explicit HidHandle(hid_device* d) noexcept;
    HidHandle(const HidHandle&) = delete;
    HidHandle& operator=(const HidHandle&) = delete;

    HidHandle(HidHandle&& other) noexcept;
    HidHandle& operator=(HidHandle&& other) noexcept;

    ~HidHandle();

    void reset(hid_device* d = nullptr) noexcept;
    hid_device* release() noexcept;
    hid_device* get() const noexcept;
    explicit operator bool() const noexcept;
};

class HidDevice final : public Device {
    const unsigned short vid_, pid_;
    std::string id_;
    HidHandle handle_;
    mutable std::mutex mutex_;
    std::atomic<bool> isOpen_{false};
    std::thread readThread_;

public:
    HidDevice(unsigned short vid, unsigned short pid);
    ~HidDevice() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    std::string id() const override;
    bool sendCommand(const std::vector<uint8_t>& data) override;

private:
    void readLoop();
};

struct HidDeviceInfo {
    unsigned short vendor_id;
    unsigned short product_id;
    std::string path;
    std::string manufacturer;
    std::string product;
    std::string serial;
    int usage_page;
    int usage;
};

class OsInputDevice : public Device {
    std::atomic<bool> running_{false};
    std::thread hookThread_;
    inline static OsInputDevice* s_instance = nullptr;

    static void dispatchCallback(uiohook_event* const event) {
        auto* self = s_instance;
        if (!self || !event) return;

        if (event->type == EVENT_KEY_PRESSED || event->type == EVENT_KEY_RELEASED) {
            uint16_t modifiers = event->mask;
            uint16_t keycode = event->data.keyboard.keycode;

            std::vector<uint8_t> packet(5);
            packet[0] = static_cast<uint8_t>(event->type);
            packet[1] = static_cast<uint8_t>(keycode & 0xFF);
            packet[2] = static_cast<uint8_t>((keycode >> 8) & 0xFF);
            packet[3] = static_cast<uint8_t>(modifiers & 0xFF);
            packet[4] = static_cast<uint8_t>((modifiers >> 8) & 0xFF);

            self->emitData(packet.data(), packet.size());
        }
    }

    void hookLoop() {
        // Установка коллбэка ДО запуска хука
        hook_set_dispatch_proc(&OsInputDevice::dispatchCallback);

        // hook_run() блокирующий — работает пока не вызван hook_stop()
        hook_run();
    }

public:
    OsInputDevice() {
        s_instance = this;
    }

    ~OsInputDevice() override {
        close();
        s_instance = nullptr;
    }

    bool open() override {
        if (running_.exchange(true)) return true;

        // Запускаем поток ДО hook_run(), т.к. он блокирующий
        hookThread_ = std::thread(&OsInputDevice::hookLoop, this);
        return true;
    }

    void close() override {
        if (!running_.exchange(false)) return;

        // hook_stop() прервёт блокирующий hook_run() в другом потоке
        hook_stop();

        if (hookThread_.joinable()) {
            hookThread_.join();
        }
    }

    bool isOpen() const override { return running_.load(); }
    std::string id() const override { return "os:keyboard"; }
    bool sendCommand(const std::vector<uint8_t>&) override { return false; }
};

//OsInputDevice* OsInputDevice::s_instance = nullptr;
#endif // DEVICES_H
