#define MA_NO_JACK
// MINIAUDIO_IMPLEMENTATION должна быть определена ровно в одном трансляционном модуле
#ifndef MINIAUDIO_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#endif

#include "devices.h"
#include <codecvt>
#include <chrono>


// === Device Base Implementation ===

void Device::setDataCallback(DataCallback cb) {
    dataCallback_ = std::move(cb);
}

bool Device::sendCommand(const std::vector<uint8_t>& /*data*/) {
    return false;
}

void Device::emitData(const void* data, size_t size) {
    if (dataCallback_ && data && size > 0) {
        dataCallback_(std::vector<uint8_t>(static_cast<const uint8_t*>(data),
                                           static_cast<const uint8_t*>(data) + size));
    }
}

// === MiniAudioContext Implementation ===

MiniAudioContext::MiniAudioContext() {
    ma_context_config config = ma_context_config_init();
    ma_result result = ma_context_init(nullptr, 0, &config, &ctx_);
    if (result != MA_SUCCESS)
        throw std::runtime_error("ma_context_init failed");
}

MiniAudioContext::~MiniAudioContext() {
    ma_context_uninit(&ctx_);
}

ma_context* MiniAudioContext::get() noexcept {
    return &ctx_;
}

// === VideoDevice Implementation ===

VideoDevice::VideoDevice(int index)
    : index_(index), id_("video:" + std::to_string(index)) {
    if (index < 0)
        throw std::invalid_argument("Video device index must be non-negative");
}

VideoDevice::~VideoDevice() {
    close();
}

// допилить создание камер не через индекс

bool VideoDevice::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isOpen_) return true;

    cap_.open(index_, cv::CAP_ANY);
    if (!cap_.isOpened()) {
        std::cerr << "Failed to open video device " << index_ << std::endl;
        return false;
    }

    cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
    isOpen_ = true;

    captureThread_ = std::thread(&VideoDevice::captureLoop, this);
    return true;
}

void VideoDevice::close() {
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

bool VideoDevice::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isOpen_;
}

std::string VideoDevice::id() const {
    return id_;
}

void VideoDevice::captureLoop() {
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

        std::vector<uint8_t> buffer;
        if (!cv::imencode(".jpg", frame, buffer)) {
            std::cerr << "Failed to encode frame from device " << index_ << std::endl;
            continue;
        }

        emitData(buffer.data(), buffer.size());
    }
}

// === AudioDevice Implementation ===

AudioDevice::AudioDevice(std::shared_ptr<MiniAudioContext> ctx, Params params)
    : ctx_(std::move(ctx)), params_(std::move(params)),
    id_("audio:" + params_.name) {
    if (params_.channels == 0 || params_.sampleRate == 0)
        throw std::invalid_argument("Invalid audio parameters");
}

AudioDevice::~AudioDevice() {
    close();
}

bool AudioDevice::open() {
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

void AudioDevice::close() {
    std::lock_guard lock(mutex_);
    if (!isOpen_) return;
    ma_device_stop(&device_);
    ma_device_uninit(&device_);
    isOpen_ = false;
}

bool AudioDevice::isOpen() const {
    std::lock_guard lock(mutex_);
    return isOpen_;
}

std::string AudioDevice::id() const {
    return id_;
}

void AudioDevice::dataCallback(ma_device* device, void* /*output*/, const void* input, ma_uint32 frameCount) {
    if (!device || !device->pUserData || !input) return;

    auto* self = static_cast<AudioDevice*>(device->pUserData);
    const size_t bytesPerFrame = ma_get_bytes_per_frame(device->capture.format, device->capture.channels);
    const size_t totalBytes = frameCount * bytesPerFrame;

    self->emitData(input, totalBytes);
}

// === HidHandle Implementation ===

HidHandle::HidHandle(hid_device* d) noexcept : dev_(d) {}

HidHandle::HidHandle(HidHandle&& other) noexcept : dev_(other.release()) {}

HidHandle& HidHandle::operator=(HidHandle&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

HidHandle::~HidHandle() {
    reset();
}

void HidHandle::reset(hid_device* d) noexcept {
    if (dev_) hid_close(dev_);
    dev_ = d;
}

hid_device* HidHandle::release() noexcept {
    hid_device* tmp = dev_;
    dev_ = nullptr;
    return tmp;
}

hid_device* HidHandle::get() const noexcept {
    return dev_;
}

HidHandle::operator bool() const noexcept {
    return dev_ != nullptr;
}

// === HidDevice Implementation ===

HidDevice::HidDevice(unsigned short vid, unsigned short pid)
    : vid_(vid), pid_(pid) {
    struct hid_device_info* devices = hid_enumerate(vid, pid);
    if (devices) {
        // Примечание: std::wstring_convert устарел в C++17.
        // Для строгого соответствия современным стандартам лучше использовать альтернативы.
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            if (devices->serial_number && devices->serial_number[0]) {
                std::string serial = conv.to_bytes(devices->serial_number);
                id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid) + ":" + serial;
            } else if (devices->path) {
                std::string path = devices->path;
                id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid) + ":path:" + path;
            }
        } catch (...) {
            // Fallback если конвертация失败
        }
        hid_free_enumeration(devices);
    }
    if (id_.empty()) {
        id_ = "hid:" + std::to_string(vid) + ":" + std::to_string(pid);
    }
}

HidDevice::~HidDevice() {
    close();
}

bool HidDevice::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isOpen_) return true;

    hid_device* dev = hid_open(vid_, pid_, nullptr);
    if (!dev) return false;

    handle_.reset(dev);
    hid_set_nonblocking(handle_.get(), 1);

    isOpen_ = true;
    readThread_ = std::thread(&HidDevice::readLoop, this);
    return true;
}

void HidDevice::close() {
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

bool HidDevice::isOpen() const {
    return isOpen_.load(std::memory_order_acquire);
}

std::string HidDevice::id() const {
    return id_;
}

bool HidDevice::sendCommand(const std::vector<uint8_t>& data) {
    if (!isOpen()) return false;
    int res = hid_write(handle_.get(), data.data(), data.size());
    return res >= 0;
}

void HidDevice::readLoop() {
    std::vector<unsigned char> buffer(4096);
    while (isOpen_.load(std::memory_order_acquire)) {
        int res = hid_read(handle_.get(), buffer.data(), buffer.size());
        if (res < 0) {
            break;
        }
        if (res == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        emitData(buffer.data(), static_cast<size_t>(res));
    }
}
