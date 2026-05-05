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

/// Класс плагинов-движков
class EngineManager : public ICacheable
{
public:
    EngineManager(AppCore* acptr);

    std::string cacheKey() const override { return name; }

    const std::string name = "EngineManager";

private:

    AppCore* acptr;

    struct EngineData {
        IModel* instance;
        DestroyEngine destroy;
    };
    std::unordered_map<std::string, EngineData> engines;   // path -> data
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
    void funcsTableResolvingRequest(); // пусто
    /// Активация движка от сообщения engine_resolving_respond
    void activateEngine(std::vector<void*> pointers);
    /// Запрос на активацию конкретного движка
    void activateEngineByPath(std::string path);
    void removeEngine(std::string path);
    void setFuncs(funcMap map);
    void getActiveFrames(); // запрос на получение указателя на очередь кадров, не актуально
    void addNames(std::vector<std::string> names);
    void startRendering(GraphicBus bus); // не актуально, пусто
    void sendTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table);
    void sendRenderer(IRenderer* ptr); // не актуально
    void sendWinId(uintptr_t id);
    void engTickWrapper(); // для вызова функции tick() у плагина, не актуально
    void sendViewport(ViewportWidget* vp);
    void resize(ViewportBus b); // передаёт размер viewport в плагин
    void sendDataBus(IDataBus* dbp);

};

#endif // ENGINEMANAGER_H
