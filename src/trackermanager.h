#ifndef TRACKERMANAGER_H
#define TRACKERMANAGER_H

#include <memory>
#include <set>
#include "appcore.h"
#include "icacheable.h"
#include "misc.h"

class TrackerManager : public ICacheable
{
private:

    std::shared_ptr<ITracker> currentTracker;

    nlohmann::json serializeCache() const;
    void deserializeCache(const nlohmann::json& data);

    std::set<std::string>trackersRegistry {};

    std::string name = "TrackerManager";

    ITracker* tracker;

    AppCore* core = nullptr;

public:

    TrackerManager() = default;
    TrackerManager(AppCore* core);
    ~TrackerManager();

    std::string cacheKey() const;

    void setTracker(std::shared_ptr<ITracker> tracker);

    bool startTracking();
    void stopTracking();

    bool isRunning() const;

    void initialize();

    void preInitialize();
    void addNames(std::vector<std::string> names);
    void activateTracker(std::vector<void*> pointers);
};

#endif
