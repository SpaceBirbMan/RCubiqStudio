#ifndef CONSTS_H
#define CONSTS_H
#include <string>

const std::string CACHE_FILE_PATH = "cache.json";

// Обобщённые события жизненного цикла потоковых привязок (без идентификации конкретного плагина).
namespace M3Events {
inline constexpr const char* kStreamBindingsInvalidate = "stream_bindings_invalidate";
inline constexpr const char* kStreamBindingsRestore = "stream_bindings_restore";
/// Снять Qt-вкладки с m3_plugin_library_path == path до unload DLL (отключение по чекбоксу, не удаление из списка).
inline constexpr const char* kPluginRuntimeTeardown = "plugin_runtime_teardown";
}

#endif // CONSTS_H
