#ifndef PLUGINFILEBROKER_H
#define PLUGINFILEBROKER_H

#include "misc.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

/// Файлы плагина под каталогом `plugins_cache` (корень задаёт приложение через `setPluginsCacheRoot`, иначе рядом с exe).
class PluginFileBrokerImpl final : public IPluginFileBroker {
public:
    /// Создаёт каталог при необходимости; при ошибке корень не меняется (остаётся поведение по умолчанию).
    void setPluginsCacheRoot(const std::filesystem::path& absoluteRoot);

    void setPluginStorageRelativePath(const std::string& scopeKey,
                                      const std::string& relativePathUnderPluginsCache) override;
    PluginFileReadResult read(const std::string& scopeKey, const std::string& fileName) override;
    bool write(const std::string& scopeKey, const std::string& fileName, const uint8_t* data, size_t size) override;

    static std::string sanitizedBaseName(const std::string& fileName);
    /// Короткий сегмент каталога по полному ключу scope (обычно путь DLL): базовое имя файла + FNV-1a (64-бит hex).
    static std::string scopeToDirSegment(const std::string& scopeKey);
    static std::string sanitizeRelativePath(const std::string& rel);

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::string> scopeRel_;
    std::filesystem::path root_override_;
    bool has_root_override_ = false;

    std::string effectiveSubPath(const std::string& scopeKey);
    std::filesystem::path absoluteScopeDir(const std::string& scopeKey);
};

#endif
