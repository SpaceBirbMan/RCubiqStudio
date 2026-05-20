#include "appsettings.h"
#include "databus.h"
#include "bushandle.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QStyleHints>
#include <QStyleFactory>
#include <QTranslator>
#include <QWidget>

#include <cstdio>

namespace {

constexpr QLatin1String kQmBase(QLatin1String("Studio_"));

QTranslator g_appTranslator;

static QtMessageHandler s_downstreamMessageHandler = nullptr;
static bool s_traceActive = false;
static QFile g_traceLog;

void ensureTraceHookInstalled();

void traceMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (s_traceActive) {
        const char* level = "info";
        switch (type) {
        case QtDebugMsg: level = "debug"; break;
        case QtInfoMsg: level = "info"; break;
        case QtWarningMsg: level = "warning"; break;
        case QtCriticalMsg: level = "critical"; break;
        case QtFatalMsg: level = "fatal"; break;
        }
        const QByteArray formatted = QByteArray("[trace ").append(level).append("] ").append(ctx.file ? ctx.file : "?")
                                                         .append(':').append(QByteArray::number(ctx.line)).append(" (")
                                                         .append(ctx.function ? ctx.function : "?").append(") ")
                                                         .append(msg.toUtf8()).append('\n');
        if (g_traceLog.isOpen()) {
            g_traceLog.write(formatted.constData(), formatted.size());
            g_traceLog.flush();
        }
        std::fwrite(formatted.constData(), 1, formatted.size(), stderr);
        std::fflush(stderr);
    }
    if (s_downstreamMessageHandler)
        s_downstreamMessageHandler(type, ctx, msg);
    else if (!s_traceActive) {
        const QByteArray line = msg.toLocal8Bit() + '\n';
        std::fwrite(line.constData(), 1, line.size(), stderr);
        std::fflush(stderr);
    }
}

void ensureTraceHookInstalled()
{
    static bool installed = false;
    if (installed)
        return;
    s_downstreamMessageHandler = qInstallMessageHandler(traceMessageHandler);
    installed = true;
}

void syncTraceLogWithFlag()
{
    AppSettings::installAppMetadata();
    if (!s_traceActive) {
        if (g_traceLog.isOpen()) {
            g_traceLog.flush();
            g_traceLog.close();
        }
        return;
    }
    ensureTraceHookInstalled();

    QString iniPath = QSettings().fileName();
    if (iniPath.isEmpty())
        iniPath = QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String("settings.ini"));

    const QFileInfo sf(iniPath);
    QDir dir = sf.absoluteDir();
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QString path = dir.filePath(QLatin1String("trace.log"));
    if (g_traceLog.isOpen() && QFileInfo(g_traceLog.fileName()).canonicalFilePath() != QFileInfo(path).canonicalFilePath()) {
        g_traceLog.flush();
        g_traceLog.close();
    }
    if (!g_traceLog.isOpen()) {
        g_traceLog.setFileName(path);
        g_traceLog.open(QIODevice::Append | QIODevice::Text | QIODevice::Unbuffered);
    }
}

void applyTraceFromSettings()
{
    AppSettings::installAppMetadata();
    QSettings st;
    s_traceActive = st.value(QLatin1String("diag/trace_verbose"), false).toBool();
    syncTraceLogWithFlag();
}

void applyFusionDarkPalette(QApplication& app)
{
    QPalette palette;
    const QColor darkGray(53, 53, 53);
    const QColor gray(128, 128, 128);
    const QColor black(42, 42, 42);
    const QColor blue(162, 162, 208);

    palette.setColor(QPalette::Window, darkGray);
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
    palette.setColor(QPalette::Base, black);
    palette.setColor(QPalette::AlternateBase, darkGray);
    palette.setColor(QPalette::ToolTipBase, darkGray);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, gray);
    palette.setColor(QPalette::Button, darkGray);
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(162, 198, 255));
    palette.setColor(QPalette::Highlight, blue);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(127, 127, 127));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, gray);
    app.setPalette(palette);
}

QString systemQmLocalePick()
{
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = QString(kQmBase) + QLocale(locale).name();
        const QString appDir = QCoreApplication::applicationDirPath();
        if (QFile::exists(appDir + QLatin1String("/i18n/") + baseName + QLatin1String(".qm"))
            || QFile::exists(appDir + QLatin1Char('/') + baseName + QLatin1String(".qm"))
            || QFile::exists(QLatin1String(":/i18n/") + baseName + QLatin1String(".qm"))) {
            return QLocale(locale).name();
        }
    }
    return QString();
}

} // namespace

bool AppSettings::usesPortableUserDataRoots()
{
#if defined(STUDIO_FORCE_ROOT_CACHE) && STUDIO_FORCE_ROOT_CACHE
    return true;
#elif defined(STUDIO_PORTABLE_USER_DATA) && STUDIO_PORTABLE_USER_DATA
    return true;
#else
    return false;
#endif
}

