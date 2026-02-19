#include "DeviceManager.h"
#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>

DeviceManager::DeviceManager() = default;

DeviceManager::DeviceManager(AppCore* core) {
    this->acptr = core;

    this->acptr->getEventManager().subscribe(name, "initialize", &DeviceManager::initialize, this);
    this->acptr->getEventManager().subscribe(name, "get_video_devices_request", &DeviceManager::sendVideoDevices, this);
    this->acptr->getEventManager().subscribe(name, "activate_camera", &DeviceManager::activateCamera, this);
}

void DeviceManager::sendVideoDevices() {
    std::vector<CameraInfo> tmp = this->cameras = enumerateCameras(10);
    acptr->getEventManager().sendMessage(AppMessage(name, "get_video_devices_respond", tmp));
}

DeviceManager::~DeviceManager() = default;

bool DeviceManager::registerDevice(DevicePtr dev) {
    if (!dev) return false;
    std::string id = dev->id();
    if (id.empty()) return false;

    std::lock_guard<std::mutex> lock(mtx_);
    if (devices_.count(id)) return false;

    devices_[id] = std::move(dev);
    return true;
}

void DeviceManager::activateCamera(std::string name) {
    CameraInfo foundCamera;
    bool exists = false;

    for (const auto& camera : cameras) {
        if (camera.name == name) {
            foundCamera = camera;
            exists = true;
            break;
        }
    }

    if (!exists) {
        std::cerr << "Error: Camera not found by name: " << name << std::endl;
        return;
    }

    std::string expectedId = "video:" + std::to_string(foundCamera.index);

    std::unique_lock<std::mutex> lock(mtx_);

    if (devices_.count(expectedId)) {
        auto existingDev = devices_[expectedId];
        if (existingDev->isOpen()) {
            lock.unlock();

            this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_info", foundCamera));
            this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_device", existingDev));
            return;
        } else {
            devices_.erase(expectedId);
            deviceCallbacks_.erase(expectedId);
            deviceDataBuses_.erase(expectedId);
        }
    }

    auto devp = std::make_shared<VideoDevice>(foundCamera.index);
    std::string devId = devp->id();

    if (devId.empty()) {
        std::cerr << "Error: Device ID is empty" << std::endl;
        return;
    }

    devices_[devId] = std::move(devp);
    std::shared_ptr<Device> devicePtr = devices_[devId];

    lock.unlock();

    this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_info", foundCamera));
    this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_device", devicePtr));
}

void DeviceManager::initialize() {
    getCaptureDevices();
}

std::vector<CameraInfo> DeviceManager::enumerateCameras(int maxIndex) {
    std::vector<CameraInfo> cameras;

    for (int i = 0; i < maxIndex; ++i) {
        cv::VideoCapture cap(i, cv::CAP_ANY);
        if (!cap.isOpened()) {
            continue;
        }

        CameraInfo info;
        info.index = i;
        info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        info.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        info.maxFps = cap.get(cv::CAP_PROP_FPS);

        // TODO: Platform-specific name retrieval
        info.name = "Camera #" + std::to_string(i) + " (" + std::to_string(info.width) + "x" + std::to_string(info.height) + ")";
        std::cout << info.name << " " << info.maxFps << std::endl;

        cap.release();
        cameras.push_back(info);
    }

    return cameras;
}

std::vector<AudioDeviceInfo> DeviceManager::getCaptureDevices() {
    ma_context context;
    ma_result result = ma_context_init(nullptr, 0, nullptr, &context);
    if (result != MA_SUCCESS) {
        return {};
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    result = ma_context_get_devices(
        &context,
        &pPlaybackInfos, &playbackCount,
        &pCaptureInfos, &captureCount
        );

    std::vector<AudioDeviceInfo> devices;
    if (result == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            AudioDeviceInfo info;
            info.id = pCaptureInfos[i].name;
            info.name = pCaptureInfos[i].name;
            info.type = ma_device_type_capture;
            devices.push_back(info);

            std::cout << "Device " << i << ": " << info.name << "\n";
        }
    }

    ma_context_uninit(&context);
    return devices;
}

bool DeviceManager::openDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it == devices_.end()) return false;

    std::shared_ptr<Device> dev = it->second;

    if (deviceDataBuses_.count(id)) {
        auto* bus = deviceDataBuses_[id];
        dev->setDataCallback([bus](const std::vector<uint8_t>& data) {
            bus->push(data);
        });
    } else if (deviceCallbacks_.count(id)) {
        dev->setDataCallback(deviceCallbacks_[id]);
    }

    return dev->open();
}

void DeviceManager::closeDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it != devices_.end()) {
        it->second->close();
    }
}

bool DeviceManager::bindToDeviceDataBus(const std::string& id, DataBus<std::vector<uint8_t>>* bus) {
    if (!bus) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    if (devices_.find(id) == devices_.end()) return false;

    deviceCallbacks_.erase(id);
    deviceDataBuses_[id] = bus;

    std::shared_ptr<Device> dev = devices_[id];
    if (dev->isOpen()) {
        dev->setDataCallback([bus](const std::vector<uint8_t>& data) {
            bus->push(data);
        });
    }

    return true;
}

bool DeviceManager::bindToDeviceDataCallback(const std::string& id, DataCallback callback) {
    if (!callback) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    if (devices_.find(id) == devices_.end()) return false;

    deviceDataBuses_.erase(id);
    deviceCallbacks_[id] = std::move(callback);

    std::shared_ptr<Device> dev = devices_[id];
    if (dev->isOpen()) {
        dev->setDataCallback(deviceCallbacks_[id]);
    }

    return true;
}

void DeviceManager::unbindDeviceData(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    deviceDataBuses_.erase(id);
    deviceCallbacks_.erase(id);

    auto it = devices_.find(id);
    if (it != devices_.end() && it->second->isOpen()) {
        it->second->setDataCallback(nullptr);
    }
}

bool DeviceManager::sendDataToDevice(const std::string& id, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it == devices_.end()) return false;
    return it->second->sendCommand(data);
}

std::shared_ptr<Device> DeviceManager::getDevice(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    return (it != devices_.end()) ? it->second : nullptr;
}
