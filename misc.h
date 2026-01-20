#ifndef MISC_H
#define MISC_H

#include <string>
#include <functional>
#include <any>
#include <nlohmann/json.hpp>
#include <deque>
#include "dynamiclibrary.h"
#include "AbstractUiNodes.h"

using json = nlohmann::json;
using payload = std::vector<uint8_t>; //байт-буфер для payload
using void_func = std::function<void(const std::any&)>;

struct cacheForm {
    std::string name;
    void_func desfn; // сюда помещается функция десериализации кеша
    std::function<nlohmann::json(const std::any&)> sefn; // сюда помещается функция сериализации кеша
};

struct Frame {
    std::vector<uint8_t> pixels{}; // пиксели (8 бит на каждый канал)
    int width;
    int height;
    short stride; // шаг для массива пикселей
};

using renderQueue = std::deque<Frame>;

struct subStruct {
    std::string name;
    std::function<void(const std::any&)> callback; // todo: Мб ссылка нужна

    subStruct(std::string m, std::function<void(const std::any&)> cb)
        : name(std::move(m)), callback(std::move(cb)) {}
};

enum UIElements {
    CONTROL_TABLE,
    COMPONENT_TREE,
    SLIDER,
    BUTTON
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
    virtual void test();
    virtual std::shared_ptr<UiPage> getUiRoot() = 0;
};

using CreateEngine = IModel* (*)(void);

struct LibMeta {
    std::string path;
    std::vector<std::string> func_names;
};

struct EngineFuncs {
    CreateEngine ce;
    // ...
};

#endif // MISC_H
