#include "datamanager.h"
#include "appsettings.h"
#include "filetypes.h"
#include "shorts.h"
#include "misc.h"
#include "consts.h"
#include <functional>
#include <filesystem>

DataManager::DataManager(AppCore* acptr) {
    this->appCorePtr = acptr;

    pluginFileBroker_ = std::make_unique<PluginFileBrokerImpl>();
    {
        const QString dir = AppSettings::writablePluginsCacheDirectory();
#ifdef _WIN32
        const std::filesystem::path root(reinterpret_cast<const wchar_t*>(dir.utf16()));
#else
        const std::filesystem::path root(dir.toUtf8().constData());
#endif
        pluginFileBroker_->setPluginsCacheRoot(root);
    }
    pluginDeviceBroker_ = std::make_unique<PluginDeviceBrokerImpl>();
    IDataBus* bus = acptr->getEventManager().getBusPtr();
    bus->registerData("plugin_file_broker", static_cast<IPluginFileBroker*>(pluginFileBroker_.get()));
    bus->registerData("plugin_device_broker", static_cast<IPluginDeviceBroker*>(pluginDeviceBroker_.get()));

    acptr->setPersistPipeline([this](const std::string& dllPathHint) {
        this->appCorePtr->getEventManager().dispatchImmediately(
            AppMessage(this->name, AppLifecycleEvents::kPersistModules, dllPathHint));
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

void DataManager::resolveTracker(Meta meta) {
    std::cout << "Started resolving" << std::endl;
    // TODO: а если два движка откроется?
    auto it = libsPool.find(meta.path);
    if (it == libsPool.end()) {
        try {
            auto lib = std::make_shared<DynamicLibrary>(meta.path);
            auto result = libsPool.emplace(meta.path, lib);
            it = result.first;
        } catch (const std::exception& e) {
            std::cerr << "[DataManager] resolveTracker: failed to load library '"
                      << meta.path << "': " << e.what() << std::endl;
            appCorePtr->getEventManager().sendMessage(
                AppMessage(name, "tracker_resolving_respond", std::vector<void*>{})
            );
            return;
        }
    }

    std::vector<void*> ptrs;
    for (int i = 0; i < (int)meta.func_names.size(); i++) {
        try {
            ptrs.emplace_back(it->second->getSymbol(meta.func_names[i]));
        } catch (const std::exception& e) {
            if (i == 0) {
                // 'create' is mandatory — abort
                std::cerr << "[DataManager] resolveTracker: mandatory symbol '"
                          << meta.func_names[i] << "' not found: " << e.what() << std::endl;
                appCorePtr->getEventManager().sendMessage(
                    AppMessage(name, "tracker_resolving_respond", std::vector<void*>{})
                );
                return;
            }
            // Optional symbols (e.g. 'destroy') — use nullptr, TrackerManager handles it
            std::cerr << "[DataManager] resolveTracker: optional symbol '"
                      << meta.func_names[i] << "' not found, using nullptr\n";
            ptrs.emplace_back(nullptr);
        }
    }

    appCorePtr->getEventManager().sendMessage(
        AppMessage(name, "tracker_resolving_respond", ptrs)
    );
}

void DataManager::resolveFuncTable(LibMeta meta) {

    std::cout << "Started resolving" << std::endl;
    // TODO: а если два движка откроется?
    auto it = libsPool.find(meta.path);
    if (it == libsPool.end()) {
        try {
            auto lib = std::make_shared<DynamicLibrary>(meta.path);
            auto result = libsPool.emplace(meta.path, lib);
            it = result.first;
        } catch (const std::exception& e) {
            std::cerr << "[DataManager] resolveFuncTable: failed to load library '"
                      << meta.path << "': " << e.what() << std::endl;
            appCorePtr->getEventManager().sendMessage(
                AppMessage(name, "engine_resolving_respond", std::vector<void*>{})
            );
            return;
        }
    }

    std::vector<void*> ptrs;
    for (int i = 0; i < (int)meta.func_names.size(); i++) {
        try {
            ptrs.emplace_back(it->second->getSymbol(meta.func_names[i]));
        } catch (const std::exception& e) {
            if (i == 0) {
                std::cerr << "[DataManager] resolveFuncTable: mandatory symbol '"
                          << meta.func_names[i] << "' not found: " << e.what() << std::endl;
                appCorePtr->getEventManager().sendMessage(
                    AppMessage(name, "engine_resolving_respond", std::vector<void*>{})
                );
                return;
            }
            std::cerr << "[DataManager] resolveFuncTable: optional symbol '"
                      << meta.func_names[i] << "' not found, using nullptr\n";
            ptrs.emplace_back(nullptr);
        }
    }

    appCorePtr->getEventManager().sendMessage(
        AppMessage(name, "engine_resolving_respond", ptrs)
    );
}

// TODO: Функции друг на друга слишком похожи, имеет смысл унифицировать для отправки в этот менеджер, а ответное сообщение помещать в LibMeta

void DataManager::resolveApi(LibMeta meta) {

    auto it = libsPool.find(meta.path);
    if (it == libsPool.end()) {
        auto lib = std::make_shared<DynamicLibrary>(meta.path);
        auto result = libsPool.emplace(meta.path, lib);
        it = result.first;
    }

    std::vector<void*> ptrs;
    for (int i = 0; i < meta.func_names.size(); i++) {
        ptrs.emplace_back(it->second->getSymbol(meta.func_names[i]));
    }

    this->appCorePtr->getEventManager().sendMessage(AppMessage(name,"resolve_render_api_respond", ptrs));
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
    std::cout << "Started plugin resolving" << std::endl;
    // TODO: а если два движка откроется?
    auto it = libsPool.find(meta.path);
    if (it == libsPool.end()) {
        try {
            auto lib = std::make_shared<DynamicLibrary>(meta.path);
            auto result = libsPool.emplace(meta.path, lib);
            it = result.first;
        } catch (const std::exception& e) {
            std::cerr << "[DataManager] resolvePlugin: failed to load library '"
                      << meta.path << "': " << e.what() << std::endl;
            appCorePtr->getEventManager().sendMessage(
                AppMessage(name, "plugin_resolving_respond", std::vector<void*>{})
            );
            return;
        }
    }

    std::vector<void*> ptrs;
    for (int i = 0; i < (int)meta.func_names.size(); i++) {
        try {
            ptrs.emplace_back(it->second->getSymbol(meta.func_names[i]));
        } catch (const std::exception& e) {
            if (i == 0) {
                std::cerr << "[DataManager] resolvePlugin: mandatory symbol '"
                          << meta.func_names[i] << "' not found: " << e.what() << std::endl;
                appCorePtr->getEventManager().sendMessage(
                    AppMessage(name, "plugin_resolving_respond", std::vector<void*>{})
                );
                return;
            }
            std::cerr << "[DataManager] resolvePlugin: optional symbol '"
                      << meta.func_names[i] << "' not found, using nullptr\n";
            ptrs.emplace_back(nullptr);
        }
    }

    appCorePtr->getEventManager().sendMessage(
        AppMessage(name, "plugin_resolving_respond", ptrs)
    );
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
