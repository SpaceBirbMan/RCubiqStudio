#include "pluginmanager.h"

#include "consts.h"
#include "jsonutil.h"
#include "pluginarchive.h"
#include "pluginmeta.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace {

const char* kHttpTopics[] = {
    "http_get", "http_post", "http_put", "http_patch", "http_delete", "http_head"
};

const char* kHttpMethods[] = {
    "get", "post", "put", "patch", "delete", "head"
};

const char* kSharedDataKeys[] = {
    "plugin_file_broker",
    "plugin_device_broker",
    "app_logger",
    "plugins_cache_root",
    "control_table",
    "render_pipeline",
    "render_pipeline_mutex",
    "viewport_size",
    "viewport_dpr",
    "viewport_commands_deque",
    "keyboard_state",
    "scene_board",
    "scene_board_mutex"
};

const char* kSharedHandleKeys[] = {
    "window_handle",
    "render_backend",
    "render_device",
    "frames_buffer",
    "engine_gpu_load"
};

} // namespace

PluginManager::PluginManager(AppCore* core)
    : core_(core)
    , host_(&core->getEventManager())
    , hostBus_(core->getEventManager().getBusPtr())
    , plugins_("PluginBus")
    , pluginBus_(plugins_.getBusPtr())
{
    setName("PluginPackages");
    mirrorAllowedData();
    bridgeHost();

    host_->subscribe(getName(), "pre_initialize", &PluginManager::preInitialize, this);
    host_->subscribe(getName(), "initialize", &PluginManager::initialize, this);
    host_->subscribe(getName(), "init_complete", &PluginManager::postInitialize, this);
    host_->subscribe(getName(), "add_plugins_to_registry", &PluginManager::addPaths, this);
    host_->subscribe(getName(), "enable_gen_plugin", &PluginManager::enablePlugin, this);
    host_->subscribe(getName(), "disable_gen_plugin", &PluginManager::disablePlugin, this);
    host_->subscribe(getName(), "remove_gen_plugin", &PluginManager::removePlugin, this);
    host_->subscribe(getName(), "package_resolving_respond", &PluginManager::onResolved, this);

    ticking_ = true;
    ticker_ = std::thread(&PluginManager::tickLoop, this);
}

PluginManager::~PluginManager()
{
    ticking_ = false;
    if (ticker_.joinable())
        ticker_.join();
    std::lock_guard<std::mutex> lock(slotsMutex_);
    for (auto& [path, slot] : slots_)
        dropSlot(slot);
    slots_.clear();
}

nlohmann::json PluginManager::serializeCache() const
{
    std::lock_guard<std::mutex> lock(slotsMutex_);
    return nlohmann::json{
        {"pluginsRegistry", registry_},
        {"activePluginPaths", active_}
    };
}

void PluginManager::deserializeCache(const nlohmann::json& data)
{
    std::lock_guard<std::mutex> lock(slotsMutex_);
    if (data.contains("pluginsRegistry"))
        fillStringSetFromJsonArray(data["pluginsRegistry"], registry_);
    else
        registry_.clear();
    if (data.contains("activePluginPaths"))
        fillStringSetFromJsonArray(data["activePluginPaths"], active_);
    else
        active_.clear();
}

void PluginManager::preInitialize()
{
    cacheForm form;
    form.name = getName();
    form.desfn = [this](const nlohmann::json& data) { deserializeCache(data); };
    form.sefn = [this]() { return serializeCache(); };
    host_->sendMessage(AppMessage(getName(), "sub_to_cache", form));
    host_->sendMessage(AppMessage(getName(), "module_subscribed", getName()));
}

void PluginManager::initialize()
{
    host_->sendMessage(AppMessage(getName(), "init_started", 0));
    host_->sendMessage(AppMessage(getName(), "module_initialized", getName()));
}

void PluginManager::postInitialize()
{
    std::vector<std::string> registered;
    std::vector<std::string> active;
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        registered.assign(registry_.begin(), registry_.end());
        active.assign(active_.begin(), active_.end());
    }
    for (const std::string& path : registered) {
        if (!isPluginArchive(path))
            continue;
        const bool on = std::find(active.begin(), active.end(), path) != active.end();
        if (!on)
            notifyUi(path, std::filesystem::path(path).stem().string(), "", false);
    }
    if (!active.empty())
        addPaths(active);
}

void PluginManager::mirrorAllowedData()
{
    for (const char* key : kSharedDataKeys)
        pluginBus_->follow(hostBus_, key, false);
    for (const char* key : kSharedHandleKeys)
        pluginBus_->follow(hostBus_, key, true);
}

