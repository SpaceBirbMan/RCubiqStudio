#ifndef MISC_H
#define MISC_H

#include <set>
#include <string>
#include <functional>
#include <any>
#include <nlohmann/json.hpp>
#include <deque>
#include "AbstractUiNodes.h"

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

struct EngineMeta {

    std::unordered_map<std::string, std::shared_ptr<void>>* table;

};

class IModel {
public:
    virtual ~IModel() = default;
    virtual std::shared_ptr<renderQueue> acquireRenderQueue() = 0;
    virtual std::shared_ptr<std::deque<std::any>> acquireControlQueue() = 0;
    virtual void processControl(const std::any& cmd) = 0;
    virtual void test() = 0;
    virtual std::shared_ptr<std::vector<RUI::UiPage>> getUiPages() = 0;
    virtual void setHandlersQueue(std::deque<std::shared_ptr<void>>* ptrs) = 0;
    virtual void setMeta(EngineMeta meta) = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
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

class ITracker {
public:
    virtual ~ITracker() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual std::unordered_map<std::string, std::shared_ptr<void>>* getTable() const = 0;

};

using CreateEngine = IModel* (*)(void);
using CreateRenderer = IRenderer* (*)(void);
using CreateTracker = ITracker* (*)(void);

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
