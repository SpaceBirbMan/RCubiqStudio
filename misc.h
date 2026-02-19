#ifndef MISC_H
#define MISC_H

#include <string>
#include <functional>
#include <any>
#include <nlohmann/json.hpp>
#include <deque>
#include "dynamiclibrary.h"
#include "AbstractUiNodes.h"
#include "nlohmann/json.hpp"
#include <optional>

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

class IModel {
public:
    virtual ~IModel() = default;
    virtual std::shared_ptr<renderQueue> acquireRenderQueue() = 0;
    virtual std::shared_ptr<std::deque<std::any>> acquireControlQueue() = 0;
    virtual void processControl(const std::any& cmd) = 0;
    virtual void subscribeToEvents(
        const std::string& event,
        std::function<void(const std::any&)> callback,
        void* subscriber
        ) = 0;
    virtual void test() = 0;
    virtual std::shared_ptr<std::vector<RUI::UiPage>> getUiPages() = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void test() = 0;
};

using CreateEngine = IModel* (*)(void);
using CreateRenderer = IRenderer* (*)(void);

struct LibMeta {
    std::string path;
    std::vector<std::string> func_names;
};

struct EngineFuncs {
    CreateEngine ce;
    // ...
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
};

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    std::string type;
};

#endif // MISC_H