void AppSettings::installAppMetadata()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    QCoreApplication::setOrganizationName(QLatin1String("RCubiq"));
    QCoreApplication::setApplicationName(QLatin1String("Studio"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
#if (defined(STUDIO_FORCE_ROOT_CACHE) && STUDIO_FORCE_ROOT_CACHE)                                     \
    || (defined(STUDIO_PORTABLE_USER_DATA) && STUDIO_PORTABLE_USER_DATA)
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationDirPath());
#endif
}

void AppSettings::bootstrap(QApplication& app)
{
    installAppMetadata();
    applyTranslator();
    applyWidgetTheme(app);
    applyTraceFromSettings();
}

AppThemeMode AppSettings::widgetTheme()
{
    installAppMetadata();
    QSettings st;
    int v = st.value(QLatin1String("appearance/widget_theme"), int(AppThemeMode::System)).toInt();
    if (v < int(AppThemeMode::System) || v > int(AppThemeMode::Dark))
        v = int(AppThemeMode::System);
    return static_cast<AppThemeMode>(v);
}

void AppSettings::setWidgetTheme(AppThemeMode mode)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("appearance/widget_theme"), int(mode));
    st.sync();
}

void AppSettings::applyWidgetTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QLatin1String("Fusion")));
    const AppThemeMode mode = widgetTheme();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QStyleHints* hints = app.styleHints();
#endif

    switch (mode) {
    case AppThemeMode::Dark:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (hints)
            hints->setColorScheme(Qt::ColorScheme::Dark);
#endif
        applyFusionDarkPalette(app);
        break;
    case AppThemeMode::Light:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (hints)
            hints->setColorScheme(Qt::ColorScheme::Light);
#endif
        app.setPalette(QPalette());
        break;
    case AppThemeMode::System:
    default:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (hints)
            hints->setColorScheme(Qt::ColorScheme::Unknown);
#endif
        app.setPalette(QPalette());
        break;
    }
}

QString AppSettings::uiLanguage()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("locale/ui_language"), QLatin1String("system")).toString();
}

void AppSettings::setUiLanguage(const QString& code)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("locale/ui_language"), code);
    st.sync();
}

void AppSettings::applyTranslator()
{
    installAppMetadata();
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app)
        return;

    app->removeTranslator(&g_appTranslator);

    QString code = uiLanguage();
    if (code == QLatin1String("system"))
        code = systemQmLocalePick();

    if (code.isEmpty() || code == QLatin1String("en")) {
        // Английский / нет перевода — только системные строки Qt по умолчанию
    } else {
        const QString baseName = QString(kQmBase) + code;
        const QString appDir = QCoreApplication::applicationDirPath();
        if (g_appTranslator.load(baseName, appDir + QLatin1String("/i18n"))
            || g_appTranslator.load(appDir + QLatin1Char('/') + baseName + QLatin1String(".qm"))
            || g_appTranslator.load(QLatin1String(":/i18n/") + baseName)) {
            app->installTranslator(&g_appTranslator);
        }
    }

    QEvent ev(QEvent::LanguageChange);
    const QWidgetList top = app->topLevelWidgets();
    for (QWidget* w : top)
        QApplication::sendEvent(w, &ev);
}

bool AppSettings::traceProgramFlow()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("diag/trace_verbose"), false).toBool();
}

void AppSettings::setTraceProgramFlow(bool enabled)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("diag/trace_verbose"), enabled);
    st.sync();
    s_traceActive = enabled;
    syncTraceLogWithFlag();
}

bool AppSettings::ignorePluginVersionCheck()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("plugins/ignore_version_check"), false).toBool();
}

void AppSettings::setIgnorePluginVersionCheck(bool on)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("plugins/ignore_version_check"), on);
    st.sync();
}

bool AppSettings::showWelcomeOnStartup()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("behavior/show_welcome_on_startup"), false).toBool();
}

void AppSettings::setShowWelcomeOnStartup(bool enabled)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("behavior/show_welcome_on_startup"), enabled);
    st.sync();
}

bool AppSettings::developerMode()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("behavior/developer_mode"), false).toBool();
}

void AppSettings::setDeveloperMode(bool on)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("behavior/developer_mode"), on);
    st.sync();
}

bool AppSettings::enumerateVideoInputsAtStartup()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("devices/enumerate_video_at_startup"), false).toBool();
}

void AppSettings::setEnumerateVideoInputsAtStartup(bool on)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("devices/enumerate_video_at_startup"), on);
    st.sync();
}

bool AppSettings::startVirtualCameraBridgeAtStartup()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("devices/start_virtual_cam_bridge_at_startup"), false).toBool();
}

void AppSettings::setStartVirtualCameraBridgeAtStartup(bool on)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("devices/start_virtual_cam_bridge_at_startup"), on);
    st.sync();
}

bool AppSettings::showGpuFrameLoadInStatusBar()
{
    installAppMetadata();
    QSettings st;
    return st.value(QLatin1String("diag/show_gpu_frame_load"), true).toBool();
}

void AppSettings::setShowGpuFrameLoadInStatusBar(bool on)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("diag/show_gpu_frame_load"), on);
    st.sync();
}

