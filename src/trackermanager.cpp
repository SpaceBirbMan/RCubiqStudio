#include "trackermanager.h"
#include "appshutdown.h"
#include "jsonutil.h"
#include "consts.h"
#include "pluginmeta.h"
#include "hostinput.h"

TrackerManager::TrackerManager(AppCore* core)
    : controlTableHost_(core->getEventManager().getBusPtr())
    , log_(core->getEventManager().getLogger().registerModule(name))
{
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
    this->core->getEventManager().subscribe(name, "stop_tracker", &TrackerManager::onStopTracker, this);
    this->core->getEventManager().subscribe(name, "tracker_ui_entry_ready", &TrackerManager::syncTrackerUiAfterEntry, this);
    this->core->getEventManager().subscribe(name, TrackerEvents::kControlTableRegister,
                                            &TrackerManager::onControlTableRegister, this);
}

void TrackerManager::publishControlTable(const std::string& sourcePath,
                                         std::vector<std::string> added,
                                         std::vector<std::string> removed)
{
    auto* table = controlTableHost_.table();
    if (!table)
        return;

    ControlTableUpdate update;
    update.table = table;
    update.sourceTrackerPath = sourcePath;
    update.addedKeys = std::move(added);
    update.removedKeys = std::move(removed);
    update.keySources = controlTableHost_.keySources();

    log_.info("control_table_updated: source=" + sourcePath
              + " added=" + std::to_string(update.addedKeys.size())
              + " removed=" + std::to_string(update.removedKeys.size())
              + " total=" + std::to_string(table->size()));

    if (isApplicationShuttingDown())
        return;

    core->getEventManager().sendMessage(
        AppMessage(name, AppLifecycleEvents::kControlTableUpdated, std::move(update)));
    // Legacy topic — same host table pointer for UI / EngineManager.
    core->getEventManager().sendMessage(AppMessage(name, "send_table", table));
}

void TrackerManager::mergeTrackerControls(const std::string& path, ITracker* instance)
{
    std::vector<std::string> added;
    controlTableHost_.mergeTracker(path, trackerDisplayName(path), instance, added);
    publishControlTable(path, std::move(added), {});
}

void TrackerManager::removeTrackerControls(const std::string& path)
{
    std::vector<std::string> removed;
    controlTableHost_.removeTracker(path, removed);
    if (!removed.empty())
        publishControlTable(path, {}, std::move(removed));
}

void TrackerManager::rebuildControlTableFromAllTrackers()
{
    std::vector<std::string> removed;
    controlTableHost_.clearAll(removed);

    std::vector<std::string> added;
    for (const auto& [path, data] : trackers) {
        if (data.instance)
            controlTableHost_.mergeTracker(path, trackerDisplayName(path), data.instance, added);
    }
    publishControlTable("", std::move(added), std::move(removed));
}

void TrackerManager::onControlTableRegister(ControlTableRegister reg)
{
    if (reg.trackerPath.empty()) {
        log_.warn("control_table_register: empty tracker path");
        return;
    }
    std::vector<std::string> added;
    controlTableHost_.merge(reg.trackerPath, trackerDisplayName(reg.trackerPath), reg.entries, added);
    publishControlTable(reg.trackerPath, std::move(added), {});
}

void TrackerManager::requestTrackerResolve(const std::string& path)
{
    pendingResolutionPath = path;
    Meta meta;
    meta.path = path;
    meta.func_names = {"create", "destroy"};
    core->getEventManager().sendMessage(AppMessage(name, "tracking_resolving_request", meta));
    log_.info("resolving tracker: " + path);
}

void TrackerManager::advanceResolutionQueue()
{
    if (!pendingResolutionQueue_.empty()) {
        const std::string next = pendingResolutionQueue_.front();
        pendingResolutionQueue_.pop_front();
        requestTrackerResolve(next);
        return;
    }
    pendingResolutionPath.clear();
}

TrackerManager::~TrackerManager() {
    for (auto& [path, data] : trackers) {
        if (data.instance) {
            if (data.instance->isRunning()) data.instance->stop();
            data.instance->shutdown();
            if (data.destroy) {
                data.destroy(data.instance);
            } else {
                delete data.instance;
            }
        }
    }
    trackers.clear();
}

