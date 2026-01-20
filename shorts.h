#ifndef SHORTS_H
#define SHORTS_H

#include <unordered_map>
#include <functional>
#include <any>

using cacheMap = std::unordered_map<std::string, std::any>;
using funcMap = std::unordered_map<std::string, std::function<std::any(const std::any&)>>;

#endif // SHORTS_H
