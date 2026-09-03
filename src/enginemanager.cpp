#include "enginemanager.h"
#include "consts.h"
#include "pluginmeta.h"
#include <chrono>
#include <thread>

#ifdef _WIN32
#include "spoutsender.h"
#endif

EngineManager::EngineManager(AppCore* acptr)
    : log_(acptr->getEventManager().getLogger().registerModule(name))
{
    this->acptr = acptr;

    acptr->getEventManager().subscribe(name, "initialize", &EngineManager::initialize, this);
    acptr->getEventManager().subscribe(name, "pre_initialize", &EngineManager::preInitialize, this);
    acptr->getEventManager().subscribe(name, "engine_resolving_respond", &EngineManager::activateEngine, this);
    acptr->getEventManager().subscribe(name, "set_data", &EngineManager::deserializeCache, this);
    acptr->getEventManager().subscribe(name, "start_drawing_frames", &EngineManager::getActiveFrames, this);
    acptr->getEventManager().subscribe(name, "add_engines_names", &EngineManager::addNames, this);
    acptr->getEventManager().subscribe(name, "send_table", &EngineManager::sendTrackerTable, this);
    acptr->getEventManager().subscribe(name, AppLifecycleEvents::kControlTableUpdated,
                                       &EngineManager::onControlTableUpdated, this);
    acptr->getEventManager().subscribe(name, "send_win_id", &EngineManager::sendWinId, this);
    acptr->getEventManager().subscribe(name, "send_vp", &EngineManager::sendViewport, this);
    acptr->getEventManager().subscribe(name, "send_dbus_e", &EngineManager::sendDataBus, this);
    acptr->getEventManager().subscribe(name, "activate_engine_by_path", &EngineManager::activateEngineByPath, this);
    acptr->getEventManager().subscribe(name, "deactivate_engine_by_path", &EngineManager::deactivateEngineByPath, this);
    acptr->getEventManager().subscribe(name, "remove_engine", &EngineManager::removeEngine, this);
    acptr->getEventManager().subscribe(name, AppShutdownEvents::kStopEngineTick,
                                       &EngineManager::stopEngineTick, this);
    acptr->getEventManager().subscribe(name, AppShutdownEvents::kShutdownAllEngines,
                                       &EngineManager::shutdownAllEngines, this);
}

void EngineManager::stopEngineTick() {
    tickWrapper = nullptr;
    log_.info("stop_engine_tick: tick wrapper cleared");
}

void EngineManager::setFuncs(funcMap map) {}

void EngineManager::preInitialize() {

    auto deserialize_lambda = [this](const nlohmann::json& data) { this->deserializeCache(data); };
    auto serialize_lambda = [this]() -> json { return this->serializeCache(); };
    std::function<void(const nlohmann::json&)> deserialize_wrapper = deserialize_lambda;
    std::function<json()> serialize_wrapper = serialize_lambda;

    cacheForm cf_instance;
    cf_instance.name = name;
    cf_instance.desfn = deserialize_wrapper;
    cf_instance.sefn = serialize_wrapper;

    acptr->getEventManager().sendMessage(AppMessage(name, "sub_to_cache", cf_instance));
    acptr->getEventManager().sendMessage(AppMessage(name, "module_subscribed", name));
}

void EngineManager::initialize() {
    acptr->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    std::vector<std::string> tmp_names {};
    for (const std::string& n : enginesRegistry) {
        tmp_names.emplace_back(n);
    }
    acptr->getEventManager().sendMessage(AppMessage(name, "add_engines_names", tmp_names));
    acptr->getEventManager().sendMessage(AppMessage(name, "module_initialized", name));
    acptr->getEventManager().sendMessage(AppMessage(name, "build_gui", 0));

    // Restore previously active engine
    if (!activeEnginePath.empty() && enginesRegistry.count(activeEnginePath)) {
        acptr->getEventManager().sendMessage(AppMessage(name, "activate_engine_by_path", activeEnginePath));
    }
}

