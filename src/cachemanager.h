#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include "filesaver.h"
#include "fileloader.h"
#include "misc.h"

class CacheManager
{
public:
    CacheManager();

    void cacheSubscribe(cacheForm cf) {
        subscribers_deserfns[cf.name] = std::move(cf.desfn);
        subscribers_serfns[cf.name] = std::move(cf.sefn);
    }

    void loadCache();

    void saveCache();

    /// Раздача кеша модулям
    void distributeCache();
    /// Сбор кеша у модулей
    void pickCache();

    nlohmann::json getCache() { return cache; }

private:

    void writeCache(const CacheObject& object);

    // Штуки для чтения/записи кеша
    nlohmann::json cache = nlohmann::json::object();
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> subscribers_deserfns {};
    std::unordered_map<std::string, std::function<nlohmann::json()>> subscribers_serfns {};

    FileLoader fileLoader;
    FileSaver fileSaver;

    // таблица icacheable - ссылка на функцию-десериализатор(payload)
};

#endif // CACHEMANAGER_H
