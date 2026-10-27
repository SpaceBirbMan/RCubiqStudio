#ifndef PLUGINMETA_H
#define PLUGINMETA_H

#include "rcqapi.h"
#include "dynamiclibrary.h"
#include "pluginarchive.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
// Чё-то для правильного отображения
struct PluginDisplayMeta {
    std::string displayName;
    std::string description;
};

inline std::string pluginMetaFallbackName(const std::string& dllPath)
{
    try {
        return std::filesystem::path(dllPath).stem().string();
    } catch (...) {
        return dllPath;
    }
}

inline bool pluginMetaIsLibrary(const std::string& path)
{
    if (path.empty() || isPluginArchive(path))
        return false;
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".dll" || ext == ".so";
}

inline PluginDisplayMeta readPluginDisplayMeta(const std::string& dllPath)
{
    PluginDisplayMeta meta;
    meta.displayName = pluginMetaFallbackName(dllPath);
    meta.description.clear();
    if (!pluginMetaIsLibrary(dllPath))
        return meta;
    try {
        DynamicLibrary lib(dllPath);
        if (void* sym = lib.getSymbol("plugin_display_name")) {
            auto fn = reinterpret_cast<const char* (*)()>(sym);
            if (fn) {
                const char* s = fn();
                if (s && s[0] != '\0')
                    meta.displayName = s;
            }
        }
        if (void* sym = lib.getSymbol("plugin_description")) {
            auto fn = reinterpret_cast<const char* (*)()>(sym);
            if (fn) {
                const char* s = fn();
                if (s)
                    meta.description = s;
            }
        }
    } catch (...) {}
    return meta;
}

inline void fillPluginUiInfo(PluginUIInfo& info, const std::string& dllPath)
{
    info.path = dllPath;
    const PluginDisplayMeta meta = readPluginDisplayMeta(dllPath);
    info.name = meta.displayName;
    info.description = meta.description;
}

/// Place in the plugin .cpp after includes:
///   static constexpr const char kPluginDisplayName[] = "...";
///   static constexpr const char kPluginDescription[] = "...";
///   M3_EXPORT_PLUGIN_META(kPluginDisplayName, kPluginDescription)
#define M3_EXPORT_PLUGIN_META(nameLiteral, descLiteral)                 \
    extern "C" __declspec(dllexport) const char* plugin_display_name()  \
    {                                                                     \
        return nameLiteral;                                               \
    }                                                                     \
    extern "C" __declspec(dllexport) const char* plugin_description()   \
    {                                                                     \
        return descLiteral;                                               \
    }

#endif // PLUGINMETA_H
