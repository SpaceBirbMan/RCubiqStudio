#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QtGlobal>

#include <QStringList>

#include <cstdint>

class QApplication;
class QString;
class IDataBus;

enum class AppThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
};

class AppSettings {
public:
    static void installAppMetadata();
    static void bootstrap(QApplication& app);

    /// True если данные приложения рядом с exe: Debug (`STUDIO_PORTABLE_USER_DATA`) или сборка с `-DSTUDIO_FORCE_ROOT_CACHE=ON`.
    static bool usesPortableUserDataRoots();

    static AppThemeMode widgetTheme();
    static void setWidgetTheme(AppThemeMode mode);

    static void applyWidgetTheme(QApplication& app);

    static QString uiLanguage();
    static void setUiLanguage(const QString& code);
    /// "system", "en", or locale suffix (e.g. ru_RU); .qm basename is APP_QM_PREFIX + code.
    static void applyTranslator();

    static bool traceProgramFlow();
    static void setTraceProgramFlow(bool enabled);

    static bool ignorePluginVersionCheck();
    static void setIgnorePluginVersionCheck(bool on);

    static bool showWelcomeOnStartup();
    static void setShowWelcomeOnStartup(bool enabled);

    static bool developerMode();
    static void setDeveloperMode(bool on);

    /// Перечень видеоустройств DirectShow при старте (без необходимости можно отключить).
    static bool enumerateVideoInputsAtStartup();
    static void setEnumerateVideoInputsAtStartup(bool on);

    /// Мост виртуальной камеры (OBS shared memory и т.п.) при старте главного окна.
    static bool startVirtualCameraBridgeAtStartup();
    static void setStartVirtualCameraBridgeAtStartup(bool on);

    /// Индикатор загрузки GPU по времени кадра (сообщает движок) в строке состояния.
    static bool showGpuFrameLoadInStatusBar();
    static void setShowGpuFrameLoadInStatusBar(bool on);

    /// Индекс адаптера DXGI из настроек. На шине `render_device` значение держится в BusHandle<uintptr_t>::defaultVal();
    /// указатель живого клиента там же содержит ID3D11Device* движка (не смешивать).
    static std::uintptr_t renderAdapterIndex();
    static void setRenderAdapterIndex(std::uintptr_t DXGIOrdinal);
    static void applyRenderAdapterToDataBus(IDataBus* bus);

    /// Абсолютный путь к файлу настроек (INI).
    static QString settingsFilePath();

    static QString cacheJsonPath();

    static QStringList cacheJsonCandidatePaths();

    /// Абсолютный путь к записываемому session cache (каталог создаётся при необходимости).
    static QString writableSessionCachePath();

    /// Корень каталога `plugins_cache` с правом записи (рядом с exe при возможности, иначе AppLocalData).
    static QString writablePluginsCacheDirectory();

    static QStringList cacheJsonCandidatePathsIncludingWritableFallback();
    static void restoreFactoryDefaults(QApplication& app);

    /// Восстанавливает значения приложения из настроек (после «Сбросить к умолчанию»).
    static void reapplyPersistedAppearance(QApplication& app);
};

#endif
