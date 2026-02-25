#ifndef ENGINEMANAGER_H
#define ENGINEMANAGER_H

#include <vector>
#include "appcore.h"
#include "unordered_map"
#include <set>
#include "shorts.h"
#include "icacheable.h"
#include <nlohmann/json.hpp>
#include "misc.h"

using json = nlohmann::json;

class EngineManager : public ICacheable
{
public:
    EngineManager(AppCore* acptr);

    std::string cacheKey() const override { return name; }

    const std::string name = "EngineManager";

private:



    AppCore* acptr;

    EngineFuncs currentEngineFunctions; // указывает на функцию входа в код движка (main в движке)

    IModel* engine;

    ////////////////////////////////Сериализуемые поля///////////////////////////////////

    std::set<std::string> enginesRegistry {};

    /////////////////////////////////////////////////////////////////////////////////////

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            {"enginesRegistry", enginesRegistry}
        };
        return j;
    }

    void deserializeCache(const nlohmann::json& data) override {
        enginesRegistry = data["enginesRegistry"];
    }

    void preInitialize();

    void initialize();

    void funcsTableResolvingRequest();

    void activateEngine(std::vector<void*> pointers);

    void setFuncs(funcMap map);

    void getActiveFrames();

    void addNames(std::vector<std::string> names);

};

#endif // ENGINEMANAGER_H
