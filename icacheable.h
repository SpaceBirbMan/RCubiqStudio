#ifndef ICACHEABLE_H
#define ICACHEABLE_H

#include <nlohmann/json.hpp>

struct ICacheable { // все получатели кэша должны от него наследоваться
    virtual std::string cacheKey() const = 0;
    virtual nlohmann::json serializeCache() const = 0;
    virtual void deserializeCache(const nlohmann::json& data) = 0;
};

#endif // ICACHEABLE_H
