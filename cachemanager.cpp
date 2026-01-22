#include "cachemanager.h"
#include "shorts.h"
#include "misc.h"
#include <iostream>


CacheManager::CacheManager(): fileLoader(FileLoader()), fileSaver(FileSaver()) {}

void CacheManager::loadCache() {

    try {
        this->cache = fileLoader.loadJson(CACHE_FILE_PATH);
    } catch (...) {}

}

void CacheManager::saveCache() {

    try {
        fileSaver.saveJson(CACHE_FILE_PATH, this->cache);
    } catch (...) {}

}

void CacheManager::writeCache(const CacheObject& object) {
    this->cache[object.name] = object.object;
}

void CacheManager::pickCache() {
    cache.clear();
    for (auto& pair : subscribers_serfns) {
        const std::string& name = pair.first;
        nlohmann::json data = pair.second(); // вызываем сериализацию
        cache[name] = data;
    }
    saveCache();
}

void CacheManager::distributeCache() {
    std::cout << cache.dump() << std::endl;
    for (auto& pair : subscribers_deserfns) {
        const std::string& name = pair.first;
        if (cache.contains(name)) {
            pair.second(cache[name]); // передаём JSON в десериализатор
        }
    }
}
