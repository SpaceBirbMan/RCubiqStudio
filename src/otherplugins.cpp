#include "otherplugins.h"
#include "appcore.h"

OtherPlugins::OtherPlugins(AppCore *appcore) {
    this->core = appcore;

    core->getEventManager().subscribe(name, "initialize", &OtherPlugins::initialize, this);
    core->getEventManager().subscribe(name, "pre_initialize", &OtherPlugins::preInitialize, this);
    core->getEventManager().subscribe(name, "plugin_resolving_respond", &OtherPlugins::registerPlugin, this);
    core->getEventManager().subscribe(name, "add_plugins_to_registry", &OtherPlugins::addPaths, this);
    core->getEventManager().subscribe(name, "send_dbus_p", &OtherPlugins::setDataBus, this);
    core->getEventManager().subscribe(name, "init_complete", &OtherPlugins::postInitialize, this);
    core->getEventManager().subscribe(name, "remove_gen_plugin", &OtherPlugins::deletePlugin, this);
    core->getEventManager().subscribe(name, "disable_gen_plugin", &OtherPlugins::disablePlugin, this);
    core->getEventManager().subscribe(name, "enable_gen_plugin", &OtherPlugins::enablePlugin, this);
}

void OtherPlugins::preInitialize() {

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

void OtherPlugins::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    core->getEventManager().sendMessage(AppMessage(name, "plugin_manager_ready", 0));
    core->getEventManager().sendMessage(AppMessage(name, "module_initialized", name));
}

void OtherPlugins::postInitialize() {
    std::cout << name << " POST\n";

    // Show ALL registered plugins in UI (loaded or not)
    for (const std::string& path : pluginsRegistry) {
        if (!activePluginPaths.count(path)) {
            // Plugin is registered but inactive — just notify UI with empty name (path as fallback)
            PluginUIInfo info;
            info.name = path;
            info.path = path;
            info.type = PluginUIType::Generic;
            core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_ui_ready", info));
        }
    }

    // Load only the active ones
    std::vector<std::string> active_paths(activePluginPaths.begin(), activePluginPaths.end());
    if (!active_paths.empty()) {
        core->getEventManager().sendMessage(AppMessage(name, "add_plugins_to_registry", active_paths));
    }
}

void OtherPlugins::addPaths(std::vector<std::string> paths) {
    for (const std::string& path : paths) {
        // Always add to registry (idempotent)
        this->pluginsRegistry.emplace(path);

        // If already loaded — just re-notify UI (UI deduplicates via pluginPageEntries)
        auto it = plugins.find(path);
        if (it != plugins.end()) {
            PluginUIInfo info;
            info.name = it->second->getName();
            info.path = path;
            info.type = PluginUIType::Generic;
            core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_ui_ready", info));
            std::cout << "[OtherPlugins] Plugin already loaded, re-notifying UI: " << path << std::endl;
            continue;
        }

        // Not yet loaded — resolve and load
        pendingPaths.push(path);
        Meta meta;
        meta.path = path;
        meta.func_names = {"create", "destroy"};
        core->getEventManager().sendMessage(AppMessage(name, "plugin_resolving_request", meta));
    }
}

void OtherPlugins::setDataBus(IDataBus* dbp) {
    if (!dbp) {
        std::cerr << "[OtherPlugins] Invalid handle on DataBus\n";
        return;
    }
    this->data_bus = dbp;
    if (this->need_to_set_db) {
        postSetDataBus();
    }
}

void OtherPlugins::postSetDataBus() {
    std::cout << "[OtherPlugins] Post set requested" << std::endl;
    if (plugins.empty()) {
        std::cerr << "[OtherPlugins] plugins empty, but post setting was requested. check resolving pipeline\n";
        return;
    }
    for (auto& [path, plugin] : plugins) {
        if (plugin) plugin->setDataBus(this->data_bus);
    }
    this->need_to_set_db = false;
}

