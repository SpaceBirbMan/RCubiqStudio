#include "datamanager.h"
#include "filetypes.h"
#include "shorts.h"
#include "misc.h"

DataManager::DataManager(AppCore* acptr) {
    this->appCorePtr = acptr;

    acptr->getEventManager().subscribe("initialize", &DataManager::initialize, this);

    acptr->getEventManager().subscribe("ask_cache", &DataManager::tryToLoadCache, this);

    acptr->getEventManager().subscribe("engine_resolving_request", &DataManager::resolveFuncTable, this);

    acptr->getEventManager().subscribe("model_markdown_request", &DataManager::loadModel, this);

    acptr->getEventManager().subscribe("sub_to_cache", &CacheManager::cacheSubscribe, &this->cacheManager);

    acptr->getEventManager().subscribe("save", &DataManager::saveFiles, this);
    acptr->getEventManager().subscribe("new", &DataManager::dummy2, this);

    acptr->getEventManager().subscribe("save_cache", &CacheManager::pickCache, &this->cacheManager);

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

void DataManager::resolveFuncTable(LibMeta meta) {

    std::cout << "Started resolving" << std::endl;
    // TODO: а если два движка откроется?
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

    appCorePtr->getEventManager().sendMessage(
        AppMessage(name, "engine_resolving_respond", ptrs)
        );
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
