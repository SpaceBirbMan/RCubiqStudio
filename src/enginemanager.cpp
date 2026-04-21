#include "enginemanager.h"

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

    // Already loaded — just switch to it
    auto it = engines.find(path);
    if (it != engines.end()) {
        activeEnginePath = path;
        tickWrapper = [this, path]() { engines.at(path)->tick(); };
        acptr->getEventManager().sendMessage(AppMessage(name, "engine_ready", tickWrapper));
        std::cout << "[EngineManager] Switched to already-loaded engine: " << path << std::endl;
        return;
    }

    // Need to resolve and load
    pendingResolutionPath = path;
    LibMeta meta;
    meta.path = path;
    meta.func_names = {"create_engine"};
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_resolving_request", meta));
    std::cout << "[EngineManager] Resolving engine: " << path << std::endl;
}

void EngineManager::removeEngine(std::string path) {
    if (path.empty()) {
        std::cerr << "[EngineManager] removeEngine called with empty path\n";
        return;
    }

    // Destroy instance first (calls DLL code before library is unloaded)
    auto it = engines.find(path);
    if (it != engines.end()) {
        IModel* eng = it->second;
        engines.erase(it);  // remove from map before shutdown to prevent re-entry

        // shutdown() nulls the pipeline slot and waits for any in-progress tick.
        // It is safe to call from this (MessageProcessor) thread.
        eng->shutdown();

        // Actual delete (→ bgfx::shutdown) must run on the main thread.
        // ViewportWidget processes this in its timer callback and then sends unload_library.
        acptr->getEventManager().sendMessage(
            AppMessage(name, "schedule_engine_delete", std::make_pair(eng, path)));
        std::cout << "[EngineManager] Engine shutdown done; delete scheduled on main thread: " << path << std::endl;
    }

    enginesRegistry.erase(path);

    if (activeEnginePath == path) {
        activeEnginePath.clear();
        tickWrapper = nullptr;
        std::cout << "[EngineManager] Active engine cleared\n";
    }

    // Notify UI to remove the page
    acptr->getEventManager().sendMessage(AppMessage(name, "engine_ui_removed", path));
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

    engines[resolvedPath] = eng;
    activeEnginePath = resolvedPath;

    eng->test();
    try {
        std::shared_ptr<std::vector<RUI::UiPage>> rp2 = eng->getUiPages();
        std::cout << "[EngineManager] UI pages: " << rp2->size() << std::endl;
        acptr->getEventManager().sendMessage(AppMessage(name, "init_ui_eng", rp2));
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
    if (it != engines.end() && it->second) {
        it->second->update(b);
    }
}

void EngineManager::sendTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table) {
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second) {
        EngineMeta em;
        em.table = table;
        em.windowHandle = 0;
        it->second->setMeta(em);
    }
}

void EngineManager::sendDataBus(IDataBus* dbp) {
    auto it = engines.find(activeEnginePath);
    if (it != engines.end() && it->second) {
        it->second->setDataBus(dbp);
    }
}

void EngineManager::sendWinId(uintptr_t id) {
    auto it = engines.find(activeEnginePath);
    if (it == engines.end() || !it->second) {
        std::cerr << "[EngineManager] sendWinId: no active engine\n";
        return;
    }
    IModel* eng = it->second;

    EngineMeta b = EngineMeta();
    b.windowHandle = id;
    std::cout << "[EngineManager] WHI2: " << id << std::endl;
    eng->setMeta(b);

    tickWrapper = [this]() {
        auto i = engines.find(activeEnginePath);
        if (i != engines.end() && i->second) i->second->tick();
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
    if (it != engines.end() && it->second) {
        EngineMeta em;
        em.renderer = ptr;
        em.table = nullptr;
        em.windowHandle = 0;
        it->second->setMeta(em);
    }
}

void EngineManager::getActiveFrames() {
    this->acptr->getEventManager().sendMessage(AppMessage(name, "extract_render_queue", 0));
}