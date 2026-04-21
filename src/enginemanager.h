#ifndef ENGINEMANAGER_H
#define ENGINEMANAGER_H

#include <vector>
#include <unordered_map>
#include "appcore.h"
#include <set>
#include "shorts.h"
#include "icacheable.h"
#include <nlohmann/json.hpp>
#include "misc.h"

using json = nlohmann::json;
class ViewportWidget;

class EngineManager : public ICacheable
{
public:
    EngineManager(AppCore* acptr);

    std::string cacheKey() const override { return name; }

    const std::string name = "EngineManager";

private:

    AppCore* acptr;

    std::unordered_map<std::string, IModel*> engines;   // path -> instance
    std::string activeEnginePath;                        // currently active engine path
    std::string pendingResolutionPath;                   // path being resolved right now

    std::function<void()> tickWrapper = nullptr;
    void* viewport = nullptr;

    ////////////////////////////////Сериализуемые поля///////////////////////////////////

    std::set<std::string> enginesRegistry {};

    /////////////////////////////////////////////////////////////////////////////////////

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            {"enginesRegistry", enginesRegistry},
            {"activeEnginePath", activeEnginePath}
        };
        return j;
    }

    void deserializeCache(const nlohmann::json& data) override {
        enginesRegistry = data["enginesRegistry"];
        if (data.contains("activeEnginePath"))
            activeEnginePath = data["activeEnginePath"];
    }

    void preInitialize();
    void initialize();
    void funcsTableResolvingRequest();
    void activateEngine(std::vector<void*> pointers);
    void activateEngineByPath(std::string path);
    void removeEngine(std::string path);
    void setFuncs(funcMap map);
    void getActiveFrames();
    void addNames(std::vector<std::string> names);
    void startRendering(GraphicBus bus);
    void sendTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table);
    void sendRenderer(IRenderer* ptr);
    void sendWinId(uintptr_t id);
    void engTickWrapper();
    void sendViewport(ViewportWidget* vp);
    void resize(ViewportBus b);
    void sendDataBus(IDataBus* dbp);

};

#endif // ENGINEMANAGER_H