void EngineManager::addNames(std::vector<std::string> names) {
    for (const std::string& path : names) {
        enginesRegistry.insert(path);

        const PluginDisplayMeta meta = readPluginDisplayMeta(path);
        PluginUIInfo info;
        info.path = path;
        info.name = meta.displayName;
        info.description = meta.description;
        info.type = PluginUIType::Engine;
        acptr->getEventManager().sendMessage(AppMessage(name, "engine_ui_ready", info));
        log_.info("engine ui_ready: " + meta.displayName);
    }
    acptr->getEventManager().sendMessage(AppMessage(name, "added_names", names.size()));
}

void EngineManager::postStatus(const std::string& text) {
    acptr->getEventManager().sendMessage(
        AppMessage("Core", AppUiEvents::kStatusMessage, text));
}

void EngineManager::flushPendingEngineDeletes() {
    log_.info("flush_pending_engine_deletes requested (thread="
              + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ")");
    acptr->getEventManager().dispatchImmediately(
        AppMessage("ViewportWidget", "flush_engine_deletes", 0));
    log_.info("flush_pending_engine_deletes done");
}

void EngineManager::deactivateOtherEnginesExcept(const std::string& keepPath) {
    std::vector<std::string> toStop;
    toStop.reserve(engines.size());
    for (const auto& [p, _] : engines) {
        if (p != keepPath)
            toStop.push_back(p);
    }
    for (const auto& p : toStop) {
        log_.info("switch: deactivating other loaded engine: " + p);
        deactivateEngineByPath(p);
    }
}

void EngineManager::shutdownAllEngines() {
    log_.info("shutdown_all_engines: begin");
    if (!activeEnginePath.empty()) {
        log_.info("shutdown: deactivating active engine: " + activeEnginePath);
        deactivateEngineByPathImpl(activeEnginePath, false);
    }
    std::vector<std::string> loaded;
    loaded.reserve(engines.size());
    for (const auto& [p, _] : engines)
        loaded.push_back(p);
    for (const auto& p : loaded) {
        log_.info("shutdown: deactivating loaded engine: " + p);
        deactivateEngineByPathImpl(p, false);
    }
    pendingResolutionPath.clear();
    acptr->getEventManager().drainPendingMessages();
    flushPendingEngineDeletes();
    log_.info("shutdown_all_engines: complete");
    postStatus("");
}

void EngineManager::activateEngineByPath(std::string path) {
    if (path.empty()) {
        log_.warn("activateEngineByPath called with empty path");
        return;
    }

    log_.info("activate_engine_by_path: " + path);
    postStatus("Loading engine: " + path);
    flushPendingEngineDeletes();
    deactivateOtherEnginesExcept(path);
    acptr->getEventManager().drainPendingMessages();
    flushPendingEngineDeletes();

    // Already loaded — switch tick/UI (test() + win id refresh republish init_ui_eng)
    auto it = engines.find(path);
    if (it != engines.end()) {
        activeEnginePath = path;
        tickWrapper = [this, path]() { engines.at(path).instance->tick(); };
        postStatus("Activating engine: " + path);
        try {
            it->second.instance->test();
        } catch (...) {
            log_.warn("test() on re-activate failed: " + path);
        }
        acptr->getEventManager().sendMessage(AppMessage(name, "get_win_id", 0));
        log_.info("switched to already-loaded engine: " + path);
        return;
    }

    // Need to resolve and load
    pendingResolutionPath = path;
    postStatus("Resolving engine DLL: " + path);
    log_.info("resolving engine: " + path);
    LibMeta meta;
    meta.path = path;
    meta.func_names = {"create_engine", "destroy_engine"};
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_resolving_request", meta));
}

void EngineManager::deactivateEngineByPath(std::string path) {
    deactivateEngineByPathImpl(std::move(path), true);
}

