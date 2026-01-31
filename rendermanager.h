#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include <string>
#include "icacheable.h"
#include "appcore.h"
#include "renderapi.h"

#include <vector>
#include "unordered_map"
#include <set>
#include "shorts.h"
#include "misc.h"

class RenderManager : public ICacheable
{
private:
    std::string renderApi = ""; // путь к API рендера

    IRenderer* renderer;

    AppCore* core;

    const std::string name = "RenderManager";

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            {"renderApi", renderApi}
        };
        return j;
    }

    void deserializeCache(const nlohmann::json& data) override {
        try {
        renderApi = data["renderApi"];
        } catch (...) {
            std::cerr << "cannot deserialize cache" << std::endl;
        }
    }

    void setRenderApi(std::string apiPath);
    void createRenderer(std::vector<void*> pointers);

    void preInitialize() {

        auto deserialize_lambda = [this](const nlohmann::json& data) { this->deserializeCache(data); };
        auto serialize_lambda = [this]() -> json { return this->serializeCache(); };
        std::function<void(const nlohmann::json&)> deserialize_wrapper = deserialize_lambda;
        std::function<json()> serialize_wrapper = serialize_lambda;

        cacheForm cf_instance;
        cf_instance.name = name;
        cf_instance.desfn = deserialize_wrapper;
        cf_instance.sefn = serialize_wrapper;

        core->getEventManager().sendMessage(AppMessage(name, "sub_to_cache", cf_instance));
        core->getEventManager().sendMessage(AppMessage(name, "module_subscribed", name));
    }


public:
    RenderManager(AppCore* acptr);

    std::string cacheKey() const override { return name; }
};

#endif // RENDERMANAGER_H
