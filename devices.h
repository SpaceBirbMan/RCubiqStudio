#ifndef DEVICES_H
#define DEVICES_H

#include <codecvt>
#include <thread>
#define MA_NO_JACK
// MINIAUDIO_IMPLEMENTATION должен быть только в одном .cpp файле

#include <opencv2/opencv.hpp>
#include "miniaudio.h"
#include <hidapi/hidapi.h>
#include <libusb-1.0/libusb.h>
#include <libserialport.h>
#include <string>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <memory>

class MiniAudioContext;

class Device {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

    virtual ~Device() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual std::string id() const = 0;

    virtual void setDataCallback(DataCallback cb) { dataCallback_ = std::move(cb); }

    virtual bool sendCommand(const std::vector<uint8_t>& /*data*/) { return false; }

protected:
    void emitData(const void* data, size_t size) {
        if (dataCallback_ && data && size > 0) {
            dataCallback_(std::vector<uint8_t>(static_cast<const uint8_t*>(data),
                                               static_cast<const uint8_t*>(data) + size));
        }
    }

private:
    DataCallback dataCallback_;
};

class MiniAudioContext {
    ma_context ctx_{};
public:
    MiniAudioContext() {
        ma_context_config config = ma_context_config_init();
        ma_result result = ma_context_init(nullptr, 0, &config, &ctx_);
        if (result != MA_SUCCESS)
            throw std::runtime_error("ma_context_init failed");
    }
    ~MiniAudioContext() {
        ma_context_uninit(&ctx_);
    }
    ma_context* get() noexcept { return &ctx_; }
    MiniAudioContext(const MiniAudioContext&) = delete;
    MiniAudioContext& operator=(const MiniAudioContext&) = delete;
};

// === Devices ===

class VideoDevice : public Device {
public:
    explicit VideoDevice(int index)
        : index_(index), id_("video:" + std::to_string(index)) {
        if (index < 0)
            throw std::invalid_argument("Video device index must be non-negative");
    }

    ~VideoDevice() override { close(); }

    bool open() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isOpen_) return true;

        cap_.open(index_, cv::CAP_ANY);
        if (!cap_.isOpened()) {
            std::cerr << "Failed to open video device " << index_ << std::endl;
            return false;
        }

        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
        isOpen_ = true;

        // Запуск потока захвата
        captureThread_ = std::thread(&VideoDevice::captureLoop, this);
        return true;
    }

    void close() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!isOpen_) return;
            isOpen_ = false;
        }

        if (captureThread_.joinable()) {
            captureThread_.join();
        }

        cap_.release();
    }

    bool isOpen() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return isOpen_;
    }

    std::string id() const override { return id_; }

private:
    void captureLoop() {
        cv::Mat frame;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!isOpen_) break;
            }

            if (!cap_.read(frame) || frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Преобразуем в JPEG (альтернатива: raw BGR, H.264 и т.д.)
            std::vector<uint8_t> buffer;
            if (!cv::imencode(".jpg", frame, buffer)) {
                std::cerr << "Failed to encode frame from device " << index_ << std::endl;
                continue;
            }

            emitData(buffer.data(), buffer.size());
        }
    }

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

    AudioDevice(std::shared_ptr<MiniAudioContext> ctx, Params params)
        : ctx_(std::move(ctx)), params_(std::move(params)),
        id_("audio:" + params_.name) {
        if (params_.channels == 0 || params_.sampleRate == 0)
            throw std::invalid_argument("Invalid audio parameters");
    }

    ~AudioDevice() override { close(); }

    bool open() override {
        std::lock_guard lock(mutex_);
        if (isOpen_) return true;

        config_ = ma_device_config_init(ma_device_type_capture);
        config_.capture.format   = params_.format;
        config_.capture.channels = params_.channels;
        config_.sampleRate       = params_.sampleRate;
        config_.dataCallback     = dataCallback;
        config_.pUserData        = this;

        ma_result result = ma_device_init(ctx_->get(), &config_, &device_);
        if (result != MA_SUCCESS) return false;

        result = ma_device_start(&device_);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&device_);
            return false;
        }

        isOpen_ = true;
        return true;
    }

    void close() override {
        std::lock_guard lock(mutex_);
        if (!isOpen_) return;
        ma_device_stop(&device_);
        ma_device_uninit(&device_);
        isOpen_ = false;
    }

    bool isOpen() const override {
        std::lock_guard lock(mutex_);
        return isOpen_;
    }

    std::string id() const override { return id_; }

