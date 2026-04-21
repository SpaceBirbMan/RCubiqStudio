#ifndef TRACKERMANAGER_H
#define TRACKERMANAGER_H

#include <memory>
#include <set>
#include <unordered_map>
#include <queue>
#include "appcore.h"
#include "icacheable.h"
#include "misc.h"

class TrackerManager : public ICacheable
{
private:

    std::unordered_map<std::string, ITracker*> trackers;  // path -> instance
    std::string pendingResolutionPath;                      // path currently being resolved

    nlohmann::json serializeCache() const;
    void deserializeCache(const nlohmann::json& data);

    std::set<std::string> trackersRegistry {};
    std::set<std::string> activeTrackerPaths {};  // paths of trackers that should be running

    std::string name = "TrackerManager";

    AppCore* core = nullptr;

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
    void setTrackerActiveInUI(std::string path);
};

#endif