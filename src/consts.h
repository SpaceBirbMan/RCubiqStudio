#ifndef CONSTS_H
#define CONSTS_H
#include <string>

const std::string CACHE_FILE_PATH = "cache.json";

// Обобщённые события жизненного цикла потоковых привязок (без идентификации конкретного плагина).
namespace M3Events {
inline constexpr const char* kStreamBindingsInvalidate = "stream_bindings_invalidate";
inline constexpr const char* kStreamBindingsRestore = "stream_bindings_restore";
}

#endif // CONSTS_H
