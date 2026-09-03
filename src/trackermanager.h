#ifndef TRACKERMANAGER_H
#define TRACKERMANAGER_H

#include <memory>
#include <set>
#include <unordered_map>
#include <queue>
#include "appcore.h"
#include "icacheable.h"
#include "rcqapi.h"
#include "logger.h"
#include "controltablehost.h"
#include "hostinput.h"
#include <deque>

class TrackerManager : public ICacheable
{
private:

    struct TrackerData {
        ITracker* instance;
        DestroyTracker destroy;
    };
    std::unordered_map<std::string, TrackerData> trackers;  // path -> data
    std::string pendingResolutionPath;                      // path currently being resolved
    std::deque<std::string> pendingResolutionQueue_;

    ControlTableHost controlTableHost_;

    nlohmann::json serializeCache() const;
    void deserializeCache(const nlohmann::json& data);

    std::set<std::string> trackersRegistry {};
    std::set<std::string> activeTrackerPaths {};  // paths of trackers that should be running

    std::string name = "TrackerManager";

    AppCore* core = nullptr;
    ModuleLogProvider log_;

    /// Graceful shutdown for all tracker plugins (quit path).
    void onStopTracker();

    std::unordered_map<std::string, std::string> trackerDisplayNames_;
    std::string trackerDisplayName(const std::string& path) const;

public:

    TrackerManager() = default;
    TrackerManager(AppCore* core);
    ~TrackerManager();

    std::string cacheKey() const;

    void startTracking();
    void stopTracking();

    bool isRunning() const;

    void initialize();
    void preInitialize();
    void addNames(std::vector<std::string> names);
    void activateTracker(std::vector<void*> pointers);
    void activateTrackerByPath(std::string path);
    void deactivateTrackerByPath(std::string path);
    void removeTracker(std::string path);
    void resendTrackerTables();
    void syncTrackerUiAfterEntry(std::string path);
    void onControlTableRegister(ControlTableRegister reg);
    void publishControlTable(const std::string& sourcePath,
                               std::vector<std::string> added,
                               std::vector<std::string> removed);
    void mergeTrackerControls(const std::string& path, ITracker* instance);
    void removeTrackerControls(const std::string& path);
    void rebuildControlTableFromAllTrackers();
    void requestTrackerResolve(const std::string& path);
    void advanceResolutionQueue();
    void tryAutoStartTracker(const std::string& path, ITracker* instance);

    HostInputControllers hostInput_;
};

#endif