void PluginManager::bridgeHost()
{
    for (const char* topic : kHttpTopics) {
        plugins_.subscribe(getName(), topic, [this, topic](const std::any& data) {
            host_->sendMessage(AppMessage(getName(), topic, data));
        });
    }
    plugins_.subscribe(getName(), "browser_auth", [this](const std::any& data) {
        host_->sendMessage(AppMessage(getName(), "browser_auth", data));
    });
    plugins_.subscribe(getName(), "websocket_open", [this](const std::any& data) {
        host_->sendMessage(AppMessage(getName(), "websocket_open", data));
    });
    plugins_.subscribe(getName(), TrackerEvents::kControlTableRegister, [this](const std::any& data) {
        host_->sendMessage(AppMessage(getName(), TrackerEvents::kControlTableRegister, data));
    });
    host_->subscribe(getName(), AppLifecycleEvents::kPersistModules, [this](const std::any& data) {
        plugins_.sendMessage(AppMessage(getName(), AppLifecycleEvents::kPersistModules, data));
    });
    host_->subscribe(getName(), AppLifecycleEvents::kStreamBindingsInvalidate, [this](const std::any& data) {
        plugins_.sendMessage(AppMessage(getName(), AppLifecycleEvents::kStreamBindingsInvalidate, data));
    });
    host_->subscribe(getName(), AppLifecycleEvents::kStreamBindingsRestore, [this](const std::any& data) {
        plugins_.sendMessage(AppMessage(getName(), AppLifecycleEvents::kStreamBindingsRestore, data));
    });
    plugins_.subscribe(getName(), "plugin_ui_pages", [this](const std::any& data) {
        host_->sendMessage(AppMessage(getName(), "init_ui_tracker", data));
    });
}

void PluginManager::armResponses(const std::string& pluginName)
{
    for (const char* method : kHttpMethods) {
        const std::string topic = pluginName + "_http_" + method + "_respond";
        host_->subscribe(pluginName, topic, [this, topic](const std::any& data) {
            plugins_.sendMessage(AppMessage(getName(), topic, data));
        });
    }
    const std::string browser = pluginName + "_browser_auth_respond";
    host_->subscribe(pluginName, browser, [this, browser](const std::any& data) {
        plugins_.sendMessage(AppMessage(getName(), browser, data));
    });
    const std::string socket = pluginName + "_websocket_frame";
    host_->subscribe(pluginName, socket, [this, socket](const std::any& data) {
        plugins_.sendMessage(AppMessage(getName(), socket, data));
    });
}

std::filesystem::path PluginManager::cacheRoot() const
{
    try {
        return std::any_cast<std::string>(hostBus_->getData("plugins_cache_root"));
    } catch (...) {
        return std::filesystem::current_path() / "plugins_cache";
    }
}

void PluginManager::notifyUi(const std::string& path, const std::string& displayName,
                             const std::string& description, bool active)
{
    PluginUIInfo info;
    info.path = path;
    info.name = displayName.empty() ? std::filesystem::path(path).stem().string() : displayName;
    info.description = description;
    info.type = PluginUIType::Generic;
    host_->sendMessage(AppMessage(getName(), "gen_plugin_ui_ready", info));
    if (active)
        host_->sendMessage(AppMessage(getName(), "gen_plugin_activated", path));
}

bool PluginManager::loadPackage(const std::string& path)
{
    if (!isPluginArchive(path))
        return false;

    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        if (slots_.count(path))
            return true;
        registry_.insert(path);
        active_.insert(path);
    }

    const PluginArchiveExtract extracted = extractPluginArchive(path, cacheRoot());
    if (!extracted.ok) {
        std::cerr << "[PluginPackages] " << extracted.error << " @ " << path << std::endl;
        std::lock_guard<std::mutex> lock(slotsMutex_);
        active_.erase(path);
        return false;
    }

    std::string displayName = std::filesystem::path(path).stem().string();
    std::string description;
    const PluginDisplayMeta meta = readPluginDisplayMeta(extracted.entry.string());
    if (!meta.displayName.empty() && meta.displayName != "index")
        displayName = meta.displayName;
    description = meta.description;

    PendingPackage pending;
    pending.packagePath = path;
    pending.entryPath = extracted.entry.string();
    pending.displayName = displayName;
    pending.description = description;
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        pending_.push(pending);
    }

    Meta request;
    request.path = pending.entryPath;
    request.func_names = {"create"};
    request.respond = "package_resolving_respond";
    host_->sendMessage(AppMessage(getName(), "plugin_resolving_request", request));
    return true;
}

