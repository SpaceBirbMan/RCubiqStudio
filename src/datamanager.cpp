#include "datamanager.h"
#include "appsettings.h"
#include "rcqapi.h"
#include "consts.h"
#include "logger.h"
#include <functional>
#include <filesystem>

DataManager::DataManager(AppCore* acptr) {
    this->appCorePtr = acptr;

    pluginFileBroker_ = std::make_unique<PluginFileBrokerImpl>();
    QString pluginsCacheDir;
    {
        pluginsCacheDir = AppSettings::writablePluginsCacheDirectory();
#ifdef _WIN32
        const std::filesystem::path root(reinterpret_cast<const wchar_t*>(pluginsCacheDir.utf16()));
#else
        const std::filesystem::path root(pluginsCacheDir.toUtf8().constData());
#endif
        pluginFileBroker_->setPluginsCacheRoot(root);
    }
    pluginDeviceBroker_ = std::make_unique<PluginDeviceBrokerImpl>();
    IDataBus* bus = acptr->getEventManager().getBusPtr();
    bus->registerData("plugin_file_broker", static_cast<IPluginFileBroker*>(pluginFileBroker_.get()));
    bus->registerData("plugin_device_broker", static_cast<IPluginDeviceBroker*>(pluginDeviceBroker_.get()));
    bus->registerData("app_logger", static_cast<IAppLogger*>(&acptr->getEventManager().getLogger()));
    bus->registerData("plugins_cache_root", pluginsCacheDir.toStdString());

    acptr->setPersistCallbacks(
        [this](const std::string& dllPathHint) {
            this->appCorePtr->getEventManager().dispatchImmediately(
                AppMessage(this->name, AppLifecycleEvents::kPersistModules, dllPathHint));
        },
        [this]() {
            this->cacheManager.pickCache();
        });

    acptr->getEventManager().subscribe(name, "initialize", &DataManager::initialize, this);
    acptr->getEventManager().subscribe(name, "ask_cache", &DataManager::tryToLoadCache, this);
    acptr->getEventManager().subscribe(name, "engine_resolving_request", &DataManager::resolveFuncTable, this);
    acptr->getEventManager().subscribe(name, "model_markdown_request", &DataManager::loadModel, this);
    acptr->getEventManager().subscribe(name, "sub_to_cache", &CacheManager::cacheSubscribe, &this->cacheManager);
    acptr->getEventManager().subscribe(name, "save", &DataManager::saveFiles, this);
    acptr->getEventManager().subscribe(name, "new", &DataManager::dummy2, this);
    acptr->getEventManager().subscribe(name, "save_cache", std::function<void(const std::any&)>{
        [this](const std::any&) {
            this->appCorePtr->persistPluginsAndWriteSessionCache(std::string{});
        }});
    acptr->getEventManager().subscribe(name, "resolve_render_api_request", &DataManager::resolveApi, this);
    acptr->getEventManager().subscribe(name, "tracking_resolving_request", &DataManager::resolveTracker, this);
    acptr->getEventManager().subscribe(name, "plugin_resolving_request", &DataManager::resolvePlugin, this);
    acptr->getEventManager().subscribe(name, "unload_library", &DataManager::unloadLibrary, this);

}

void DataManager::initialize() {


    appCorePtr->getEventManager().sendMessage(AppMessage("dataManager", "dm_ready", 0));

    //appCorePtr->getEventManager().sendMessage(AppMessage(name, "set_data", &cache));
}

void DataManager::tryToLoadCache() {
    try {
        cacheManager.loadCache();
        if (!this->cacheManager.getCache().empty()) {
            cacheManager.distributeCache();
        } else {
            std::cerr << "Cache is empty" << std::endl;
        }
        appCorePtr->getEventManager().sendMessage(AppMessage(name, "cache_ok", 0));
    } catch (...) {
        std::cerr << "Cache loading error" << std::endl;
    }
}

void DataManager::resolveSymbols(const Meta& meta, const std::string& respondTopic)
{
    const std::string topic = meta.respond.empty() ? respondTopic : meta.respond;
    auto it = libsPool.find(meta.path);
    if (it == libsPool.end()) {
        try {
            auto lib = std::make_shared<DynamicLibrary>(meta.path);
            auto result = libsPool.emplace(meta.path, lib);
            it = result.first;
        } catch (const std::exception& e) {
            std::cerr << "[DataManager] resolveSymbols: failed to load library '"
                      << meta.path << "': " << e.what() << std::endl;
            appCorePtr->getEventManager().sendMessage(
                AppMessage(name, topic, std::vector<void*>{}));
            return;
        }
    }

    std::vector<void*> ptrs;
    for (int i = 0; i < (int)meta.func_names.size(); i++) {
        try {
            ptrs.emplace_back(it->second->getSymbol(meta.func_names[i]));
        } catch (const std::exception& e) {
            if (i == 0) {
                std::cerr << "[DataManager] resolveSymbols: mandatory symbol '"
                          << meta.func_names[i] << "' not found: " << e.what() << std::endl;
                appCorePtr->getEventManager().sendMessage(
                    AppMessage(name, topic, std::vector<void*>{}));
                return;
            }
            std::cerr << "[DataManager] resolveSymbols: optional symbol '"
                      << meta.func_names[i] << "' not found, using nullptr\n";
            ptrs.emplace_back(nullptr);
        }
    }

    appCorePtr->getEventManager().sendMessage(AppMessage(name, topic, ptrs));
}

void DataManager::resolveTracker(Meta meta) {
    resolveSymbols(meta, "tracker_resolving_respond");
}

void DataManager::resolveFuncTable(LibMeta meta) {
    auto log = appCorePtr->getEventManager().getLogger().module(name);
    log.info("resolveFuncTable: begin " + meta.path);
    appCorePtr->getEventManager().sendMessage(
        AppMessage("Core", AppUiEvents::kStatusMessage, "Loading DLL: " + meta.path));
    Meta request;
    request.path = meta.path;
    request.func_names = meta.func_names;
    resolveSymbols(request, "engine_resolving_respond");
}

void DataManager::resolveApi(LibMeta meta) {
    Meta request;
    request.path = meta.path;
    request.func_names = meta.func_names;
    resolveSymbols(request, "resolve_render_api_respond");
}

void DataManager::loadModel(std::vector<std::string> exts) {

    // верификация...

    // FileInstance instance = FileInstance();
    // for (std::string ext : exts) {
    //     instance = this->modelManager.getLoader().load(""+ext);
    // }

    //appCorePtr->getEventManager().sendMessage(AppMessage(name, "model_markdown_respond", instance.getData()));
}

void DataManager::saveFiles(std::any data) {

}

void DataManager::resolvePlugin(Meta meta) {
    resolveSymbols(meta, "plugin_resolving_respond");
}

void DataManager::unloadLibrary(std::string path) {
    auto it = libsPool.find(path);
    if (it != libsPool.end()) {
        libsPool.erase(it);
        std::cout << "[DataManager] Library unloaded: " << path << std::endl;
    } else {
        std::cerr << "[DataManager] unloadLibrary: path not found in pool: " << path << std::endl;
    }
}
