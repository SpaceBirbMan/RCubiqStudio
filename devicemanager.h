#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "appcore.h"
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include "devices.h"


template<typename T>
class DataBus {
private:
    mutable std::mutex mtx;
    std::queue<T> data;

public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push(item);
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) return false;
        out = std::move(data.front());
        data.pop();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }
};

class DeviceManager {
public:
    using DevicePtr = std::unique_ptr<Device>;
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

    DeviceManager();
    DeviceManager(AppCore* core);
    ~DeviceManager();

    void initialize();

    std::vector<CameraInfo> enumerateCameras(int maxIndex = 10);
    std::vector<AudioDeviceInfo> getCaptureDevices();

    bool registerDevice(DevicePtr dev);

    bool openDevice(const std::string& id);
    void closeDevice(const std::string& id);

    // Привязка к шине данных (устройство будет пушить данные в шину)
    bool bindToDeviceDataBus(const std::string& id, DataBus<std::vector<uint8_t>>* bus);

    // Привязка к коллбэку (устройство будет вызывать функцию)
    bool bindToDeviceDataCallback(const std::string& id, DataCallback callback);

    // Отвязка (удаляет любую привязку: шину или коллбэк)
    void unbindDeviceData(const std::string& id);

    // Отправка данных в устройство (если поддерживается)
    bool sendDataToDevice(const std::string& id, const std::vector<uint8_t>& data);

    // Получение указателя на устройство (для интроспекции или специфичных действий)
    const Device* getDevice(const std::string& id) const;



private:

    AppCore* acptr;

    std::string name = "DeviceManager";

    mutable std::mutex mtx_;
    std::unordered_map<std::string, DevicePtr> devices_;

    // Только один тип привязки на устройство
    std::unordered_map<std::string, DataBus<std::vector<uint8_t>>*> deviceDataBuses_;
    std::unordered_map<std::string, DataCallback> deviceCallbacks_;
};

#endif // DEVICEMANAGER_H
