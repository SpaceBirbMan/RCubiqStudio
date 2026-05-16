#ifndef CONSTS_H
#define CONSTS_H
#include <string>

const std::string CACHE_FILE_PATH = "cache.json";

/// Строковые топики событий жизненного цикла (шина сообщений).
namespace AppLifecycleEvents {
inline constexpr const char* kStreamBindingsInvalidate = "stream_bindings_invalidate";
inline constexpr const char* kStreamBindingsRestore = "stream_bindings_restore";
/// Снять Qt-вкладки до выгрузки DLL (виджеты с свойством `plugin_library_path`).
inline constexpr const char* kPluginRuntimeTeardown = "plugin_runtime_teardown";
/// Плагины могут отреагировать и отдать данные хосту; payload — путь DLL или пустая строка.
inline constexpr const char* kPersistModules = "persist_modules";
}

#endif
