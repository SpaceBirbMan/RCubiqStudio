#ifndef PLUGINARCHIVE_H
#define PLUGINARCHIVE_H

#include <filesystem>
#include <string>

/// Для распаковки архивов, чтобы плагины могли соответствовать правилу одного файла (но, пожалуй, надо сменить формат)
/// Файл начинается с сигнатуры zip (пакет .ofp или .zip).
bool isPluginArchive(const std::string& path);

struct PluginArchiveExtract {
    bool ok = false;
    std::string error;
    /// index.dll или index.so внутри распакованного каталога.
    std::filesystem::path entry;
};

/// Распаковывает архив в `cacheRoot/packages/<имя>`. В именах записей не допускаются `..` и абсолютные пути.
PluginArchiveExtract extractPluginArchive(const std::filesystem::path& archive,
                                          const std::filesystem::path& cacheRoot);

#endif
