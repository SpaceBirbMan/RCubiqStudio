#include "settingsdialog.h"

#include "appcore.h"
#include "appsettings.h"
#include "crashhandler.h"
#include "gpuadapters.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QUrl>
#include <QCheckBox>
#include <QVariant>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>

#include <QtGlobal>

#include <cstdint>
#include <utility>

namespace {

QColor mutedHintColor(const QPalette& p)
{
    const QColor wt = p.color(QPalette::WindowText);
    const QColor wb = p.color(QPalette::Window);
    return QColor((wt.red() * 88 + wb.red() * 12) / 100,
                  (wt.green() * 88 + wb.green() * 12) / 100,
                  (wt.blue() * 88 + wb.blue() * 12) / 100);
}

QLabel* makeHint(QWidget* parent, const QString& text, QVector<QLabel*>* registry)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setWordWrap(true);
    lbl->setAutoFillBackground(false);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QFont f = lbl->font();
    QFont sf = f;
    if (sf.pointSizeF() > 0.)
        sf.setPointSizeF(qMax(7., sf.pointSizeF() - 1.));
    lbl->setFont(sf);
    if (registry)
        registry->push_back(lbl);
    return lbl;
}

QFrame* sep(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent, AppCore* core)
    : QDialog(parent)
    , m_core(core)
{
    setWindowTitle(tr("Settings"));
    resize(560, 640);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* holder = new QWidget;
    scroll->setWidget(holder);
    outer->addWidget(scroll);

    auto* root = new QVBoxLayout(holder);
    root->setSpacing(14);

    // --- Appearance ---
    auto* grpLook = new QGroupBox(tr("Appearance"), holder);
    auto* layLook = new QVBoxLayout(grpLook);
    layLook->addWidget(new QLabel(tr("Widget theme"), grpLook));

    m_theme = new QComboBox(grpLook);
    m_theme->addItem(tr("Follow system"), int(AppThemeMode::System));
    m_theme->addItem(tr("Light"), int(AppThemeMode::Light));
    m_theme->addItem(tr("Dark"), int(AppThemeMode::Dark));
    layLook->addWidget(m_theme);

    m_themeHint = makeHint(grpLook,
        tr("Applies Fusion style palette to dock panels, menus, and dialogs. "
           "\"Follow system\" follows the operating system color scheme where available; "
           "Light and Dark fix a predictable Fusion look."),
        &m_hintLabels);
    layLook->addWidget(m_themeHint);

    layLook->addWidget(sep(grpLook));

    layLook->addWidget(new QLabel(tr("Interface language"), grpLook));

    m_language = new QComboBox(grpLook);
    m_language->addItem(tr("Automatic (follow system locales)"), QStringLiteral("system"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->addItem(QStringLiteral("Русский"), QStringLiteral("ru_RU"));
    layLook->addWidget(m_language);

    m_languageHint = makeHint(
        grpLook,
        tr("Loads translation files from the program directory: place them in the i18n subfolder "
           "using the usual Qt naming pattern — prefix, locale code, and the compiled qm extension. "
           "The main window updates when the language changes; close and reopen other dialogs if labels "
           "stay outdated."),
        &m_hintLabels);
    layLook->addWidget(m_languageHint);

    root->addWidget(grpLook);

    // --- Plugins ---
    auto* grpPlug = new QGroupBox(tr("Plugins"), holder);
    auto* layPlug = new QVBoxLayout(grpPlug);

    m_ignorePluginVersion = new QCheckBox(
        tr("Ignore plugin ABI / version compatibility check"), grpPlug);
    layPlug->addWidget(m_ignorePluginVersion);

    m_ignorePluginHint = makeHint(
        grpPlug,
        tr("Reserved for stricter checks when loading add-on libraries and packages. "
           "If enabled, the loader skips compatibility checks (higher risk of crashes when a plugin "
           "does not match this build)."),
        &m_hintLabels);
    layPlug->addWidget(m_ignorePluginHint);

    m_enumVideoStartup = new QCheckBox(
        tr("Enumerate video capture devices when the application starts"), grpPlug);
    layPlug->addWidget(m_enumVideoStartup);
    m_enumVideoStartupHint = makeHint(
        grpPlug,
        tr("When off (default), DirectShow camera names are not queried at startup. "
           "The list is built when a plugin or the session needs a camera."),
        &m_hintLabels);
    layPlug->addWidget(m_enumVideoStartupHint);

    m_virtualCamStartup = new QCheckBox(
        tr("Start virtual camera output at startup"), grpPlug);
    layPlug->addWidget(m_virtualCamStartup);
    m_virtualCamStartupHint = makeHint(
        grpPlug,
        tr("Sends the viewport to the OBS Virtual Camera so other programs can use it as a "
           "video source. Requires OBS Studio and the OBS virtual camera to be registered in "
           "Windows (see scripts/register_obs_virtualcam.bat). Disable if you do not need this "
           "output — takes effect after restart."),
        &m_hintLabels);
    layPlug->addWidget(m_virtualCamStartupHint);
    root->addWidget(grpPlug);

    auto* grpRen = new QGroupBox(tr("Rendering (engine)"), holder);
    auto* layRen = new QVBoxLayout(grpRen);
    layRen->addWidget(new QLabel(tr("GPU adapter for rendering"), grpRen));
    m_gpuAdapter = new QComboBox(grpRen);
    layRen->addWidget(m_gpuAdapter);
    m_gpuAdapterHint = makeHint(
        grpRen,
        tr("On Windows this list comes from DXGI. The adapter index is written to the shared data-bus "
           "slot \"render_device\" so the active engine can choose the GPU. Restart the engine after "
           "changing this value."),
        &m_hintLabels);
    layRen->addWidget(m_gpuAdapterHint);
    root->addWidget(grpRen);

    // --- Diagnostics ---
    auto* grpDiag = new QGroupBox(tr("Diagnostics"), holder);
    auto* layDiag = new QVBoxLayout(grpDiag);

    m_traceFlow = new QCheckBox(tr("Enable detailed activity tracing"), grpDiag);
    layDiag->addWidget(m_traceFlow);

    m_traceHint = makeHint(
        grpDiag,
        tr("Writes timed diagnostic lines (with file and line when available) to a log file next to "
           "the settings file. If the program is launched from a terminal, the same lines may also "
           "appear there."),
        &m_hintLabels);
    layDiag->addWidget(m_traceHint);

    m_showGpuLoad = new QCheckBox(tr("Show GPU frame-time load in the status bar"), grpDiag);
    layDiag->addWidget(m_showGpuLoad);
    m_showGpuLoadHint = makeHint(
        grpDiag,
        tr("The active engine publishes GPU workload on the shared data-bus slot \"engine_gpu_load\". "
           "Values from 0 through 1 are treated as a fraction of GPU time relative to the frame interval "
           "and are converted to a percentage; larger values are shown unchanged (engine-dependent). "
           "This is not Task Manager GPU occupancy."),
        &m_hintLabels);
    layDiag->addWidget(m_showGpuLoadHint);

    auto* crashBtnRow = new QHBoxLayout();
    auto* crashBtn = new QPushButton(tr("Open crash reports folder…"), grpDiag);
    crashBtnRow->addWidget(crashBtn);
    crashBtnRow->addStretch();
    layDiag->addLayout(crashBtnRow);

    layDiag->addWidget(makeHint(grpDiag,
        tr("Opens the folder where crash logs are stored (subfolder next to the executable). "
           "The crash handler writes files there when recording a failure."),
        &m_hintLabels));

    root->addWidget(grpDiag);

    // --- Behavior ---
    auto* grpBeh = new QGroupBox(tr("Startup and advanced"), holder);
    auto* layBeh = new QVBoxLayout(grpBeh);

    m_welcomeStartup = new QCheckBox(tr("Show welcome window on startup"), grpBeh);
    layBeh->addWidget(m_welcomeStartup);

    m_welcomeHint = makeHint(
        grpBeh,
        tr("Saved in the settings file for a future welcome screen. Disabled here until the "
           "feature exists; the value is not applied yet."),
        &m_hintLabels);
    layBeh->addWidget(m_welcomeHint);

    m_developerMode = new QCheckBox(tr("Developer mode"), grpBeh);
    layBeh->addWidget(m_developerMode);

    m_developerHint = makeHint(
        grpBeh,
        tr("Experimental interface and diagnostics for add-ons. Some plugins may expose extra "
           "controls only with this enabled."),
        &m_hintLabels);
    layBeh->addWidget(m_developerHint);

    root->addWidget(grpBeh);

    // --- Danger zone ---
    auto* grpMaintain = new QGroupBox(tr("Maintenance"), holder);
    auto* layMaintain = new QVBoxLayout(grpMaintain);

    auto* cacheBtn = new QPushButton(tr("Clear session cache"), grpMaintain);
    layMaintain->addWidget(cacheBtn);
    layMaintain->addWidget(makeHint(
        grpMaintain,
        tr("Deletes cache data for stored module state (engines, trackers, hosted add-ons). "
           "Files are looked for in the current working directory and next to the executable. "
           "Disconnect devices first if restoring from cache caused problems."),
        &m_hintLabels));
    layMaintain->addWidget(makeHint(
        grpMaintain,
        tr("Preferences are stored in: %1").arg(AppSettings::settingsFilePath()),
        &m_hintLabels));

    auto* defaultsBtn = new QPushButton(tr("Reset application preferences to defaults"), grpMaintain);
    layMaintain->addWidget(defaultsBtn);
    layMaintain->addWidget(makeHint(
        grpMaintain,
        tr("Clears values in the settings file (themes and options from this dialog). "
           "Does not remove the session cache — use the button above for that."),
        &m_hintLabels));

    root->addWidget(grpMaintain);
    root->addStretch(1);

    const auto bboxButtons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply;
    auto* bbox = new QDialogButtonBox(QDialogButtonBox::StandardButtons(bboxButtons), this);

    QAbstractButton* applyBtn = bbox->button(QDialogButtonBox::Apply);
    if (applyBtn)
        QObject::connect(applyBtn, &QAbstractButton::clicked, this, &SettingsDialog::onApplyClicked);
    QObject::connect(bbox, &QDialogButtonBox::accepted, this, [this]() {
        saveFromUi();
        accept();
    });
    QObject::connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    outer->addWidget(bbox);

    QObject::connect(crashBtn, &QPushButton::clicked, this, &SettingsDialog::openCrashReportsFolderClicked);
    QObject::connect(cacheBtn, &QPushButton::clicked, this, &SettingsDialog::clearCacheClicked);
    QObject::connect(defaultsBtn, &QPushButton::clicked, this, &SettingsDialog::onResetDefaultsClicked);

    loadFromSettings();

    m_welcomeStartup->setEnabled(false);

    refreshHintColors();
}

void SettingsDialog::loadFromSettings()
{
    refreshGpuAdapterList();
    const AppThemeMode th = AppSettings::widgetTheme();
    for (int i = 0; i < m_theme->count(); ++i) {
        if (m_theme->itemData(i).toInt() == int(th)) {
            m_theme->setCurrentIndex(i);
            break;
        }
    }

    const QString lang = AppSettings::uiLanguage();
    for (int i = 0; i < m_language->count(); ++i) {
        if (m_language->itemData(i).toString() == lang) {
            m_language->setCurrentIndex(i);
            break;
        }
    }

    m_ignorePluginVersion->setChecked(AppSettings::ignorePluginVersionCheck());
    m_traceFlow->setChecked(AppSettings::traceProgramFlow());
    m_welcomeStartup->setChecked(AppSettings::showWelcomeOnStartup());
    m_developerMode->setChecked(AppSettings::developerMode());
    m_enumVideoStartup->setChecked(AppSettings::enumerateVideoInputsAtStartup());
    m_virtualCamStartup->setChecked(AppSettings::startVirtualCameraBridgeAtStartup());
    m_showGpuLoad->setChecked(AppSettings::showGpuFrameLoadInStatusBar());
}

void SettingsDialog::saveFromUi()
{
    AppSettings::setWidgetTheme(static_cast<AppThemeMode>(m_theme->currentData().toInt()));
    AppSettings::setUiLanguage(m_language->currentData().toString());
    AppSettings::setIgnorePluginVersionCheck(m_ignorePluginVersion->isChecked());
    AppSettings::setTraceProgramFlow(m_traceFlow->isChecked());
    AppSettings::setDeveloperMode(m_developerMode->isChecked());
    AppSettings::setEnumerateVideoInputsAtStartup(m_enumVideoStartup->isChecked());
    AppSettings::setStartVirtualCameraBridgeAtStartup(m_virtualCamStartup->isChecked());
    AppSettings::setShowGpuFrameLoadInStatusBar(m_showGpuLoad->isChecked());
    AppSettings::setRenderAdapterIndex(static_cast<std::uintptr_t>(
        m_gpuAdapter->currentData().toULongLong()));
    if (m_core)
        AppSettings::applyRenderAdapterToDataBus(m_core->getEventManager().getBusPtr());

    if (QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        AppSettings::applyWidgetTheme(*app);
        AppSettings::applyTranslator();
    }

    refreshHintColors();
}

void SettingsDialog::refreshGpuAdapterList()
{
    if (!m_gpuAdapter)
        return;
    const std::uintptr_t saved = AppSettings::renderAdapterIndex();
    m_gpuAdapter->blockSignals(true);
    m_gpuAdapter->clear();
#if defined(Q_OS_WIN)
    const QVector<GpuAdapters::Entry> list = GpuAdapters::enumerateAdapters();
    if (list.isEmpty()) {
        m_gpuAdapter->addItem(tr("Primary adapter ( DXGI slot 0 )"),
                              QVariant(static_cast<qulonglong>(0)));
    } else {
        for (const GpuAdapters::Entry& e : list) {
            QString text = e.name + QLatin1String(" (DXGI ")
                + QString::number(static_cast<qulonglong>(e.index)) + QLatin1Char(')');
            m_gpuAdapter->addItem(text, QVariant(static_cast<qulonglong>(e.index)));
        }
    }
#else
    m_gpuAdapter->addItem(tr("System default"), QVariant(static_cast<qulonglong>(0)));
#endif
    m_gpuAdapter->blockSignals(false);

    int best = 0;
    for (int i = 0; i < m_gpuAdapter->count(); ++i) {
        if (static_cast<std::uintptr_t>(m_gpuAdapter->itemData(i).toULongLong()) == saved) {
            best = i;
            break;
        }
    }
    m_gpuAdapter->setCurrentIndex(best);
}

void SettingsDialog::onApplyClicked()
{
    saveFromUi();
}

void SettingsDialog::onResetDefaultsClicked()
{
    const int ans = QMessageBox::question(this, tr("Reset preferences"),
        tr("Revert all preferences in this window to defaults?"));

    if (ans != QMessageBox::Yes)
        return;

    if (QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance()))
        AppSettings::restoreFactoryDefaults(*app);

    loadFromSettings();

    refreshHintColors();
}

void SettingsDialog::clearCacheClicked()
{
    const QStringList paths = AppSettings::cacheJsonCandidatePathsIncludingWritableFallback();

    QString message = tr("Delete cache files?");
    QString detail;
    for (const QString& p : paths) {
        if (QFile::exists(p))
            detail += QLatin1Char('\n') + p;
    }
    if (detail.isEmpty()) {
        QMessageBox::information(this, tr("Clear cache"),
            tr("No cache.json files were found."));
        return;
    }
    message += detail;

    if (QMessageBox::question(this, tr("Clear cache"), message) != QMessageBox::Yes)
        return;

    int removed = 0;
    for (const QString& p : paths) {
        QFile f(p);
        if (f.exists() && f.remove())
            ++removed;
    }

    QMessageBox::information(this, tr("Clear cache"),
        tr("Removed %1 cache file(s). Restart or reload scenes for a clean slate.").arg(removed));
}

void SettingsDialog::openCrashReportsFolderClicked()
{
    if (!m_core)
        return;
    const QString dir = m_core->getCrashHandler().crashLogsDirectory();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void SettingsDialog::refreshHintColors()
{
    const QColor c = mutedHintColor(QGuiApplication::palette());
    for (QLabel* lbl : std::as_const(m_hintLabels)) {
        if (!lbl)
            continue;
        QPalette lp = lbl->palette();
        lp.setColor(QPalette::Active, QPalette::WindowText, c);
        lp.setColor(QPalette::Inactive, QPalette::WindowText, c);
        lbl->setPalette(lp);
    }
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        || event->type() == QEvent::ThemeChange
#endif
    )
        refreshHintColors();
    QDialog::changeEvent(event);
}

