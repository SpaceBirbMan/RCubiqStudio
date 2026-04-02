#ifndef OTHERPLUGINS_H
#define OTHERPLUGINS_H

#include <set>
#include "icacheable.h"
#include "misc.h"

class AppCore;

class OtherPlugins : public ICacheable
{
private:

    AppCore* core;
    IDataBus* data_bus;
    std::string name = "PluginManager";
    std::unordered_map<std::string, IGenPlugin* > plugins {};
    bool need_to_set_db = false;

    void preInitialize();
    void initialize();

    ////////////////////////////////Сериализуемые поля///////////////////////////////////

    std::set<std::string> pluginsRegistry {};

    /////////////////////////////////////////////////////////////////////////////////////

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            {"pluginsRegistry", pluginsRegistry}
        };
        return j;
    }

    void deserializeCache(const nlohmann::json& data) override {
        pluginsRegistry = data["pluginsRegistry"];
    }

public:

    OtherPlugins(AppCore* appcore);
    std::string cacheKey() const override { return name; }
    void registerPlugin(std::vector<void*> pointers);
    void deletePlugin(std::string name);
    void setDataBus(IDataBus* dbp);
    void addPaths(std::vector<std::string> paths);
    void postSetDataBus();
    void postInitialize();

};

#endif // OTHERPLUGINS_H
