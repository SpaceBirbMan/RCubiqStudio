#include "cachemanager.h"
#include "appsettings.h"

#include <QString>

#include <iostream>

CacheManager::CacheManager(): fileLoader(FileLoader()), fileSaver(FileSaver()) {}

void CacheManager::loadCache() {

    cache = nlohmann::json::object();
    const QStringList paths = AppSettings::cacheJsonCandidatePathsIncludingWritableFallback();

    try {
        for (const QString& qp : paths) {
            try {
                nlohmann::json loaded = fileLoader.loadJson(qp.toStdString());
                if (!loaded.is_object())
                    continue;
                cache = std::move(loaded);
                std::cerr << "[CacheManager] Loaded session cache from: " << qp.toStdString()
                          << std::endl;
                return;
            } catch (...) {
            }
        }
    } catch (...) {
    }

    std::cerr << "[CacheManager] Session cache missing or unreadable." << std::endl;
}

void CacheManager::saveCache() {

    const QString qpath = AppSettings::writableSessionCachePath();
    if (qpath.isEmpty()) {
        std::cerr << "[CacheManager] No writable cache path." << std::endl;
        return;
    }

    try {
        fileSaver.saveJson(qpath.toStdString(), this->cache);
        std::cerr << "[CacheManager] Session cache saved: " << qpath.toStdString() << std::endl;
    } catch (...) {
        std::cerr << "[CacheManager] saveCache failure." << std::endl;
    }
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