void EngineManager::deactivateEngineByPathImpl(std::string path, bool persistSession) {
    if (path.empty()) {
        log_.warn("deactivateEngineByPath called with empty path");
        return;
    }

    log_.info("deactivate_engine_by_path: begin " + path);
    postStatus("Unloading engine: " + path);

    log_.info("deactivate: stop render tick before engine shutdown");
    acptr->getEventManager().dispatchImmediately(
        AppMessage(name, AppShutdownEvents::kStopEngineTick, 0));

    if (persistSession) {
        log_.info("deactivate: persist session cache");
        acptr->persistPluginsAndWriteSessionCache(path);
        log_.info("deactivate: persist done");
    } else {
        log_.info("deactivate: skip persist (already saved at app shutdown)");
    }

    acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));
    log_.info("deactivate: stream bindings invalidated");

    auto it = engines.find(path);
    if (it != engines.end()) {
        EngineData data = it->second;
        engines.erase(it);
        log_.info("deactivate: erased from engines map");

#ifdef _WIN32
        spoutNotifyEngineDeviceReset();
#endif
        // plugin_runtime_teardown removes UI tabs while DLL is still mapped.
        log_.info("deactivate: plugin_runtime_teardown");
        acptr->getEventManager().dispatchImmediately(
            AppMessage(name, AppLifecycleEvents::kPluginRuntimeTeardown, path));

        log_.info("deactivate: control_table_updated subs before shutdown: "
            + acptr->getEventManager().subscriberReceiversForTopic(
                AppLifecycleEvents::kControlTableUpdated));
        log_.info("deactivate: calling instance->shutdown()");
        data.instance->shutdown();
        log_.info("deactivate: instance->shutdown() returned");
        log_.info("deactivate: control_table_updated subs after shutdown: "
            + acptr->getEventManager().subscriberReceiversForTopic(
                AppLifecycleEvents::kControlTableUpdated));

        // Payload MUST be std::pair<IModel*, std::string> — ViewportWidget subscribes with that exact type.
        acptr->getEventManager().dispatchImmediately(AppMessage(
            name, "schedule_engine_delete",
            std::make_pair(data.instance, path)));
        log_.info("engine instance shutdown, delete scheduled: " + path);
    } else {
        log_.info("deactivate: engine not loaded (registry only): " + path);
    }

    if (activeEnginePath == path) {
        activeEnginePath.clear();
        tickWrapper = nullptr;
        log_.info("active engine cleared");
    }

    acptr->getEventManager().sendMessage(AppMessage(name, "engine_set_inactive", path));

    acptr->getEventManager().drainPendingMessages();
    log_.info("deactivate: flush pending deletes");
    flushPendingEngineDeletes();
    acptr->getEventManager().dispatchImmediately(
        AppMessage(name, "clear_viewport_surface", 0));

    log_.info("deactivate_engine_by_path complete: " + path);
    postStatus("");
}

void EngineManager::removeEngine(std::string path) {
    if (path.empty()) {
        std::cerr << "[EngineManager] removeEngine called with empty path\n";
        return;
    }

    acptr->persistPluginsAndWriteSessionCache(path);

    acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));

    // Destroy instance first (calls DLL code before library is unloaded)
    auto it = engines.find(path);
    if (it != engines.end()) {
        EngineData data = it->second;
        engines.erase(it);  // remove from map before shutdown to prevent re-entry

#ifdef _WIN32
        spoutNotifyEngineDeviceReset();
#endif
        // shutdown() nulls the pipeline slot and waits for any in-progress tick.
        // It is safe to call from this (MessageProcessor) thread.
        data.instance->shutdown();

        // Actual delete (→ bgfx::shutdown) must run on the main thread.
        // ViewportWidget subscribes on std::pair<IModel*, std::string>; see deactivate_engine_by_path.
        acptr->getEventManager().dispatchImmediately(
            AppMessage(name, "schedule_engine_delete",
                       std::make_pair(data.instance, path)));
        flushPendingEngineDeletes();
        std::cout << "[EngineManager] Engine shutdown done; delete scheduled on main thread: " << path << std::endl;
    }

    enginesRegistry.erase(path);

    if (activeEnginePath == path) {
        activeEnginePath.clear();
        tickWrapper = nullptr;
        std::cout << "[EngineManager] Active engine cleared\n";
    }

    acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kPluginRuntimeTeardown, path));
    // Notify UI to remove the page (и вкладки — дубль с teardown, порядок как у gen_plugin)
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_ui_removed", path));
    acptr->getEventManager().sendMessage(AppMessage(name, "clear_viewport_surface", 0));
    acptr->getEventManager().drainPendingMessages();
    flushPendingEngineDeletes();
}

/**
 * Called when engine_resolving_respond arrives. Uses pendingResolutionPath to know which path this corresponds to.
 */
