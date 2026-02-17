#include "DeviceManager.h"
#include "string"
#include <opencv2/opencv.hpp>


DeviceManager::DeviceManager() = default;

DeviceManager::DeviceManager(AppCore* core) {
    this->acptr = core;


    this->acptr->getEventManager().subscribe(name, "initialize", &DeviceManager::initialize, this);
}

DeviceManager::~DeviceManager() = default;

bool DeviceManager::registerDevice(DevicePtr dev) {
    if (!dev) return false;
    std::string id = dev->id();
    if (id.empty()) return false;

    std::lock_guard<std::mutex> lock(mtx_);
    if (devices_.count(id)) return false; // уже зарегистрировано

    devices_[id] = std::move(dev);
    return true;
}

void DeviceManager::initialize() {
    enumerateCameras(10);
    getCaptureDevices();

    // короче, для создания звукого девайса нужен общий контекст и какие-то параметры
    // искомый варик - номер 0

    // TODO: сделать строковый айдишник у аудио-девайсных записей
    // открывать
    // читать
    // превращать громкость в ускорение
}

std::vector<CameraInfo> DeviceManager::enumerateCameras(int maxIndex) {
    std::vector<CameraInfo> cameras;

    for (int i = 0; i < maxIndex; ++i) {
        cv::VideoCapture cap(i, cv::CAP_ANY);
        if (!cap.isOpened()) {
            continue; // Устройство не доступно или не существует
        }

        CameraInfo info;
        info.index = i;
        info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        info.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        info.maxFps = cap.get(cv::CAP_PROP_FPS);

        // тут нужен платформо-спецефичный вызов для поучения имени

        info.name = "Camera #" + std::to_string(i) + " (" + std::to_string(info.width) + "x" + std::to_string(info.height) + ")";
        std::cout << info.name + " " + std::to_string(info.maxFps) << std::endl;

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
            info.id = pCaptureInfos[i].id;
            info.name = pCaptureInfos[i].name;
            info.type = ma_device_type_capture;
            devices.push_back(info);

            std::cout << "Device " << i << ": " << info.name + "\n";
        }
    }

    ma_context_uninit(&context);
    return devices;
}

bool DeviceManager::openDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it == devices_.end()) return false;

    Device* dev = it->second.get();

    // Назначаем коллбэк или шину при открытии
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

    // Убедимся, что устройство существует
    if (devices_.find(id) == devices_.end()) return false;

    // Удаляем старую привязку
    deviceCallbacks_.erase(id);
    deviceDataBuses_[id] = bus;

    // Если устройство уже открыто — обновляем коллбэк немедленно
    Device* dev = devices_[id].get();
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

    Device* dev = devices_[id].get();
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

const Device* DeviceManager::getDevice(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    return (it != devices_.end()) ? it->second.get() : nullptr;
}