void PluginManager::onResolved(std::vector<void*> pointers)
{
    PendingPackage pending;
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        if (pending_.empty())
            return;
        pending = pending_.front();
        pending_.pop();
    }

    auto fail = [this, &pending]() {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        active_.erase(pending.packagePath);
        host_->sendMessage(AppMessage(getName(), "gen_plugin_deactivated", pending.packagePath));
        if (!pending.entryPath.empty())
            host_->sendMessage(AppMessage(getName(), "unload_library", pending.entryPath));
    };

    if (pointers.empty() || pointers[0] == nullptr) {
        std::cerr << "[PluginPackages] create() не найден @ " << pending.packagePath << std::endl;
        fail();
        return;
    }

    auto* create = reinterpret_cast<CreateNewPlugin>(pointers[0]);
    IPlugin* plugin = create(&plugins_, pluginBus_);
    if (!plugin) {
        std::cerr << "[PluginPackages] create() вернул пусто @ " << pending.packagePath << std::endl;
        fail();
        return;
    }

    try {
        mirrorAllowedData();
        plugin->setLibraryPath(pending.packagePath);
        plugin->created();
        plugin->inited();
        armResponses(plugin->getName());
        plugin->start();
    } catch (const std::exception& e) {
        std::cerr << "[PluginPackages] старт прерван: " << e.what() << std::endl;
        try { plugin->stop(); } catch (...) {}
        try { plugin->cleared(); } catch (...) {}
        try { plugin->destroyed(); } catch (...) {}
        plugin->release();
        fail();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        Slot slot;
        slot.packagePath = pending.packagePath;
        slot.entryPath = pending.entryPath;
        slot.plugin = plugin;
        slot.updating = true;
        slots_.emplace(pending.packagePath, std::move(slot));
    }
    notifyUi(pending.packagePath, pending.displayName, pending.description, true);
    std::cout << "[PluginPackages] " << plugin->getName() << " @ " << pending.packagePath << std::endl;
}

void PluginManager::dropSlot(Slot& slot)
{
    if (!slot.plugin)
        return;
    const std::string pluginName = slot.plugin->getName();
    const std::string path = slot.packagePath;
    if (slot.updating) {
        try { slot.plugin->stop(); } catch (...) {}
        slot.updating = false;
    }
    try { slot.plugin->cleared(); } catch (...) {}
    plugins_.unsubscribeReceiver(pluginName);
    host_->sendMessage(AppMessage(getName(), AppLifecycleEvents::kPluginRuntimeTeardown, path));
    host_->unsubscribeReceiver(pluginName);
    try { slot.plugin->destroyed(); } catch (...) {}
    slot.plugin->release();
    slot.plugin = nullptr;
    if (!slot.entryPath.empty())
        host_->sendMessage(AppMessage(getName(), "unload_library", slot.entryPath));
}

void PluginManager::addPaths(std::vector<std::string> paths)
{
    for (const std::string& path : paths) {
        if (!isPluginArchive(path))
            continue;
        bool already = false;
        {
            std::lock_guard<std::mutex> lock(slotsMutex_);
            registry_.insert(path);
            already = slots_.count(path) > 0;
        }
        if (already) {
            std::string name;
            {
                std::lock_guard<std::mutex> lock(slotsMutex_);
                name = slots_[path].plugin ? slots_[path].plugin->getName() : std::string{};
            }
            notifyUi(path, name, "", true);
            continue;
        }
        bool shouldLoad = false;
        {
            std::lock_guard<std::mutex> lock(slotsMutex_);
            shouldLoad = active_.count(path) > 0;
        }
        if (!shouldLoad) {
            notifyUi(path, std::filesystem::path(path).stem().string(), "", false);
            continue;
        }
        loadPackage(path);
    }
}

void PluginManager::enablePlugin(std::string path)
{
    if (!isPluginArchive(path))
        return;
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        registry_.insert(path);
        if (slots_.count(path))
            return;
        active_.insert(path);
    }
    loadPackage(path);
    core_->writeSessionCache();
}

void PluginManager::disablePlugin(std::string path)
{
    if (!isPluginArchive(path))
        return;
    Slot slot;
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        auto it = slots_.find(path);
        if (it == slots_.end())
            return;
        slot = std::move(it->second);
        slots_.erase(it);
        active_.erase(path);
    }
    dropSlot(slot);
    host_->sendMessage(AppMessage(getName(), "gen_plugin_deactivated", path));
    core_->writeSessionCache();
}

void PluginManager::removePlugin(std::string path)
{
    if (!isPluginArchive(path))
        return;
    disablePlugin(path);
    {
        std::lock_guard<std::mutex> lock(slotsMutex_);
        registry_.erase(path);
        active_.erase(path);
    }
    host_->sendMessage(AppMessage(getName(), "gen_plugin_ui_removed", path));
    core_->writeSessionCache();
}

void PluginManager::tickLoop()
{
    using clock = std::chrono::steady_clock;
    auto previous = clock::now();
    while (ticking_) {
        const auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        {
            std::lock_guard<std::mutex> lock(slotsMutex_);
            for (auto& [path, slot] : slots_) {
                if (!slot.updating || !slot.plugin)
                    continue;
                try {
                    slot.plugin->update(dt);
                } catch (...) {
                    try { slot.plugin->stop(); } catch (...) {}
                    slot.updating = false;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
