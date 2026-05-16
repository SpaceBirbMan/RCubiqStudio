#include "enginemanager.h"
#include "consts.h"

#ifdef _WIN32
#include "spoutsender.h"
#endif

EngineManager::EngineManager(AppCore* acptr) {
    this->acptr = acptr;

    acptr->getEventManager().subscribe(name, "initialize", &EngineManager::initialize, this);
    acptr->getEventManager().subscribe(name, "pre_initialize", &EngineManager::preInitialize, this);
    acptr->getEventManager().subscribe(name, "engine_resolving_respond", &EngineManager::activateEngine, this);
    acptr->getEventManager().subscribe(name, "set_data", &EngineManager::deserializeCache, this);
    acptr->getEventManager().subscribe(name, "start_drawing_frames", &EngineManager::getActiveFrames, this);
    acptr->getEventManager().subscribe(name, "add_engines_names", &EngineManager::addNames, this);
    acptr->getEventManager().subscribe(name, "send_table", &EngineManager::sendTrackerTable, this);
    acptr->getEventManager().subscribe(name, "send_win_id", &EngineManager::sendWinId, this);
    acptr->getEventManager().subscribe(name, "send_vp", &EngineManager::sendViewport, this);
    acptr->getEventManager().subscribe(name, "send_dbus_e", &EngineManager::sendDataBus, this);
    acptr->getEventManager().subscribe(name, "activate_engine_by_path", &EngineManager::activateEngineByPath, this);
    acptr->getEventManager().subscribe(name, "deactivate_engine_by_path", &EngineManager::deactivateEngineByPath, this);
    acptr->getEventManager().subscribe(name, "remove_engine", &EngineManager::removeEngine, this);
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
        // Insert into registry (idempotent — set ignores duplicates)
        enginesRegistry.insert(path);

        // Always notify UI; uiAddPluginEntry has its own duplicate guard
        PluginUIInfo info;
        info.path = path;
        info.name = path;
        info.type = PluginUIType::Engine;
        acptr->getEventManager().sendMessage(AppMessage(name, "engine_ui_ready", info));
        std::cout << "[EngineManager] Engine ui_ready sent: " << path << std::endl;
    }
    acptr->getEventManager().sendMessage(AppMessage(name, "added_names", names.size()));
}

void EngineManager::activateEngineByPath(std::string path) {
    if (path.empty()) {
        std::cerr << "[EngineManager] activateEngineByPath called with empty path\n";
        return;
    }

    // Already loaded — switch tick/UI (test() + win id refresh republish init_ui_eng)
    auto it = engines.find(path);
    if (it != engines.end()) {
        activeEnginePath = path;
        tickWrapper = [this, path]() { engines.at(path).instance->tick(); };
        try {
            it->second.instance->test();
        } catch (...) {
            std::cout << "[EngineManager] test() on re-activate failed\n";
        }
        acptr->getEventManager().sendMessage(AppMessage(name, "get_win_id", 0));
        std::cout << "[EngineManager] Switched to already-loaded engine: " << path << std::endl;
        return;
    }

    // Need to resolve and load
    pendingResolutionPath = path;
    LibMeta meta;
    meta.path = path;
    meta.func_names = {"create_engine", "destroy_engine"};
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_resolving_request", meta));
    std::cout << "[EngineManager] Resolving engine: " << path << std::endl;
}

void EngineManager::deactivateEngineByPath(std::string path) {
    if (path.empty()) {
        std::cerr << "[EngineManager] deactivateEngineByPath called with empty path\n";
        return;
    }

    acptr->persistPluginsAndWriteSessionCache(path);

    acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kStreamBindingsInvalidate, 0));

    auto it = engines.find(path);
    if (it != engines.end()) {
        EngineData data = it->second;
        engines.erase(it);

#ifdef _WIN32
        spoutNotifyEngineDeviceReset();
#endif
        data.instance->shutdown();

        // Payload MUST be std::pair<IModel*, std::string> — ViewportWidget subscribes with that exact type;
        // a local struct would never match typeid and deletes (bgfx::shutdown) would never run.
        acptr->getEventManager().sendMessage(AppMessage(
            name, "schedule_engine_delete",
            std::make_pair(data.instance, path)));
        std::cout << "[EngineManager] Engine deactivated; delete scheduled: " << path << std::endl;
    }

    if (activeEnginePath == path) {
        activeEnginePath.clear();
        tickWrapper = nullptr;
        std::cout << "[EngineManager] Active engine cleared (deactivate)\n";
    }

    acptr->getEventManager().sendMessage(AppMessage(name, AppLifecycleEvents::kPluginRuntimeTeardown, path));
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_set_inactive", path));
    acptr->getEventManager().sendMessage(AppMessage(name, "clear_viewport_surface", 0));
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
        acptr->getEventManager().sendMessage(
            AppMessage(name, "schedule_engine_delete",
                       std::make_pair(data.instance, path)));
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

    acptr->getEventManager().sendMessage(AppMessage(name, "get_win_id", 0));
}

void EngineManager::sendViewport(ViewportWidget* vp) {
    this->acptr->getEventManager().getDirectSender().subscribe(this, this->viewport, &EngineManager::resize);
    acptr->getEventManager().sendMessage(AppMessage(name, "get_rec", this));
}

void EngineManager::resize(ViewportBus b) {
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