private:
    static void dataCallback(ma_device* device, void* /*output*/, const void* input, ma_uint32 frameCount) {
        if (!device || !device->pUserData || !input) return;

        auto* self = static_cast<AudioDevice*>(device->pUserData);
        const size_t bytesPerFrame = ma_get_bytes_per_frame(device->capture.format, device->capture.channels);
        const size_t totalBytes = frameCount * bytesPerFrame;

        self->emitData(input, totalBytes);
    }

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
    explicit HidHandle(hid_device* d) noexcept : dev_(d) {}
    HidHandle(const HidHandle&) = delete;
    HidHandle& operator=(const HidHandle&) = delete;

    HidHandle(HidHandle&& other) noexcept : dev_(other.release()) {}
    HidHandle& operator=(HidHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~HidHandle() { reset(); }

    void reset(hid_device* d = nullptr) noexcept {
        if (dev_) hid_close(dev_);
        dev_ = d;
    }

    hid_device* release() noexcept {
        hid_device* tmp = dev_;
        dev_ = nullptr;
        return tmp;
    }

    hid_device* get() const noexcept { return dev_; }
    explicit operator bool() const noexcept { return dev_ != nullptr; }
};

class HidDevice final : public Device {
    const unsigned short vid_, pid_;
    std::string id_;
    HidHandle handle_;
    mutable std::mutex mutex_;
    std::atomic<bool> isOpen_{false};
    std::thread readThread_;

public:
    HidDevice(unsigned short vid, unsigned short pid)
        : vid_(vid), pid_(pid) {
        // Формируем ID без открытия устройства
        struct hid_device_info* devices = hid_enumerate(vid, pid);
        if (devices) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            if (devices->serial_number && devices->serial_number[0]) {
                std::string serial = conv.to_bytes(devices->serial_number);
                id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid) + ":" + serial;
            } else if (devices->path) {
                std::string path = devices->path; // или напрямую devices->path, если char*
                id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid) + ":path:" + path;
            }
            hid_free_enumeration(devices);
        }
        if (id_.empty()) {
            id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid);
        }
    }

    ~HidDevice() override {
        close();
    }

    bool open() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isOpen_) return true;

        hid_device* dev = hid_open(vid_, pid_, nullptr);
        if (!dev) return false;

        handle_.reset(dev);

        // Устанавливаем неблокирующий режим для возможности прерывания
        hid_set_nonblocking(handle_.get(), 1);

        isOpen_ = true;
        readThread_ = std::thread(&HidDevice::readLoop, this);
        return true;
    }

    void close() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!isOpen_) return;
            isOpen_ = false;
        }

        if (readThread_.joinable()) {
            readThread_.join();
        }

        handle_.reset();
    }

    bool isOpen() const override {
        return isOpen_.load(std::memory_order_acquire);
    }

    std::string id() const override {
        return id_;
    }

    bool sendCommand(const std::vector<uint8_t>& data) override {
        if (!isOpen()) return false;
        int res = hid_write(handle_.get(), data.data(), data.size());
        return res >= 0;
    }

private:
    void readLoop() {
        // HID-пакеты обычно до 4096 байт, но чаще 64–512
        std::vector<unsigned char> buffer(4096);
        while (isOpen_.load(std::memory_order_acquire)) {
            int res = hid_read(handle_.get(), buffer.data(), buffer.size());
            if (res < 0) {
                // Ошибка чтения — выходим
                break;
            }
            if (res == 0) {
                // Таймаут (из-за non-blocking). Ждём немного, чтобы не грузить CPU.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // Передаём прочитанные данные вверх по стеку
            emitData(buffer.data(), static_cast<size_t>(res));
        }
    }
};

// serial и usb потом лучше сделать

#endif // DEVICES_H
