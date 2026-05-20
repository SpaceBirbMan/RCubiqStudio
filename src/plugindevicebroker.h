#ifndef PLUGINDEVICEBROKER_H
#define PLUGINDEVICEBROKER_H

#include "misc.h"
#include <memory>

class DeviceManager;

class PluginDeviceBrokerImpl final : public IPluginDeviceBroker {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    DeviceManager* dm_ = nullptr;

public:
    PluginDeviceBrokerImpl();
    ~PluginDeviceBrokerImpl() override;

    void setDeviceManager(DeviceManager* dm) noexcept { dm_ = dm; }

    std::vector<PluginDeviceDescriptor> listDevices() override;
    IAudioCaptureStream* openAudioCapture(const std::string& deviceId) override;

private:
    std::vector<PluginDeviceDescriptor> listAudioOnlyViaMiniAudio();
};

#endif
