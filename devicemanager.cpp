#include "DeviceManager.h"

DeviceManager::DeviceManager() = default;
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
