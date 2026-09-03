#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "appcore.h"
#include "rcqapi.h"

class PluginManager final : public Service
{
private:

    AppCore* c;
    IEventManager* em;
    IDataBus* db;

    std::vector<std::function<void()>> starters {};
    std::vector<std::function<void(float)>> updaters {};

    std::vector<path_string> plugin_paths {};
    std::unordered_map<std::string, Plugin> plugins {};

    void addPaths(std::vector<std::string> paths);
    void registerPlugin(std::vector<void*> pointers);
    void unregisterPlugin(std::string name);

public:

    PluginManager(AppCore* c);

    void start();
    void update(float deltaTime);

};

#endif // PLUGINMANAGER_H
