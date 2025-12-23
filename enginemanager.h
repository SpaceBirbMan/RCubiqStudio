#ifndef ENGINEMANAGER_H
#define ENGINEMANAGER_H

#include <vector>
#include "appcore.h"
#include "unordered_map"
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

private:

    std::string name = "EngineManager";

    AppCore* acptr;

    funcMap currentEngineFunctions; // указывает на функцию входа в код движка (main в движке)

    ////////////////////////////////Сериализуемые поля///////////////////////////////////

    std::unordered_map<std::string, std::string> enginesRegistry {}; // название | путь

    /////////////////////////////////////////////////////////////////////////////////////

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            // {"enginesRegistry", enginesRegistry}
            {"enginesRegistry", "test"},
            {"renderSettings", "CPU"},
            {"mode", "flat"},
            {"isCorrupted", "no"},
            {"hadCrash", "no"}
        };
        return j;
    }

    void deserializeCache(const std::any& data) override {
        auto j = std::any_cast<nlohmann::json>(data);
        enginesRegistry = j["enginesRegistry"];
    }

    void initialize();

    void funcsTableResolvingRequest();

    void setActiveEngine(std::string ename);

    void setFuncs(funcMap map);

    void getActiveFrames();

};

#endif // ENGINEMANAGER_H
