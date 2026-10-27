#ifndef JSONUTIL_H
#define JSONUTIL_H

#include <nlohmann/json.hpp>
#include <set>
#include <string>
// А для чего?
/// nlohmann::json::value() does not treat JSON null as "missing" — use these helpers.
inline std::string jsonStringOr(const nlohmann::json& j, const char* key,
                                const std::string& def = {}) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j[key];
    if (v.is_null() || !v.is_string()) return def;
    return v.get<std::string>();
}

inline void fillStringSetFromJsonArray(const nlohmann::json& arr, std::set<std::string>& out) {
    out.clear();
    if (!arr.is_array()) return;
    for (const auto& el : arr) {
        if (el.is_string()) {
            const std::string s = el.get<std::string>();
            if (!s.empty()) out.insert(s);
        }
    }
}

#endif // JSONUTIL_H
