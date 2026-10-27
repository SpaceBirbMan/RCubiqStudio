#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "appcore.h"
#include "icacheable.h"
#include "rcqapi.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

/// Владеет плагинами нового типа и их шиной. Ядро этим менеджером не владеет:
/// его создают в main рядом с остальными модулями.
class PluginManager final : public Service, public ICacheable
{
public:
    explicit PluginManager(AppCore* core);
    ~PluginManager() override;

    std::string cacheKey() const override { return getName(); }
    nlohmann::json serializeCache() const override;
    void deserializeCache(const nlohmann::json& data) override;

private:
    struct Slot {
        std::string packagePath;
        std::string entryPath;
        IPlugin* plugin = nullptr;
        bool updating = false;
    };

    struct PendingPackage {
        std::string packagePath;
        std::string entryPath;
        std::string displayName;
        std::string description;
    };

    AppCore* core_ = nullptr;
    IEventManager* host_ = nullptr;
    IDataBus* hostBus_ = nullptr;
    EventManager plugins_;
    IDataBus* pluginBus_ = nullptr;

    mutable std::mutex slotsMutex_;
    std::unordered_map<std::string, Slot> slots_;
    std::queue<PendingPackage> pending_;
    std::set<std::string> registry_;
    std::set<std::string> active_;

    std::atomic<bool> ticking_{false};
    std::thread ticker_;

    void preInitialize();
    void initialize();
    void postInitialize();
    void addPaths(std::vector<std::string> paths);
    void enablePlugin(std::string path);
    void disablePlugin(std::string path);
    void removePlugin(std::string path);

    void mirrorAllowedData();
    void bridgeHost();
    void armResponses(const std::string& pluginName);
    bool loadPackage(const std::string& path);
    void onResolved(std::vector<void*> pointers);
    void dropSlot(Slot& slot);
    void notifyUi(const std::string& path, const std::string& displayName, const std::string& description, bool active);
    std::filesystem::path cacheRoot() const;
    void tickLoop();
};

#endif