void OtherPlugins::registerPlugin(std::vector<void*> pointers) {
    if (pointers.empty()) {
        std::cerr << "[OtherPlugins] No function pointers provided\n";
        if (!pendingPaths.empty()) pendingPaths.pop();
        return;
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        if (pointers[i] == nullptr) {
            std::cerr << "[OtherPlugins] Pointer at index " << i << " is null\n";
            if (!pendingPaths.empty()) pendingPaths.pop();
            return;
        }
    }

    if (pendingPaths.empty()) {
        std::cerr << "[OtherPlugins] registerPlugin called but pendingPaths is empty\n";
        return;
    }

    std::string path = pendingPaths.front();
    pendingPaths.pop();

    auto cp = reinterpret_cast<CreatePlugin>(pointers[0]);
    if (!cp) {
        std::cerr << "[OtherPlugins] Invalid CreatePlugin pointer\n";
        return;
    }

    IGenPlugin* plugin = cp(&(core->getEventManager()), core->getEventManager().getBusPtr());
    if (!plugin) {
        std::cerr << "[OtherPlugins] Plugin creation failed\n";
        return;
    }

    try {
        if (this->data_bus) {
            plugin->setDataBus(this->data_bus);
        } else {
            this->need_to_set_db = true;
        }

        this->plugins.emplace(path, plugin);
        activePluginPaths.insert(path);
        core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_activated", path));

        PluginUIInfo info;
        info.name = plugin->getName();
        info.path = path;
        info.type = PluginUIType::Generic;
        core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_ui_ready", info));
        std::cout << "[OtherPlugins] Plugin registered: " << plugin->getName() << " @ " << path << std::endl;

    } catch (const char* error_message) {
        std::cerr << "[OtherPlugins] Error registering plugin: " << error_message << std::endl;
    } catch (...) {
        std::cerr << "[OtherPlugins] Unknown error registering plugin\n";
    }
}

void OtherPlugins::deletePlugin(std::string path) {
    if (path.empty()) {
        std::cerr << "[OtherPlugins] deletePlugin called with empty path\n";
        return;
    }

    auto it = plugins.find(path);
    if (it != plugins.end()) {
        // Destroy instance first (calls DLL code before library is unloaded)
        it->second->shutdown();
        delete it->second;
        plugins.erase(it);
        std::cout << "[OtherPlugins] Plugin instance destroyed: " << path << std::endl;
    }

    pluginsRegistry.erase(path);
    activePluginPaths.erase(path);

    // Ask DataManager to unload the DLL
    core->getEventManager().sendMessage(AppMessage(name, "unload_library", path));
    // Notify UI to remove the page
    core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_ui_removed", path));
}

void OtherPlugins::disablePlugin(std::string path) {
    if (path.empty()) {
        std::cerr << "[OtherPlugins] disablePlugin called with empty path\n";
        return;
    }
    auto it = plugins.find(path);
    if (it == plugins.end()) {
        std::cerr << "[OtherPlugins] disablePlugin: plugin not loaded: " << path << "\n";
        return;
    }
    // Shutdown and destroy instance, but keep in registry
    it->second->shutdown();
    delete it->second;
    plugins.erase(it);
    activePluginPaths.erase(path);
    // Unload the DLL (will be reloaded on enablePlugin)
    core->getEventManager().sendMessage(AppMessage(name, "unload_library", path));
    // Notify UI to uncheck the checkbox
    core->getEventManager().sendMessage(AppMessage(name, "gen_plugin_deactivated", path));
    std::cout << "[OtherPlugins] Plugin disabled: " << path << std::endl;
}

void OtherPlugins::enablePlugin(std::string path) {
    if (path.empty()) {
        std::cerr << "[OtherPlugins] enablePlugin called with empty path\n";
        return;
    }
    if (plugins.count(path)) {
        std::cout << "[OtherPlugins] enablePlugin: already loaded: " << path << "\n";
        return;
    }
    if (!pluginsRegistry.count(path)) {
        pluginsRegistry.insert(path);
    }
    pendingPaths.push(path);
    Meta meta;
    meta.path = path;
    meta.func_names = {"create", "destroy"};
    core->getEventManager().sendMessage(AppMessage(name, "plugin_resolving_request", meta));
    std::cout << "[OtherPlugins] Plugin enable requested: " << path << std::endl;
}