std::uintptr_t AppSettings::renderAdapterIndex()
{
    installAppMetadata();
    QSettings st;
    return static_cast<std::uintptr_t>(
        st.value(QLatin1String("render/adapter_dxgi_index"), quint64(0)).toULongLong());
}

void AppSettings::setRenderAdapterIndex(std::uintptr_t DXGIOrdinal)
{
    installAppMetadata();
    QSettings st;
    st.setValue(QLatin1String("render/adapter_dxgi_index"), QVariant(static_cast<quint64>(DXGIOrdinal)));
    st.sync();
}

void AppSettings::applyRenderAdapterToDataBus(IDataBus* bus)
{
    if (!bus)
        return;
    // Живой `render_device` после загрузки движка указывает на ID3D11Device*; индекс DXGI хранится в defaultVal.
    if (BusHandle<uintptr_t>* h = bus_handle_cast<uintptr_t>(bus, "render_device"))
        h->defaultVal() = static_cast<uintptr_t>(renderAdapterIndex());
}

QString AppSettings::cacheJsonPath()
{
    return QDir(QDir::currentPath()).filePath(QLatin1String("cache.json"));
}

QStringList AppSettings::cacheJsonCandidatePaths()
{
    QStringList list;
    if (usesPortableUserDataRoots()) {
        const QString portable = QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String("cache.json"));
        list.push_back(portable);
    }
    const QString cur = QDir(QDir::currentPath()).filePath(QLatin1String("cache.json"));
    const QString exe = QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String("cache.json"));
    if (!list.contains(cur))
        list.push_back(cur);
    if (!list.contains(exe))
        list.push_back(exe);
    return list;
}

QString AppSettings::writablePluginsCacheDirectory()
{
    installAppMetadata();
    if (usesPortableUserDataRoots()) {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins_cache"));
        QDir().mkpath(path);
        return QFileInfo(path).absoluteFilePath();
    }
    const QStringList bases = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath(),
    };
    for (const QString& base : bases) {
        if (base.isEmpty())
            continue;
        const QString path = QDir(base).filePath(QStringLiteral("plugins_cache"));
        QFileInfo fi(path);
        const QString parentPath = fi.absolutePath();
        if (!QDir(parentPath).exists()) {
            if (!QDir().mkpath(parentPath))
                continue;
        }
        if (!fi.exists()) {
            if (!QDir().mkpath(path))
                continue;
        }
        const QFileInfo dirMeta(path);
        if (dirMeta.exists() && dirMeta.isWritable())
            return QFileInfo(path).absoluteFilePath();
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    const QString fallback = QDir(base).filePath(QStringLiteral("plugins_cache"));
    QDir().mkpath(fallback);
    return QFileInfo(fallback).absoluteFilePath();
}

QString AppSettings::writableSessionCachePath()
{
    installAppMetadata();
    if (usesPortableUserDataRoots()) {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String("session_cache.json"));
        const QFileInfo fi(path);
        QDir().mkpath(fi.absolutePath());
        return fi.absoluteFilePath();
    }
    const QStringList cands = cacheJsonCandidatePaths();
    for (const QString& path : cands) {
        const QFileInfo fi(path);
        QDir parentDir = fi.absoluteDir();
        if (!parentDir.exists()) {
            if (!QDir().mkpath(parentDir.absolutePath()))
                continue;
        }
        const QFileInfo dirMeta(parentDir.absolutePath());
        if (!dirMeta.exists() || !dirMeta.isWritable())
            continue;
        return fi.absoluteFilePath();
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    const QString fallback =
        QDir(base).filePath(QStringLiteral("session_cache.json"));
    const QFileInfo fb(fallback);
    QDir().mkpath(fb.absolutePath());
    return QFileInfo(fallback).absoluteFilePath();
}

QStringList AppSettings::cacheJsonCandidatePathsIncludingWritableFallback()
{
    QStringList list = cacheJsonCandidatePaths();
    const QString w = writableSessionCachePath();
    if (!w.isEmpty() && !list.contains(w))
        list.append(w);
    return list;
}

void AppSettings::restoreFactoryDefaults(QApplication& app)
{
    installAppMetadata();
    QSettings st;
    st.remove(QLatin1String("appearance/widget_theme"));
    st.remove(QLatin1String("locale/ui_language"));
    st.remove(QLatin1String("diag/trace_verbose"));
    st.remove(QLatin1String("plugins/ignore_version_check"));
    st.remove(QLatin1String("behavior/show_welcome_on_startup"));
    st.remove(QLatin1String("behavior/developer_mode"));
    st.remove(QLatin1String("devices/enumerate_video_at_startup"));
    st.remove(QLatin1String("devices/start_virtual_cam_bridge_at_startup"));
    st.remove(QLatin1String("diag/show_gpu_frame_load"));
    st.remove(QLatin1String("render/adapter_dxgi_index"));
    st.sync();
    applyTraceFromSettings();
    reapplyPersistedAppearance(app);
}

void AppSettings::reapplyPersistedAppearance(QApplication& app)
{
    applyTranslator();
    applyWidgetTheme(app);
}

QString AppSettings::settingsFilePath()
{
    installAppMetadata();
    return QSettings().fileName();
}
