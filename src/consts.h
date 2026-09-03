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
/// Хост объединил control_table; payload — ControlTableUpdate.
inline constexpr const char* kControlTableUpdated = "control_table_updated";
/// Список камер/HID/аудио перечитан (подключение/отключение устройства или ручной refresh).
inline constexpr const char* kDevicesChanged = "devices_changed";
}

namespace TrackerEvents {
/// Трекер → TrackerManager: зарегистрировать контроллеры (ControlTableRegister).
inline constexpr const char* kControlTableRegister = "control_table_register";
}

namespace LoggerEvents {
inline constexpr const char* kLogWrite = "log_write";
inline constexpr const char* kReceiver = "Logger";
}

namespace AppShutdownEvents {
inline constexpr const char* kStopEngineTick = "stop_engine_tick";
inline constexpr const char* kShutdownAllEngines = "shutdown_all_engines";
/// Снять libuiohook (WH_KEYBOARD_LL / mouse) до долгого unload плагинов.
inline constexpr const char* kStopHidListeners = "stop_hid_listeners";
/// Снять QApplication event filter клавиатуры на главном окне.
inline constexpr const char* kReleaseUiInputCapture = "release_ui_input_capture";
}

namespace AppUiEvents {
inline constexpr const char* kStatusMessage = "status_message";
}

/// Плагин-движок сообщает хосту: начал/прекратил писать кадры в окно.
/// payload — пустой (int 0). Хост показывает/скрывает плейсхолдер поверх вьюпорта.
namespace AppRenderingEvents {
inline constexpr const char* kRenderingActive   = "engine_rendering_active";
inline constexpr const char* kRenderingInactive = "engine_rendering_inactive";
inline constexpr const char* kPlaceholderHint   = "engine_placeholder_hint";
}

#endif // CONSTS_H
