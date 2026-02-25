#include "trackermanager.h"

TrackerManager::TrackerManager(AppCore* core) {
    this->core = core;

    this->core->getEventManager().subscribe(name, "initialize", &TrackerManager::initialize, this);
    this->core->getEventManager().subscribe(name, "pre_initialize", &TrackerManager::preInitialize, this);
    this->core->getEventManager().subscribe(name, "tracker_resolving_respond", &TrackerManager::activateTracker, this);
    this->core->getEventManager().subscribe(name, "set_data", &TrackerManager::deserializeCache, this);
    this->core->getEventManager().subscribe(name, "add_trackers_names", &TrackerManager::addNames, this);
}

std::string TrackerManager::cacheKey() const  {
    return name;
}

//TODO: JSON выпилить в отдельный класс, чтобы не тянуть зависимость. В классе сделать fallback
nlohmann::json TrackerManager::serializeCache() const {
    nlohmann::json j = {
        {"trackersRegistry", trackersRegistry}
    };
    return j;
}

void TrackerManager::deserializeCache(const nlohmann::json& data) {
    trackersRegistry = data["trackersRegistry"];
}

// FIXME: Повторение кода в initialize и preInitialize тут и в EngineManager
void TrackerManager::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    std::vector<std::string> tmp_names {};
    for (std::string name : trackersRegistry) {
        tmp_names.emplace_back(name);
    }
    core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", tmp_names));
}

void TrackerManager::preInitialize() {
    auto deserialize_lambda = [this](const nlohmann::json& data) { this->deserializeCache(data); };
    auto serialize_lambda = [this]() -> json { return this->serializeCache(); };
    std::function<void(const nlohmann::json&)> deserialize_wrapper = deserialize_lambda;
    std::function<json()> serialize_wrapper = serialize_lambda;

    cacheForm cf_instance;
    cf_instance.name = name;
    cf_instance.desfn = deserialize_wrapper;
    cf_instance.sefn = serialize_wrapper;

    core->getEventManager().sendMessage(AppMessage(name, "sub_to_cache", cf_instance));
    core->getEventManager().sendMessage(AppMessage(name, "module_subscribed", name));
}

void TrackerManager::activateTracker(std::vector<void*> pointers) {

    if (pointers.empty()) {
        std::cerr << "No function pointers provided";
        return;
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        if (pointers[i] == nullptr) {
            std::cerr << "Pointer at index " << i << " is null";
            return;
        }
    }

    if (pointers.size() < 1 || pointers[0] == nullptr) {
        std::cerr << "Invalid tracker parameters";
        return;
    }

    auto c = reinterpret_cast<CreateTracker>(pointers[0]);
    if (!c) {
        std::cerr << "Invalid Create pointer";
        return;
    }

    tracker = c();
    if (!tracker) {
        std::cerr << "Engine creation failed";
        return;
    }

    tracker->start();

}

void TrackerManager::addNames(std::vector<std::string> names) {
    for (std::string name: names) {
        this->trackersRegistry.insert(name);
    }

    core->getEventManager().sendMessage(AppMessage(name, "update_trackers_combo", trackersRegistry));
}