void TrackerManager::onStopTracker() {
    log_.info("stop_tracker: begin (" + std::to_string(trackers.size()) + " loaded)");
    std::vector<std::string> removed;
    controlTableHost_.clearAll(removed);
    if (!isApplicationShuttingDown() && !removed.empty())
        publishControlTable("", {}, std::move(removed));
    if (!isApplicationShuttingDown()) {
        core->getEventManager().sendMessage(
            AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));
    }
    for (auto& [path, data] : trackers) {
        if (!data.instance) continue;
        log_.info("stop_tracker: stopping " + path);
        if (data.instance->isRunning()) data.instance->stop();
        data.instance->shutdown();
        log_.info("stop_tracker: shutdown done " + path);
    }
    log_.info("stop_tracker: complete");
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
    if (data.contains("trackersRegistry"))
        fillStringSetFromJsonArray(data["trackersRegistry"], trackersRegistry);
    else
        trackersRegistry.clear();

    if (data.contains("activeTrackerPaths"))
        fillStringSetFromJsonArray(data["activeTrackerPaths"], activeTrackerPaths);
    else
        activeTrackerPaths.clear();
}

void TrackerManager::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    std::vector<std::string> tmp_names {};
    for (const std::string& n : trackersRegistry) {
        tmp_names.emplace_back(n);
    }
    core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", tmp_names));
    core->getEventManager().sendMessage(AppMessage(name, "module_initialized", name));

    for (const std::string& path : activeTrackerPaths) {
        core->getEventManager().sendMessage(AppMessage(name, "activate_tracker_by_path", path));
    }
}

void TrackerManager::preInitialize() {
    std::vector<std::string> added;
    hostInput_.install(controlTableHost_);
    publishControlTable(HostInputControllers::kOwnerPath, std::move(added), {});
    core->getEventManager().getBusPtr()->registerData("host_input", &hostInput_);

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
    for (auto& [path, data] : trackers) {
        if (data.instance && !data.instance->isRunning()) {
            data.instance->start();
            std::cout << "[TrackerManager] Started: " << path << std::endl;
        }
    }
}

void TrackerManager::stopTracking() {
    for (auto& [path, data] : trackers) {
        if (data.instance && data.instance->isRunning()) {
            data.instance->stop();
            std::cout << "[TrackerManager] Stopped: " << path << std::endl;
        }
    }
}

bool TrackerManager::isRunning() const {
    for (const auto& [path, data] : trackers) {
        if (data.instance && data.instance->isRunning()) return true;
    }
    return false;
}

void TrackerManager::addNames(std::vector<std::string> names) {
    for (const std::string& path : names) {
        trackersRegistry.insert(path);
        const PluginDisplayMeta meta = readPluginDisplayMeta(path);
        trackerDisplayNames_[path] = meta.displayName;

        PluginUIInfo info;
        info.path = path;
        info.name = meta.displayName;
        info.description = meta.description;
        info.type = PluginUIType::Tracker;
        core->getEventManager().sendMessage(AppMessage(name, "tracker_ui_ready", info));
        log_.info("tracker ui_ready: " + meta.displayName);
    }
}

std::string TrackerManager::trackerDisplayName(const std::string& path) const
{
    const auto it = trackerDisplayNames_.find(path);
    if (it != trackerDisplayNames_.end())
        return it->second;
    return readPluginDisplayMeta(path).displayName;
}

void TrackerManager::tryAutoStartTracker(const std::string& path, ITracker* instance) {
    if (!instance || !activeTrackerPaths.count(path))
        return;
    if (!instance->startTrackingOnStartup() || instance->isRunning())
        return;
    instance->start();
    log_.info("auto-start tracker on plugin load: " + path);
}

void TrackerManager::activateTrackerByPath(std::string path) {
    if (path.empty()) {
        log_.warn("activateTrackerByPath called with empty path");
        return;
    }

    activeTrackerPaths.insert(path);

    // Already loaded — merge controls and optionally start
    auto it = trackers.find(path);
    if (it != trackers.end()) {
        mergeTrackerControls(path, it->second.instance);
        core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsRestore, 0));
        core->getEventManager().sendMessage(AppMessage(name, "tracker_set_active", path));
        log_.info("reactivated loaded tracker: " + path);
        tryAutoStartTracker(path, it->second.instance);
        return;
    }

    // Need to resolve and load (queue if another resolve is in flight)
    if (pendingResolutionPath.empty())
        requestTrackerResolve(path);
    else if (pendingResolutionPath != path)
        pendingResolutionQueue_.push_back(path);
}

