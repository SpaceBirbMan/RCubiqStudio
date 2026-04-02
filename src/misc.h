#ifndef MISC_H
#define MISC_H

#include <string>
#include <functional>
#include <any>
#include <nlohmann/json.hpp>
#include <deque>
#include "abstractuinodes.h"

using json = nlohmann::json;
using payload = std::vector<uint8_t>; //байт-буфер для payload
using void_func = std::function<void(const std::any&)>;

struct cacheForm {
    std::string name;
    std::function<void(const nlohmann::json&)> desfn;   // десериализация: принимает json
    std::function<nlohmann::json()> sefn;               // сериализация: возвращает json
};

struct Frame {
    std::vector<uint8_t> pixels{}; // пиксели (8 бит на каждый канал)
    int width;
    int height;
    short stride; // шаг для массива пикселей
};

using renderQueue = std::deque<Frame>;

struct subStruct {
    std::string receiver = "N/A";
    std::string name;
    std::function<void(const std::any&)> callback;

    subStruct(std::string r, std::string m, std::function<void(const std::any&)> cb)
        : receiver(std::move(r)), name(std::move(m)), callback(std::move(cb)) {}
};

struct GraphicBus {
    std::deque<std::shared_ptr<void>> *textures_handlers_in_app = nullptr;
    std::deque<std::shared_ptr<void>> *textures_handlers_in_engine = nullptr;
    std::deque<std::shared_ptr<void>> *images = nullptr;
};


struct TextureHandle {
    uint32_t id;
};

enum class PixelFormat : uint8_t {
    RGBA8,
    BGRA8,
    R8,
    // ...
};

struct TextureDesc {
    uint16_t width;
    uint16_t height;
    uint8_t layers;
    PixelFormat format;
    const void* data; // только для создания, не хранится
    uint32_t dataSize;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual TextureHandle createTexture(const TextureDesc* desc) = 0;
    virtual void destroyTexture(TextureHandle handle) = 0;
    virtual void setWindowHandle(void* handle) = 0;
    virtual void frame() = 0;
    virtual void test() = 0;
};

struct CacheObject {
    nlohmann::json object;
    std::string name;
};

struct CameraInfo {
    int index;
    std::string name;
    int width;
    int height;
    double maxFps;

    std::string to_string() {
        return std::to_string(index) + " " + name + " " + std::to_string(width) + "x" + std::to_string(height) + " "+  std::to_string(maxFps) + "\n";
    }
};

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    std::string type;
};

struct TrackerInfo {

};

#include "ieventmanager.h"

class IDataBus {
public:
    virtual void registerData(const std::string& key, std::any data) = 0;
    virtual std::any& getData(const std::string& key) = 0;
    virtual void remove(const std::string& key) = 0;
};

class ITracker {
public:
    ITracker() = default;
    ITracker(IEventManager* eventManager, IDataBus* dataBus) {}
    virtual ~ITracker() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual std::unordered_map<std::string, std::shared_ptr<void>>* getTable() const = 0;
};

struct EngineMeta {

    std::unordered_map<std::string, std::shared_ptr<void>>* table;
    IRenderer* renderer;
    uintptr_t windowHandle;

};

struct ViewportBus {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    double dpr = 1.0;

    bool isVisible = true;
};

class IModel {
public:
    IModel() = default;
    IModel(IEventManager* eventManager, IDataBus* dbus) {}
    virtual ~IModel() = default;
    virtual void test() = 0;
    virtual std::shared_ptr<std::vector<RUI::UiPage>> getUiPages() = 0;
    virtual void setDataBus(IDataBus* db) = 0;
    virtual void setMeta(EngineMeta meta) = 0;
    virtual void tick() = 0;
    virtual void initRender() = 0;
    virtual void update(ViewportBus) = 0;
};

class IGenPlugin {
public:
    IGenPlugin() = default;
    IGenPlugin(IEventManager* eventManager, IDataBus* dataBus) {}
    virtual ~IGenPlugin() = default;
    virtual void setDataBus(IDataBus* db) = 0;
    virtual std::string getName() = 0;
};

using CreateEngine = IModel* (*)(IEventManager*, IDataBus*);
using CreateRenderer = IRenderer* (*)(void);
using CreateTracker = ITracker* (*)(IEventManager*, IDataBus*);
using CreatePlugin = IGenPlugin* (*)(IEventManager*, IDataBus*);

struct LibMeta {
    std::string path;
    std::vector<std::string> func_names;
};

struct Meta {
    std::string path;
    std::vector<std::string> func_names;
};

struct EngineFuncs {
    CreateEngine ce;
    // ...
};

#endif // MISC_H
