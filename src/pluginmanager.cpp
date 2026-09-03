#include "pluginmanager.h"
#include "appcore.h"

PluginManager::PluginManager(AppCore *c) : c(c) {

    this->setName("PluginManager");
    this->em = &c->getEventManager();
    this->db = c->getEventManager().getBusPtr();

    em->subscribe(this->getName(), "add_paths", &PluginManager::addPaths, this);
    em->subscribe(this->getName(), "resolving_respond", &PluginManager::registerPlugin, this);
}

void PluginManager::registerPlugin(std::vector<void*> pointers) {

    try {
        auto cp = (CreateNewPlugin)pointers[0];
        Plugin p = *cp(this->em, this->db);

        this->plugins[p.getName()] = std::move(p);
    } catch (std::exception e) {
        std::cerr << "[" + this->getName() + "] " + e.what();
    }

}

void PluginManager::addPaths(std::vector<std::string> paths) {

    for (path_string p : this->plugin_paths) {
        for (int i = 0; i < paths.size(); i++) {
            if (paths[i] == p) {
                auto iter = paths.begin();
                paths.erase(iter + i);
            }
        }
    }

    if (!paths.empty()) {
        em->sendMessage(AppMessage(this->getName(), "resolving_request", paths));
    }
}


void PluginManager::start() {
    for (const auto& s : starters) {
        s();
    }
}

void PluginManager::update(float deltaTime) {
    for (const auto& u : updaters) {
        u(deltaTime);
    }
}
