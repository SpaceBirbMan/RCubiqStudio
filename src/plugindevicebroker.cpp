#include "plugindevicebroker.h"
#include "devicemanager.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <new>
#include <sstream>
#include <vector>

#include "miniaudio.h"

namespace {

class MaCaptureStream final : public IAudioCaptureStream {
    ma_device device_{};
    bool deviceInited_ = false;
    bool started_ = false;
    std::mutex mtx_;
    std::vector<float> fifo_;
    static constexpr size_t kFifoCap = 96000;

    static void dataCallback(ma_device* pDevice, void* /*pOutput*/, const void* pInput, ma_uint32 frameCount)
    {
        if (!pInput || frameCount == 0)
            return;
        auto* self = static_cast<MaCaptureStream*>(pDevice->pUserData);
        const auto* samples = static_cast<const float*>(pInput);
        const size_t n = size_t(frameCount);
        std::lock_guard<std::mutex> lk(self->mtx_);
        self->fifo_.insert(self->fifo_.end(), samples, samples + n);
        if (self->fifo_.size() > kFifoCap)
            self->fifo_.erase(self->fifo_.begin(),
                               self->fifo_.begin() + ptrdiff_t(self->fifo_.size() - kFifoCap));
    }

public:
    MaCaptureStream(ma_context* ctx, const ma_device_id* captureId)
    {
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.capture.format = ma_format_f32;
        config.capture.channels = 1;
        config.sampleRate = 48000;
        config.dataCallback = dataCallback;
        config.pUserData = this;
        if (captureId)
            config.capture.pDeviceID = captureId;

        if (ma_device_init(ctx, &config, &device_) != MA_SUCCESS)
            return;
        deviceInited_ = true;
        if (ma_device_start(&device_) != MA_SUCCESS) {
            ma_device_uninit(&device_);
            deviceInited_ = false;
            return;
        }
        started_ = true;
    }

    ~MaCaptureStream() override
    {
        if (deviceInited_)
            ma_device_uninit(&device_);
    }

    int read(float* out, int maxSamples) noexcept override
    {
        if (!out || maxSamples <= 0)
            return 0;
        std::lock_guard<std::mutex> lk(mtx_);
        const int n = int(std::min<size_t>(size_t(maxSamples), fifo_.size()));
        if (n <= 0)
            return 0;
        std::memcpy(out, fifo_.data(), sizeof(float) * size_t(n));
        fifo_.erase(fifo_.begin(), fifo_.begin() + n);
        return n;
    }

    bool isOpen() const noexcept override { return started_; }
};

} // namespace

struct PluginDeviceBrokerImpl::Impl {
    ma_context context{};
    bool ok = false;

    Impl()
    {
        if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
            return;
        ok = true;
    }

    ~Impl()
    {
        if (ok)
            ma_context_uninit(&context);
    }
};

PluginDeviceBrokerImpl::PluginDeviceBrokerImpl()
    : impl_(std::make_unique<Impl>())
{}

PluginDeviceBrokerImpl::~PluginDeviceBrokerImpl() = default;

std::vector<PluginDeviceDescriptor> PluginDeviceBrokerImpl::listAudioOnlyViaMiniAudio()
{
    std::vector<PluginDeviceDescriptor> out;
    if (!impl_ || !impl_->ok)
        return out;

    ma_device_info* captureInfos{};
    ma_uint32 captureCount{};
    if (ma_context_get_devices(&impl_->context, nullptr, nullptr, &captureInfos, &captureCount) != MA_SUCCESS)
        return out;

    for (ma_uint32 i = 0; i < captureCount; ++i) {
        PluginDeviceDescriptor d;
        d.kind = "audio_capture";
        d.name = captureInfos[i].name;
        d.id.assign(reinterpret_cast<const char*>(&captureInfos[i].id), sizeof(ma_device_id));
        out.push_back(std::move(d));
    }
    return out;
}

std::vector<PluginDeviceDescriptor> PluginDeviceBrokerImpl::listDevices()
{
    if (!dm_)
        return listAudioOnlyViaMiniAudio();

    std::vector<PluginDeviceDescriptor> out;

    for (const auto& cam : dm_->enumerateCameras(10)) {
        PluginDeviceDescriptor d;
        d.kind = "camera";
        d.id = "video:" + std::to_string(cam.index);
        d.name = cam.name;
        out.push_back(std::move(d));
    }

    for (const auto& a : dm_->getCaptureDevices()) {
        PluginDeviceDescriptor d;
        d.kind = a.type.empty() ? "audio_capture" : a.type;
        d.id = a.id;
        d.name = a.name;
        out.push_back(std::move(d));
    }

    for (const auto& h : dm_->enumerateHidDevices(0, 0)) {
        PluginDeviceDescriptor d;
        d.kind = "hid";
        d.id = std::string("hid:") + h.path;
        std::ostringstream title;
        title << h.manufacturer;
        if (!h.product.empty()) {
            if (!h.manufacturer.empty())
                title << ' ';
            title << h.product;
        }
        title << " (" << h.vendor_id << ":" << h.product_id << ")";
        d.name = title.str();
        out.push_back(std::move(d));
    }

    for (const auto& p : dm_->enumerateRegisteredDevices()) {
        PluginDeviceDescriptor d;
        d.kind = "device";
        d.id = p.first;
        d.name = p.second.empty() ? p.first : p.second;
        out.push_back(std::move(d));
    }

    return out;
}

IAudioCaptureStream* PluginDeviceBrokerImpl::openAudioCapture(const std::string& deviceId)
{
    if (!impl_ || !impl_->ok)
        return nullptr;
    if (deviceId.size() != sizeof(ma_device_id))
        return nullptr;

    ma_device_id id{};
    std::memcpy(&id, deviceId.data(), sizeof(id));
    auto* stream = new (std::nothrow) MaCaptureStream(&impl_->context, &id);
    if (stream && !stream->isOpen()) {
        delete stream;
        return nullptr;
    }
    return stream;
}