void TrackerManager::deactivateTrackerByPath(std::string path) {
    log_.info("deactivate_tracker_by_path: begin " + path);

    auto it = trackers.find(path);
    if (it == trackers.end() || !it->second.instance) {
        activeTrackerPaths.erase(path);
        core->getEventManager().sendMessage(AppMessage(name, "tracker_set_inactive", path));
        return;
    }

    // Drop dangling pointers before unload — other trackers keep their keys.
    removeTrackerControls(path);
    core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));

    core->persistPluginsAndWriteSessionCache(path);

    TrackerData data = it->second;
    trackers.erase(it);

    if (data.instance->isRunning()) {
        log_.info("deactivate: stopping " + path);
        data.instance->stop();
    }
    log_.info("deactivate: shutdown " + path);
    data.instance->shutdown();
    if (data.destroy) {
        data.destroy(data.instance);
    } else {
        delete data.instance;
    }
    log_.info("deactivate: instance destroyed " + path);

    activeTrackerPaths.erase(path);
    core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kPluginRuntimeTeardown, path));
    core->getEventManager().sendMessage(AppMessage(name, "unload_library", path));
    core->getEventManager().sendMessage(AppMessage(name, "tracker_set_inactive", path));
    core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsRestore, 0));
    log_.info("deactivate_tracker_by_path: complete " + path);
}

void TrackerManager::removeTracker(std::string path) {
    if (path.empty()) {
        log_.warn("removeTracker called with empty path");
        return;
    }

    removeTrackerControls(path);
    core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));

    core->persistPluginsAndWriteSessionCache(path);

    auto it = trackers.find(path);
    if (it != trackers.end()) {
        TrackerData data = it->second;
        trackers.erase(it);
        if (data.instance->isRunning()) data.instance->stop();
        data.instance->shutdown();
        if (data.destroy) {
            data.destroy(data.instance);
        } else {
            delete data.instance;
        }
        log_.info("tracker instance destroyed: " + path);
    }

    trackersRegistry.erase(path);
    activeTrackerPaths.erase(path);
    // Сначала снять Qt-виджеты с колбэками в DLL; иначе после FreeLibrary ~std::function в лямбдах даёт SIGSEGV.
    core->getEventManager().sendMessage(AppMessage(name, "tracker_ui_removed", path));
    core->getEventManager().sendMessage(AppMessage(name, "unload_library", path));
}

void TrackerManager::activateTracker(std::vector<void*> pointers) {

    if (pendingResolutionPath.empty()) {
        log_.warn("activateTracker called but pendingResolutionPath is empty");
        return;
    }

    const std::string failedPath = pendingResolutionPath;

    if (pointers.empty()) {
        log_.error("activateTracker: no function pointers for " + failedPath);
        activeTrackerPaths.erase(failedPath);
        advanceResolutionQueue();
        return;
    }

    // Only 'create' (index 0) is mandatory; 'destroy' (index 1+) may be nullptr
    if (pointers[0] == nullptr) {
        log_.error("activateTracker: mandatory create pointer is null for " + failedPath);
        activeTrackerPaths.erase(failedPath);
        advanceResolutionQueue();
        return;
    }

    auto c = reinterpret_cast<CreateTracker>(pointers[0]);
    auto d = (pointers.size() > 1) ? reinterpret_cast<DestroyTracker>(pointers[1]) : nullptr;

    if (!c) {
        log_.error("activateTracker: invalid create pointer for " + failedPath);
        activeTrackerPaths.erase(failedPath);
        advanceResolutionQueue();
        return;
    }

    ITracker* tracker = c(&(core->getEventManager()), core->getEventManager().getBusPtr());
    if (!tracker) {
        log_.error("activateTracker: tracker creation failed for " + failedPath);
        activeTrackerPaths.erase(failedPath);
        advanceResolutionQueue();
        return;
    }

    std::string resolvedPath = pendingResolutionPath;
    const PluginDisplayMeta meta = readPluginDisplayMeta(resolvedPath);
    trackerDisplayNames_[resolvedPath] = meta.displayName;

    trackers[resolvedPath] = TrackerData{tracker, d};
    tracker->setLibraryPath(resolvedPath);
    mergeTrackerControls(resolvedPath, tracker);
    // Запуск — только по кнопке Start в UI трекера или при повторном activate_tracker_by_path.
  core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsRestore, 0));
    core->getEventManager().sendMessage(AppMessage(name, "tracker_set_active", resolvedPath));
    log_.info("tracker activated: " + resolvedPath);

    tryAutoStartTracker(resolvedPath, tracker);
    advanceResolutionQueue();
}

void TrackerManager::resendTrackerTables() {
    rebuildControlTableFromAllTrackers();
    if (!trackers.empty())
        core->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsRestore, 0));
}

void TrackerManager::syncTrackerUiAfterEntry(std::string path) {
    if (path.empty())
        return;

    auto it = trackers.find(path);
    if (it != trackers.end() && it->second.instance)
        mergeTrackerControls(path, it->second.instance);

    if (activeTrackerPaths.count(path))
        core->getEventManager().sendMessage(AppMessage(name, "tracker_set_active", path));
}