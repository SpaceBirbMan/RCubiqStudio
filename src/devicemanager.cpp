#include "devicemanager.h"
#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>

// Вспомогательная функция для конвертации UTF-16/UTF-32 в UTF-8
static std::string wideToUtf8(const wchar_t *wstr)
{
    if (!wstr)
        return {};

    std::string result;

#ifdef _WIN32
    // Windows: wchar_t это 16 бит (UTF-16)
    const char16_t *utf16 = reinterpret_cast<const char16_t *>(wstr);
    size_t len = 0;
    while (utf16[len])
        ++len;

    // Простая ручная конвертация UTF-16 -> UTF-8 без зависимостей
    for (size_t i = 0; i < len;)
    {
        uint32_t codepoint = utf16[i++];
        // Обработка суррогатных пар
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF && i < len)
        {
            uint32_t low = utf16[i];
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                codepoint = ((codepoint - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
                ++i;
            }
        }

        if (codepoint < 0x80)
        {
            result += static_cast<char>(codepoint);
        }
        else if (codepoint < 0x800)
        {
            result += static_cast<char>(0xC0 | (codepoint >> 6));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint < 0x10000)
        {
            result += static_cast<char>(0xE0 | (codepoint >> 12));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else
        {
            result += static_cast<char>(0xF0 | (codepoint >> 18));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
#else
    // Linux/macOS: wchar_t это 32 бита (UTF-32)
    // Конвертация UTF-32 -> UTF-8
    while (*wstr)
    {
        uint32_t codepoint = static_cast<uint32_t>(*wstr++);
        if (codepoint < 0x80)
        {
            result += static_cast<char>(codepoint);
        }
        else if (codepoint < 0x800)
        {
            result += static_cast<char>(0xC0 | (codepoint >> 6));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint < 0x10000)
        {
            result += static_cast<char>(0xE0 | (codepoint >> 12));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else
        {
            result += static_cast<char>(0xF0 | (codepoint >> 18));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
#endif
    return result;
}

DeviceManager::DeviceManager() = default;

DeviceManager::DeviceManager(AppCore *core)
{
    this->acptr = core;

    this->acptr->getEventManager().subscribe(name, "initialize", &DeviceManager::initialize, this);
    this->acptr->getEventManager().subscribe(name, "get_video_devices_request", &DeviceManager::sendVideoDevices, this);
    this->acptr->getEventManager().subscribe(name, "activate_camera", &DeviceManager::activateCamera, this);
    this->acptr->getEventManager().subscribe(name, "start_hid_listeners", &DeviceManager::startHid, this);
    this->acptr->getEventManager().subscribe(name, "stop_hid_listeners", &DeviceManager::stopHid, this);
}

void DeviceManager::sendVideoDevices()
{
    std::vector<CameraInfo> tmp = this->cameras = enumerateCameras(10);
    acptr->getEventManager().sendMessage(AppMessage(name, "get_video_devices_respond", tmp));
}

DeviceManager::~DeviceManager() = default;

bool DeviceManager::registerDevice(DevicePtr dev)
{
    if (!dev)
        return false;
    std::string id = dev->id();
    if (id.empty())
        return false;

    std::lock_guard<std::mutex> lock(mtx_);
    if (devices_.count(id))
        return false;

    devices_[id] = std::move(dev);
    return true;
}

void DeviceManager::startHid()
{
    std::cout << "Disabled for debugging purposes\n";
    // const std::string id = "os:keyboard";

    // std::unique_lock<std::mutex> lock(mtx_);

    // if (devices_.count(id))
    // {
    //     return;
    // }

    // auto dev = std::make_shared<OsInputDevice>();
    // devices_[id] = dev;
    // auto *bus = new DataBus<std::vector<uint8_t>>();
    // deviceDataBuses_[id] = bus;

    // dev->setDataCallback([bus](const std::vector<uint8_t> &data)
    //                      { bus->push(data); });

    // if (!dev->open())
    // {
    //     std::cerr << "Failed to open os keyboard listener\n";
    //     devices_.erase(id);
    //     deviceDataBuses_.erase(id);
    //     delete bus;
    //     return;
    // }

    // hidRunning_ = true;

    // hidThread_ = std::thread([this, bus]()
    //                          {
    //     while (hidRunning_) {
    //         std::vector<uint8_t> packet {};
    //         bus->try_pop(packet);

    //         if (packet.size() < 5) {
    //             continue;
    //         }

    //         uint8_t type = packet[0];
    //         uint16_t keycode =
    //             static_cast<uint16_t>(packet[1]) |
    //             (static_cast<uint16_t>(packet[2]) << 8);

    //         uint16_t modifiers =
    //             static_cast<uint16_t>(packet[3]) |
    //             (static_cast<uint16_t>(packet[4]) << 8);

    //         std::cout
    //             << "KEY "
    //             << (type == EVENT_KEY_PRESSED ? "DOWN " : "UP ")
    //             << "code=" << keycode
    //             << " mod=" << modifiers
    //             << std::endl;
    //     } });
}

void DeviceManager::stopHid()
{
}

void DeviceManager::activateCamera(std::string name)
{
    CameraInfo foundCamera;
    bool exists = false;

    for (const auto &camera : cameras)
    {
        if (camera.name == name)
        {
            foundCamera = camera;
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        std::cerr << "Error: Camera not found by name: " << name << std::endl;
        return;
    }

    std::string expectedId = "video:" + std::to_string(foundCamera.index);

    std::unique_lock<std::mutex> lock(mtx_);

    if (devices_.count(expectedId))
    {
        auto existingDev = devices_[expectedId];
        if (existingDev->isOpen())
        {
            lock.unlock();

            this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_info", foundCamera));
            this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_device", existingDev));
            return;
        }
        else
        {
            devices_.erase(expectedId);
            deviceCallbacks_.erase(expectedId);
            deviceDataBuses_.erase(expectedId);
        }
    }

    auto devp = std::make_shared<VideoDevice>(foundCamera.index);
    std::string devId = devp->id();

    if (devId.empty())
    {
        std::cerr << "Error: Device ID is empty" << std::endl;
        return;
    }

    devices_[devId] = std::move(devp);
    std::shared_ptr<Device> devicePtr = devices_[devId];

    lock.unlock();

    this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_info", foundCamera));
    this->acptr->getEventManager().sendMessage(AppMessage(name, "active_camera_device", devicePtr));
}

void DeviceManager::initialize()
{
    getCaptureDevices();
    this->acptr->getEventManager().sendMessage(AppMessage(name, "start_hid_listeners", 0));
}

std::vector<CameraInfo> DeviceManager::enumerateCameras(int maxIndex)
{
    std::vector<CameraInfo> cameras;

    for (int i = 0; i < maxIndex; ++i)
    {
        cv::VideoCapture cap(i, cv::CAP_ANY);
        if (!cap.isOpened())
        {
            continue;
        }

        CameraInfo info;
        info.index = i;
        info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)); // TODO: нужна настройка по размерам вебки
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

std::vector<HidDeviceInfo> DeviceManager::enumerateHidDevices(unsigned short vid, unsigned short pid)
{
    std::vector<HidDeviceInfo> result;

    struct hid_device_info *devs = hid_enumerate(vid, pid); // vid=0,pid=0 = все устройства [[13]]
    if (!devs)
        return result;

    for (auto *cur = devs; cur; cur = cur->next)
    {
        HidDeviceInfo info{};
        info.vendor_id = cur->vendor_id;
        info.product_id = cur->product_id;
        info.path = cur->path ? cur->path : "";
        info.usage_page = cur->usage_page;
        info.usage = cur->usage;

#ifdef _WIN32
        if (cur->manufacturer_string)
            info.manufacturer = wideToUtf8(cur->manufacturer_string);
        if (cur->product_string)
            info.product = wideToUtf8(cur->product_string);
        if (cur->serial_number)
            info.serial = wideToUtf8(cur->serial_number);
#else
        // Linux/macOS обычно уже в UTF-8
        if (cur->manufacturer_string)
        {
            char mbstr[256]{};
            wcstombs(mbstr, cur->manufacturer_string, sizeof(mbstr) - 1);
            info.manufacturer = mbstr;
        }
        // ... аналогично для product/serial
#endif

        result.push_back(info);
    }

    hid_free_enumeration(devs); // Обязательно освободить память [[13]]
    return result;
}

std::vector<AudioDeviceInfo> DeviceManager::getCaptureDevices()
{
    ma_context context;
    ma_result result = ma_context_init(nullptr, 0, nullptr, &context);
    if (result != MA_SUCCESS)
    {
        return {};
    }

    ma_device_info *pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info *pCaptureInfos;
    ma_uint32 captureCount;

    result = ma_context_get_devices(
        &context,
        &pPlaybackInfos, &playbackCount,
        &pCaptureInfos, &captureCount);

    std::vector<AudioDeviceInfo> devices;
    if (result == MA_SUCCESS)
    {
        for (ma_uint32 i = 0; i < captureCount; ++i)
        {
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

bool DeviceManager::openDevice(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it == devices_.end())
        return false;

    std::shared_ptr<Device> dev = it->second;

    if (deviceDataBuses_.count(id))
    {
        auto *bus = deviceDataBuses_[id];
        dev->setDataCallback([bus](const std::vector<uint8_t> &data)
                             { bus->push(data); });
    }
    else if (deviceCallbacks_.count(id))
    {
        dev->setDataCallback(deviceCallbacks_[id]);
    }

    return dev->open();
}

void DeviceManager::closeDevice(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it != devices_.end())
    {
        it->second->close();
    }
}

bool DeviceManager::bindToDeviceDataBus(const std::string &id, DeviceDataBus<std::vector<uint8_t>> *bus)
{
    if (!bus)
        return false;
    std::lock_guard<std::mutex> lock(mtx_);

    if (devices_.find(id) == devices_.end())
        return false;

    deviceCallbacks_.erase(id);
    deviceDataBuses_[id] = bus;

    std::shared_ptr<Device> dev = devices_[id];
    if (dev->isOpen())
    {
        dev->setDataCallback([bus](const std::vector<uint8_t> &data)
                             { bus->push(data); });
    }

    return true;
}

bool DeviceManager::bindToDeviceDataCallback(const std::string &id, DataCallback callback)
{
    if (!callback)
        return false;
    std::lock_guard<std::mutex> lock(mtx_);

    if (devices_.find(id) == devices_.end())
        return false;

    deviceDataBuses_.erase(id);
    deviceCallbacks_[id] = std::move(callback);

    std::shared_ptr<Device> dev = devices_[id];
    if (dev->isOpen())
    {
        dev->setDataCallback(deviceCallbacks_[id]);
    }

    return true;
}

void DeviceManager::unbindDeviceData(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mtx_);
    deviceDataBuses_.erase(id);
    deviceCallbacks_.erase(id);

    auto it = devices_.find(id);
    if (it != devices_.end() && it->second->isOpen())
    {
        it->second->setDataCallback(nullptr);
    }
}

bool DeviceManager::sendDataToDevice(const std::string &id, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    if (it == devices_.end())
        return false;
    return it->second->sendCommand(data);
}

std::shared_ptr<Device> DeviceManager::getDevice(const std::string &id) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = devices_.find(id);
    return (it != devices_.end()) ? it->second : nullptr;
}