void EngineManager::activateEngine(std::vector<void*> pointers) {

    if (pendingResolutionPath.empty()) {
        std::cerr << "[EngineManager] activateEngine called but pendingResolutionPath is empty\n";
        return;
    }

    if (pointers.empty()) {
        std::cerr << "[EngineManager] No function pointers provided\n";
        pendingResolutionPath.clear();
        return;
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        if (pointers[i] == nullptr) {
            std::cerr << "[EngineManager] Pointer at index " << i << " is null\n";
            pendingResolutionPath.clear();
            return;
        }
    }

    auto ce = reinterpret_cast<CreateEngine>(pointers[0]);
    auto de = (pointers.size() > 1) ? reinterpret_cast<DestroyEngine>(pointers[1]) : nullptr;

    if (!ce) {
        std::cerr << "[EngineManager] Invalid CreateEngine pointer\n";
        pendingResolutionPath.clear();
        return;
    }

    IModel* eng = ce(&(acptr->getEventManager()), acptr->getEventManager().getBusPtr());
    if (!eng) {
        std::cerr << "[EngineManager] Engine creation failed\n";
        pendingResolutionPath.clear();
        return;
    }

    std::string resolvedPath = pendingResolutionPath;
    pendingResolutionPath.clear();

    engines[resolvedPath] = EngineData{eng, de};
    activeEnginePath = resolvedPath;

    eng->setLibraryPath(resolvedPath);
    try {
        eng->test();
    } catch (...) {
        std::cout << "[EngineManager] Engine test() failed\n";
    }
    try {
        auto rp2 = eng->getUiPages();
        std::cout << "[EngineManager] UI pages: " << rp2->size() << std::endl;
    } catch (...) {
        std::cout << "[EngineManager] getUiPages failed\n";
    }

    postStatus("Engine ready: " + resolvedPath);
    acptr->getEventManager().sendMessage(AppMessage(name, "get_win_id", 0));
}

void EngineManager::sendViewport(ViewportWidget* vp) {
    this->acptr->getEventManager().getDirectSender().subscribe(this, this->viewport, &EngineManager::resize);
    acptr->getEventManager().sendMessage(AppMessage(name, "get_rec", this));
}

void EngineManager::resize(ViewportBus b) {
    if (activeEnginePath.empty())
        return;
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second.instance) {
        it->second.instance->update(b);
    }
}

void EngineManager::sendTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table) {
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second.instance) {
        EngineMeta em;
        em.table = table;
        em.windowHandle = 0;
        it->second.instance->setMeta(em);
        acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsRestore, 0));
    }
}

void EngineManager::onControlTableUpdated(ControlTableUpdate update) {
    if (update.table)
        sendTrackerTable(update.table);
}

void EngineManager::sendDataBus(IDataBus* dbp) {
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second.instance) {
        it->second.instance->setDataBus(dbp);
    }
}

void EngineManager::sendWinId(uintptr_t id) {
    auto it = engines.find(activeEnginePath);
    if (it == engines.end() || !it->second.instance) {
        std::cerr << "[EngineManager] sendWinId: no active engine\n";
        return;
    }
    IModel* eng = it->second.instance;

    EngineMeta b = EngineMeta();
    b.windowHandle = id;
    std::cout << "[EngineManager] WHI2: " << id << std::endl;
    eng->setMeta(b);

    tickWrapper = [this]() {
        auto i = engines.find(activeEnginePath);
        if (i != engines.end() && i->second.instance) i->second.instance->tick();
    };

    acptr->getEventManager().sendMessage(AppMessage(name, "engine_ready", tickWrapper));
    acptr->getEventManager().sendMessage(AppMessage(
        name, "engine_init_render",
        std::function<void()>([eng]() { eng->initRender(); })));
    // Notify UI to check the active engine checkbox
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_set_active", activeEnginePath));
    // Request tracker tables to reconnect engine with active trackers
    acptr->getEventManager().sendMessage(AppMessage(name, "request_tracker_table_resend", 0));
}

void EngineManager::sendRenderer(IRenderer* ptr) {
    std::cout << "[EngineManager] RENDERER_SENT\n";
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second.instance) {
        EngineMeta em;
        em.renderer = ptr;
        em.table = nullptr;
        em.windowHandle = 0;
        it->second.instance->setMeta(em);
    }
}

void EngineManager::getActiveFrames() {
    this->acptr->getEventManager().sendMessage(AppMessage(name, "extract_render_queue", 0));
}