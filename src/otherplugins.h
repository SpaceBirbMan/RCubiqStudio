#ifndef OTHERPLUGINS_H
#define OTHERPLUGINS_H

#include <set>
#include <queue>
#include "icacheable.h"
#include "misc.h"

class AppCore;

class OtherPlugins : public ICacheable
{
private:

    AppCore* core;
    IDataBus* data_bus = nullptr;
    std::string name = "PluginManager";
    std::unordered_map<std::string, IGenPlugin*> plugins {};  // path -> instance
    bool need_to_set_db = false;
    std::queue<std::string> pendingPaths;  // paths currently being resolved (FIFO)

    void preInitialize();
    void initialize();

    ////////////////////////////////Сериализуемые поля///////////////////////////////////

    std::set<std::string> pluginsRegistry {};
    std::set<std::string> activePluginPaths {};  // paths of plugins that are currently loaded/active

    /////////////////////////////////////////////////////////////////////////////////////

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {
            {"pluginsRegistry", pluginsRegistry},
            {"activePluginPaths", activePluginPaths}
        };
        return j;
    }

    void deserializeCache(const nlohmann::json& data) override {
        pluginsRegistry = data["pluginsRegistry"];
        if (data.contains("activePluginPaths"))
            activePluginPaths = data["activePluginPaths"];
    }

public:

    OtherPlugins(AppCore* appcore);
    std::string cacheKey() const override { return name; }
    void registerPlugin(std::vector<void*> pointers);
    void deletePlugin(std::string path);
    void setDataBus(IDataBus* dbp);
    void addPaths(std::vector<std::string> paths);
    void postSetDataBus();
    void postInitialize();
    void disablePlugin(std::string path);
    void enablePlugin(std::string path);

};

#endif // OTHERPLUGINS_H