#include "trackermanager.h"

TrackerManager::TrackerManager(AppCore* core) {
    this->core = core;

    this->core->getEventManager().subscribe(name, "initialize", &TrackerManager::initialize, this);
    this->core->getEventManager().subscribe(name, "pre_initialize", &TrackerManager::preInitialize, this);
    this->core->getEventManager().subscribe(name, "tracker_resolving_respond", &TrackerManager::activateTracker, this);
    this->core->getEventManager().subscribe(name, "set_data", &TrackerManager::deserializeCache, this);
    this->core->getEventManager().subscribe(name, "add_trackers_names", &TrackerManager::addNames, this);
    this->core->getEventManager().subscribe(name, "activate_tracker_by_path", &TrackerManager::activateTrackerByPath, this);
    this->core->getEventManager().subscribe(name, "deactivate_tracker_by_path", &TrackerManager::deactivateTrackerByPath, this);
    this->core->getEventManager().subscribe(name, "remove_tracker", &TrackerManager::removeTracker, this);
    this->core->getEventManager().subscribe(name, "request_tracker_table_resend", &TrackerManager::resendTrackerTables, this);
}

TrackerManager::~TrackerManager() {
    for (auto& [path, tracker] : trackers) {
        if (tracker) {
            if (tracker->isRunning()) tracker->stop();
            delete tracker;
        }
    }
    trackers.clear();
}

std::string TrackerManager::cacheKey() const {
    return name;
}

nlohmann::json TrackerManager::serializeCache() const {
    nlohmann::json j = {
        {"trackersRegistry", trackersRegistry},
        {"activeTrackerPaths", activeTrackerPaths}
    };
    return j;
}

void TrackerManager::deserializeCache(const nlohmann::json& data) {
    trackersRegistry = data["trackersRegistry"];
    if (data.contains("activeTrackerPaths"))
        activeTrackerPaths = data["activeTrackerPaths"];
}

void TrackerManager::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    std::vector<std::string> tmp_names {};
    for (const std::string& n : trackersRegistry) {
        tmp_names.emplace_back(n);
    }
    core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", tmp_names));
    core->getEventManager().sendMessage(AppMessage(name, "module_initialized", name));

    // Restore previously active trackers
    for (const std::string& path : activeTrackerPaths) {
        core->getEventManager().sendMessage(AppMessage(name, "activate_tracker_by_path", path));
    }
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

void TrackerManager::startTracking() {
    for (auto& [path, tracker] : trackers) {
        if (tracker && !tracker->isRunning()) {
            tracker->start();
            std::cout << "[TrackerManager] Started: " << path << std::endl;
        }
    }
}

void TrackerManager::stopTracking() {
    for (auto& [path, tracker] : trackers) {
        if (tracker && tracker->isRunning()) {
            tracker->stop();
            std::cout << "[TrackerManager] Stopped: " << path << std::endl;
        }
    }
}

bool TrackerManager::isRunning() const {
    for (const auto& [path, tracker] : trackers) {
        if (tracker && tracker->isRunning()) return true;
    }
    return false;
}

void TrackerManager::addNames(std::vector<std::string> names) {
    for (const std::string& path : names) {
        // Always insert (set is idempotent) - registry dedup happens here
        trackersRegistry.insert(path);

        // Always notify UI - uiAddPluginEntry has its own duplicate guard
        PluginUIInfo info;
        info.path = path;
        info.name = path;
        info.type = PluginUIType::Tracker;
        core->getEventManager().sendMessage(AppMessage(name, "tracker_ui_ready", info));
        std::cout << "[TrackerManager] Tracker ui_ready sent: " << path << std::endl;
    }
}

void TrackerManager::activateTrackerByPath(std::string path) {
    if (path.empty()) {
        std::cerr << "[TrackerManager] activateTrackerByPath called with empty path\n";
        return;
    }

    // Already loaded — just start it
    auto it = trackers.find(path);
    if (it != trackers.end()) {
        if (!it->second->isRunning()) {
            it->second->start();
            core->getEventManager().sendMessage(AppMessage(name, "send_table", it->second->getTable()));
            core->getEventManager().sendMessage(AppMessage(name, "tracker_set_active", path));
        }
        return;
    }

    // Need to resolve and load
    activeTrackerPaths.insert(path);
    pendingResolutionPath = path;
    Meta meta;
    meta.path = path;
    meta.func_names = {"create"};
    core->getEventManager().sendMessage(AppMessage(name, "tracking_resolving_request", meta));
    std::cout << "[TrackerManager] Resolving tracker: " << path << std::endl;
}

void TrackerManager::deactivateTrackerByPath(std::string path) {
    auto it = trackers.find(path);
    if (it != trackers.end() && it->second) {
        if (it->second->isRunning()) {
            it->second->stop();
            std::cout << "[TrackerManager] Deactivated tracker: " << path << std::endl;
        }
    }
    activeTrackerPaths.erase(path);
    core->getEventManager().sendMessage(AppMessage(name, "tracker_set_inactive", path));
}

void TrackerManager::removeTracker(std::string path) {
    if (path.empty()) {
        std::cerr << "[TrackerManager] removeTracker called with empty path\n";
        return;
    }

    auto it = trackers.find(path);
    if (it != trackers.end()) {
        if (it->second->isRunning()) it->second->stop();
        it->second->shutdown();
        delete it->second;
        trackers.erase(it);
        std::cout << "[TrackerManager] Tracker instance destroyed: " << path << std::endl;
    }

    trackersRegistry.erase(path);
    activeTrackerPaths.erase(path);
    core->getEventManager().sendMessage(AppMessage(name, "unload_library", path));
    core->getEventManager().sendMessage(AppMessage(name, "tracker_ui_removed", path));
}

void TrackerManager::activateTracker(std::vector<void*> pointers) {

    if (pendingResolutionPath.empty()) {
        std::cerr << "[TrackerManager] activateTracker called but pendingResolutionPath is empty\n";
        return;
    }

    if (pointers.empty()) {
        std::cerr << "[TrackerManager] No function pointers provided\n";
        pendingResolutionPath.clear();
        return;
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        if (pointers[i] == nullptr) {
            std::cerr << "[TrackerManager] Pointer at index " << i << " is null\n";
            pendingResolutionPath.clear();
            return;
        }
    }

    auto c = reinterpret_cast<CreateTracker>(pointers[0]);
    if (!c) {
        std::cerr << "[TrackerManager] Invalid 'create' pointer\n";
        pendingResolutionPath.clear();
        return;
    }

    ITracker* tracker = c(&(core->getEventManager()), core->getEventManager().getBusPtr());
    if (!tracker) {
        std::cerr << "[TrackerManager] Tracker creation failed\n";
        pendingResolutionPath.clear();
        return;
    }

    std::string resolvedPath = pendingResolutionPath;
    pendingResolutionPath.clear();

    trackers[resolvedPath] = tracker;
    tracker->start();
    core->getEventManager().sendMessage(AppMessage(name, "send_table", tracker->getTable()));
    core->getEventManager().sendMessage(AppMessage(name, "tracker_set_active", resolvedPath));
    std::cout << "[TrackerManager] Tracker activated: " << resolvedPath << std::endl;
}

void TrackerManager::resendTrackerTables() {
    for (auto& [path, tracker] : trackers) {
        if (tracker && tracker->isRunning()) {
            core->getEventManager().sendMessage(AppMessage(name, "send_table", tracker->getTable()));
        }
    }
}