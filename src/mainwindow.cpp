#include "consts.h"
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QQuickView>
#include <QTableWidget>
#include <QHeaderView>
#include "viewportwidget.h"
// #include "rcqvirtualcamera.h"
#include "obsvirtualcamera.h"
#include "uirenderer.h"
#include "trackerrenderer.h"
#include <QDebug>
#include <QTimer>
#include "databus.h"
#include "bushandle.h"
#include <qobjectdefs.h>
#include <qthread.h>
#include <QMetaObject>
#include <QLabel>
#include <QVBoxLayout>
#include <QMenu>
#include <QCursor>
#include <QPalette>
#include <QEvent>
#include <QDockWidget>
#include <QDir>
#include <QFileInfo>
#include <any>
#include <atomic>
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

bool pluginPathsMatch(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    const QString ca = QDir::cleanPath(a);
    const QString cb = QDir::cleanPath(b);
    if (ca == cb)
        return true;
#if defined(Q_OS_WIN)
    if (ca.compare(cb, Qt::CaseInsensitive) == 0)
        return true;
#endif
    const QFileInfo fa(ca), fb(cb);
    if (fa.exists() && fb.exists()) {
        const QString c1 = fa.canonicalFilePath();
        const QString c2 = fb.canonicalFilePath();
        if (!c1.isEmpty() && !c2.isEmpty() && c1 == c2)
            return true;
    }
    return false;
}

bool subtreeHasPluginPath(QWidget* root, const QString& needlePath)
{
    if (!root)
        return false;
    const QVariant v = root->property("m3_plugin_library_path");
    if (v.isValid() && pluginPathsMatch(v.toString(), needlePath))
        return true;
    for (QObject* ch : root->children()) {
        if (auto* cw = qobject_cast<QWidget*>(ch)) {
            if (subtreeHasPluginPath(cw, needlePath))
                return true;
        }
    }
    return false;
}

} // namespace

MainWindow::MainWindow(QWidget *parent, AppCore *core)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    this->core = core;
    ui->setupUi(this);

    ui->centralwidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->centralwidget->setMaximumHeight(ui->centralwidget->sizeHint().height());

    ui->dockWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->dockWidgetViewport->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->dockWidget_3->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // Horizontal dock widths must scale with *window width*. Using height() here made tall
    // windows assign huge side dock widths and the right panel could not be narrowed.
    int winW = width();
    if (winW < 200)
        winW = geometry().width() > 200 ? geometry().width() : 1000;

    const int sideSize = (std::max)(140, static_cast<int>(winW * 0.18));
    const int centerSize = (std::max)(280, winW - 2 * sideSize);

    QList<QDockWidget*> docks = {ui->dockWidget, ui->dockWidgetViewport, ui->dockWidget_3};
    resizeDocks(docks, {sideSize, centerSize, sideSize}, Qt::Horizontal);

    ui->dockWidget->setMinimumWidth(120);
    ui->dockWidgetViewport->setMinimumWidth(200);
    ui->dockWidget_3->setMinimumWidth(120);

    ui->rightPanel->setMinimumWidth(0);
    if (ui->scrollAreaWidgetContents)
        ui->scrollAreaWidgetContents->setMinimumWidth(0);
    if (ui->scrollAreaWidgetContents_2)
        ui->scrollAreaWidgetContents_2->setMinimumWidth(0);
    if (ui->scrollAreaWidgetContents_3)
        ui->scrollAreaWidgetContents_3->setMinimumWidth(0);

    _updateTimer = nullptr;
    _trackerTableCache = nullptr;
    trackerRenderer = nullptr;

    // Event subscriptions
    core->getEventManager().subscribe("cache_err", &MainWindow::showCacheErrorMessage, this);
    core->getEventManager().subscribe("send_control_table", &MainWindow::setControlsTable, this);
    core->getEventManager().subscribe<PluginUiEngineTrees>("init_ui_eng", &MainWindow::initDynamicUi, this);
    core->getEventManager().subscribe<PluginUiTrackerTrees>("init_ui_tracker", &MainWindow::initTrackerDynamicUi, this);
    core->getEventManager().subscribe("initialize", &MainWindow::initialize, this);
    core->getEventManager().subscribe(name, "send_table", &MainWindow::initTrackerTable, this);

    // Plugin UI subscriptions (all three types)
    core->getEventManager().subscribe(name, "engine_ui_ready",      &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "tracker_ui_ready",     &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "gen_plugin_ui_ready",  &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "engine_ui_removed",    &MainWindow::uiRemovePluginEntry, this);
    core->getEventManager().subscribe(name, "tracker_ui_removed",   &MainWindow::uiRemovePluginEntry, this);
    core->getEventManager().subscribe(name, "gen_plugin_ui_removed",&MainWindow::uiRemovePluginEntry, this);

    core->getEventManager().subscribe(name, M3Events::kPluginRuntimeTeardown, &MainWindow::uiTeardownPluginTabs, this);

    // Active state restore signals
    core->getEventManager().subscribe(name, "engine_set_active",       &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "engine_set_inactive",     &MainWindow::uiSetPluginInactive, this);
    core->getEventManager().subscribe(name, "tracker_set_active",      &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "tracker_set_inactive",    &MainWindow::uiSetPluginInactive, this);
    core->getEventManager().subscribe(name, "gen_plugin_activated",    &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "gen_plugin_deactivated",  &MainWindow::uiSetPluginInactive, this);

    // Button connections
    connect(ui->newSetupMenuButton,    &QAction::triggered,  this, &MainWindow::onNewFileClicked);
    connect(ui->saveAsSetupMenuButton,   &QAction::triggered,  this, &MainWindow::onSaveFileClicked);
    connect(ui->action_Render_API,    &QAction::triggered,  this, &MainWindow::setRenderApi);
    connect(ui->addPlugin,            &QPushButton::clicked, this, &MainWindow::addPlugin);
    connect(ui->deletePlugin,         &QPushButton::clicked, this, &MainWindow::removePlugin);
    connect(ui->reloadPlugin,         &QPushButton::clicked, this, &MainWindow::reloadPlugin);
    connect(ui->reloadAllPlugins,     &QPushButton::clicked, this, &MainWindow::reloadAllPlugins);

    // Clear the placeholder page from QToolBox
    while (ui->pluginsToolBox->count() > 0) {
        QWidget *widget = ui->pluginsToolBox->widget(0);
        ui->pluginsToolBox->removeItem(0);
        delete widget;
    }

    // Replace placeholder viewport with the real ViewportWidget
    ViewportWidget* vw = new ViewportWidget(core, this);
    ui->dockWidgetViewport->setWidget(vw);
    delete ui->viewport;
    ui->viewport = vw;

    // --- akvirtualcamera (commented out, replaced by OBS virtual camera) ---
    // m_rcqVirtualCam = std::make_unique<RcqVirtualCamera>();
    // if (m_rcqVirtualCam->loadFromApplicationDir() && m_rcqVirtualCam->startStream(RcqVirtualCamera::kDefaultDeviceId)) {
    //     vw->setAfterFrameCallback([this, vw]() {
    //         if (m_rcqVirtualCam && m_rcqVirtualCam->isStreaming()) {
    //             m_rcqVirtualCam->pushFrameFromWidget(vw);
    //         }
    //     });
    // } else {
    //     m_rcqVirtualCam.reset();
    // }

    // --- OBS Virtual Camera (DirectShow, shared memory, no extra DLL needed) ---
    m_obsVirtualCam = std::make_unique<ObsVirtualCamera>();
    if (m_obsVirtualCam->startStream(640, 480, 30)) {
        vw->setAfterFrameCallback([this, vw]() {
            if (m_obsVirtualCam && m_obsVirtualCam->isStreaming()) {
                m_obsVirtualCam->pushFrameFromWidget(vw);
            }
        });
    } else {
        m_obsVirtualCam.reset();
    }

    setupViewMenuDockToggles();

    ui->gpu_label->setVisible(false);
    auto* resTimer = new QTimer(this);
    connect(resTimer, &QTimer::timeout, this, &MainWindow::updateResourceLabels);
    resTimer->start(1000);
    updateResourceLabels();
}

void MainWindow::setupViewMenuDockToggles()
{
    const auto bind = [this](QDockWidget* dock, QAction* act) {
        connect(dock, &QDockWidget::visibilityChanged, this, [act](bool visible) {
            if (act->isChecked() == visible)
                return;
            act->blockSignals(true);
            act->setChecked(visible);
            act->blockSignals(false);
        });
        connect(act, &QAction::toggled, dock, &QDockWidget::setVisible);
        act->blockSignals(true);
        act->setChecked(dock->isVisible());
        act->blockSignals(false);
    };
    bind(ui->dockWidget, ui->actionViewDockEngine);
    bind(ui->dockWidgetViewport, ui->actionViewDockViewport);
    bind(ui->dockWidget_3, ui->actionViewDockPlugins);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateResourceLabels();
        for (auto& entry : pluginPageEntries) {
            if (entry.second.checkBox)
                entry.second.checkBox->setText(tr("Active"));
        }
        if (ui->tableTrackerWidget && ui->tableTrackerWidget->columnCount() >= 2) {
            ui->tableTrackerWidget->setHorizontalHeaderLabels({tr("Parameter"), tr("Value")});
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::removeTabsOwnedByLibraryPath(QTabWidget* tabs, const std::string& libraryPath)
{
    if (!tabs || libraryPath.empty())
        return;
    const QString needle = QString::fromStdString(libraryPath);
    for (int i = tabs->count() - 1; i >= 0; --i) {
        QWidget* w = tabs->widget(i);
        if (!w)
            continue;
        if (subtreeHasPluginPath(w, needle)) {
            tabs->removeTab(i);
            delete w;
        }
    }
}

void MainWindow::uiTeardownPluginTabs(std::string path)
{
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, p = std::move(path)]() mutable {
            uiTeardownPluginTabs(std::move(p));
        }, Qt::BlockingQueuedConnection);
        return;
    }
    if (lastRenderedEngineLibraryPath == path)
        lastRenderedEngineLibraryPath.clear();
    removeTabsOwnedByLibraryPath(ui->leftPanel, path);
    removeTabsOwnedByLibraryPath(ui->rightPanel, path);
}

MainWindow::~MainWindow()
{
    // m_rcqVirtualCam.reset();
    m_obsVirtualCam.reset();
    if (currentCamera) {
        currentCamera->close();
    }
    delete ui;
}

void MainWindow::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "get_video_devices_request", 0));
}

// ─── Plugin management ────────────────────────────────────────────────────────

void MainWindow::addPlugin() {
    QMenu menu(this);
    QAction* actEngine    = menu.addAction(tr("Add engine"));
    QAction* actTracker   = menu.addAction(tr("Add tracker"));
    QAction* actGenPlugin = menu.addAction(tr("Add plugin"));

    QAction* chosen = menu.exec(QCursor::pos());

    if      (chosen == actEngine)    addEnginePlugin();
    else if (chosen == actTracker)   addTrackerPlugin();
    else if (chosen == actGenPlugin) addGenPlugin();
}

void MainWindow::addEnginePlugin() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        ui->centralwidget,
        tr("Choose engine library"),
        QDir::homePath(),
        tr("Dynamic libraries (*.dll);;All files (*)")
    );
    std::vector<std::string> paths;
    for (const QString& f : fileNames)
        paths.emplace_back(f.toStdString());
    if (!paths.empty())
        core->getEventManager().sendMessage(AppMessage(name, "add_engines_names", paths));
}

void MainWindow::addTrackerPlugin() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        ui->centralwidget,
        tr("Choose tracker library"),
        QDir::homePath(),
        tr("Dynamic libraries (*.dll);;All files (*)")
    );
    std::vector<std::string> paths;
    for (const QString& f : fileNames)
        paths.emplace_back(f.toStdString());
    if (!paths.empty())
        core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", paths));
}

void MainWindow::addGenPlugin() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        ui->centralwidget,
        tr("Choose plugin package"),
        QDir::homePath(),
        tr("Plugin packages (*.ofp);;All files (*)")
    );
    std::vector<std::string> paths;
    for (const QString& f : fileNames)
        paths.emplace_back(f.toStdString());
    if (!paths.empty())
        core->getEventManager().sendMessage(AppMessage(name, "add_plugins_to_registry", paths));
}

void MainWindow::removePlugin() {
    int currentIndex = ui->pluginsToolBox->currentIndex();
    if (currentIndex < 0) {
        qDebug() << "[UI] removePlugin: no page selected";
        return;
    }
    QWidget* widget = ui->pluginsToolBox->widget(currentIndex);
    if (!widget) {
        qDebug() << "[UI] removePlugin: widget is null";
        return;
    }

    QString pathQ = widget->property("pluginPath").toString();
    if (pathQ.isEmpty()) {
        qDebug() << "[UI] removePlugin: pluginPath property is empty";
        return;
    }

    std::string path = pathQ.toStdString();
    int typeInt = widget->property("pluginType").toInt();
    PluginUIType type = static_cast<PluginUIType>(typeInt);

    switch (type) {
        case PluginUIType::Engine:
            core->getEventManager().sendMessage(AppMessage(name, "remove_engine", path));
            break;
        case PluginUIType::Tracker:
            core->getEventManager().sendMessage(AppMessage(name, "remove_tracker", path));
            break;
        case PluginUIType::Generic:
            core->getEventManager().sendMessage(AppMessage(name, "remove_gen_plugin", path));
            break;
    }
}

void MainWindow::uiAddPluginEntry(PluginUIInfo info) {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, info]() {
            this->uiAddPluginEntry(info);
        }, Qt::QueuedConnection);
        return;
    }

    // Duplicate protection
    if (pluginPageEntries.count(info.path)) {
        qDebug() << "[UI] Duplicate plugin page skipped:" << QString::fromStdString(info.path);
        return;
    }

    // Choose background color by type
    QColor bgColor;
    switch (info.type) {
        case PluginUIType::Engine:  bgColor = QColor(255, 250, 205); break; // light yellow (lemonchiffon)
        case PluginUIType::Tracker: bgColor = QColor(198, 239, 206); break; // light green
        case PluginUIType::Generic: bgColor = QColor(255, 205, 210); break; // light red/pink
    }

    // Build the page widget
    QWidget* widget = new QWidget;
    widget->setAutoFillBackground(true);
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, bgColor);
    widget->setPalette(pal);

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Display name (shortened path)
    QString displayName = QString::fromStdString(info.name);
    QFileInfo fi(displayName);
    if (fi.exists() || displayName.contains('/') || displayName.contains('\\')) {
        displayName = fi.fileName(); // show only filename
    }
    QLabel* nameLabel = new QLabel("<b>" + displayName + "</b>");
    nameLabel->setWordWrap(true);
    layout->addWidget(nameLabel);

    // Full path in smaller text
    QLabel* pathLabel = new QLabel(QString::fromStdString(info.path));
    pathLabel->setWordWrap(true);
    QFont smallFont = pathLabel->font();
    smallFont.setPointSize(smallFont.pointSize() - 1);
    pathLabel->setFont(smallFont);
    layout->addWidget(pathLabel);

    // Checkbox for enable/disable
    QCheckBox* checkBox = new QCheckBox(tr("Active"));
    layout->addWidget(checkBox);
    layout->addStretch();

    // Store path/type as widget properties for removePlugin()
    widget->setProperty("pluginPath", QString::fromStdString(info.path));
    widget->setProperty("pluginType", static_cast<int>(info.type));

    // Connect checkbox behavior per type
    if (info.type == PluginUIType::Engine) {
        engineCheckboxes.push_back(checkBox);
        connect(checkBox, &QCheckBox::toggled, this,
                [this, path = info.path, checkBox](bool checked) {
            if (checked) {
                // Exclusive: deactivate other loaded engines (вкладки + выгрузка DLL)
                for (QCheckBox* cb : engineCheckboxes) {
                    if (cb != checkBox && cb->isChecked()) {
                        std::string prevPath;
                        for (const auto& [p, ent] : pluginPageEntries) {
                            if (ent.checkBox == cb && ent.type == PluginUIType::Engine) {
                                prevPath = p;
                                break;
                            }
                        }
                        cb->blockSignals(true);
                        cb->setChecked(false);
                        cb->blockSignals(false);
                        if (!prevPath.empty())
                            core->getEventManager().sendMessage(
                                AppMessage(name, "deactivate_engine_by_path", prevPath));
                    }
                }
                core->getEventManager().sendMessage(AppMessage(name, "activate_engine_by_path", path));
            } else {
                core->getEventManager().sendMessage(AppMessage(name, "deactivate_engine_by_path", path));
            }
        });
    } else if (info.type == PluginUIType::Tracker) {
        connect(checkBox, &QCheckBox::toggled, this,
                [this, path = info.path](bool checked) {
            if (checked)
                core->getEventManager().sendMessage(AppMessage(name, "activate_tracker_by_path", path));
            else
                core->getEventManager().sendMessage(AppMessage(name, "deactivate_tracker_by_path", path));
        });
    } else {
        // Generic plugin — checkbox is interactive (enable/disable)
        connect(checkBox, &QCheckBox::toggled, this,
                [this, path = info.path](bool checked) {
            if (checked)
                core->getEventManager().sendMessage(AppMessage(name, "enable_gen_plugin", path));
            else
                core->getEventManager().sendMessage(AppMessage(name, "disable_gen_plugin", path));
        });
        // Initially unchecked — will be checked when gen_plugin_activated arrives
        checkBox->setChecked(false);
    }

    // Register entry
    PluginPageEntry entry;
    entry.path     = info.path;
    entry.type     = info.type;
    entry.checkBox = checkBox;
    pluginPageEntries[info.path] = entry;

    // Add to toolbox (use display name as page title)
    ui->pluginsToolBox->addItem(widget, displayName);
}

void MainWindow::uiSetPluginActive(std::string path) {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, path]() { uiSetPluginActive(path); }, Qt::QueuedConnection);
        return;
    }
    auto it = pluginPageEntries.find(path);
    if (it == pluginPageEntries.end()) return;
    QCheckBox* cb = it->second.checkBox;
    if (!cb) return;
    cb->blockSignals(true);
    cb->setChecked(true);
    cb->blockSignals(false);
    // For engines: also enforce exclusivity (uncheck all other engine checkboxes)
    if (it->second.type == PluginUIType::Engine) {
        for (QCheckBox* other : engineCheckboxes) {
            if (other != cb && other->isChecked()) {
                other->blockSignals(true);
                other->setChecked(false);
                other->blockSignals(false);
            }
        }
    }
}

void MainWindow::uiSetPluginInactive(std::string path) {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, path]() { uiSetPluginInactive(path); }, Qt::QueuedConnection);
        return;
    }
    auto it = pluginPageEntries.find(path);
    if (it == pluginPageEntries.end()) return;
    QCheckBox* cb = it->second.checkBox;
    if (!cb) return;
    cb->blockSignals(true);
    cb->setChecked(false);
    cb->blockSignals(false);
}

void MainWindow::reloadPlugin() {
    int currentIndex = ui->pluginsToolBox->currentIndex();
    if (currentIndex < 0) return;
    QWidget* widget = ui->pluginsToolBox->widget(currentIndex);
    if (!widget) return;
    QString pathQ = widget->property("pluginPath").toString();
    if (pathQ.isEmpty()) return;
    std::string path = pathQ.toStdString();
    int typeInt = widget->property("pluginType").toInt();
    PluginUIType type = static_cast<PluginUIType>(typeInt);

    // Reload = remove then re-add
    switch (type) {
        case PluginUIType::Engine:
            core->getEventManager().sendMessage(AppMessage(name, "remove_engine", path));
            core->getEventManager().sendMessage(AppMessage(name, "add_engines_names", std::vector<std::string>{path}));
            break;
        case PluginUIType::Tracker:
            core->getEventManager().sendMessage(AppMessage(name, "remove_tracker", path));
            core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", std::vector<std::string>{path}));
            break;
        case PluginUIType::Generic:
            core->getEventManager().sendMessage(AppMessage(name, "disable_gen_plugin", path));
            core->getEventManager().sendMessage(AppMessage(name, "enable_gen_plugin", path));
            break;
    }
}

void MainWindow::reloadAllPlugins() {
    // Collect all paths by type (order: engines first, then trackers, then gen plugins)
    std::vector<std::string> enginePaths, trackerPaths, genPaths;
    for (const auto& [path, entry] : pluginPageEntries) {
        switch (entry.type) {
            case PluginUIType::Engine:  enginePaths.push_back(path); break;
            case PluginUIType::Tracker: trackerPaths.push_back(path); break;
            case PluginUIType::Generic: genPaths.push_back(path); break;
        }
    }

    // Remove all
    for (const auto& path : enginePaths)
        core->getEventManager().sendMessage(AppMessage(name, "remove_engine", path));
    for (const auto& path : trackerPaths)
        core->getEventManager().sendMessage(AppMessage(name, "remove_tracker", path));
    for (const auto& path : genPaths)
        core->getEventManager().sendMessage(AppMessage(name, "disable_gen_plugin", path));

    // Re-add in order
    if (!enginePaths.empty())
        core->getEventManager().sendMessage(AppMessage(name, "add_engines_names", enginePaths));
    if (!trackerPaths.empty())
        core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", trackerPaths));
    for (const auto& path : genPaths)
        core->getEventManager().sendMessage(AppMessage(name, "enable_gen_plugin", path));
}

void MainWindow::uiRemovePluginEntry(std::string path) {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, path]() {
            this->uiRemovePluginEntry(path);
        }, Qt::BlockingQueuedConnection);
        return;
    }

    auto it = pluginPageEntries.find(path);
    if (it == pluginPageEntries.end()) {
        qDebug() << "[UI] uiRemovePluginEntry: path not found:" << QString::fromStdString(path);
        return;
    }

    const PluginPageEntry& entry = it->second;

    switch (entry.type) {
    case PluginUIType::Engine:
        removeTabsOwnedByLibraryPath(ui->leftPanel, path);
        if (lastRenderedEngineLibraryPath == path)
            lastRenderedEngineLibraryPath.clear();
        break;
    case PluginUIType::Tracker:
        removeTabsOwnedByLibraryPath(ui->rightPanel, path);
        break;
    case PluginUIType::Generic:
        removeTabsOwnedByLibraryPath(ui->leftPanel, path);
        removeTabsOwnedByLibraryPath(ui->rightPanel, path);
        break;
    default:
        break;
    }

    // Remove from engineCheckboxes list if engine type
    if (entry.type == PluginUIType::Engine && entry.checkBox) {
        engineCheckboxes.erase(
            std::remove(engineCheckboxes.begin(), engineCheckboxes.end(), entry.checkBox),
            engineCheckboxes.end()
        );
    }

    // Find the page in the QToolBox and remove it
    for (int i = 0; i < ui->pluginsToolBox->count(); ++i) {
        QWidget* w = ui->pluginsToolBox->widget(i);
        if (w) {
            QString propPath = w->property("pluginPath").toString();
            if (propPath.toStdString() == path) {
                ui->pluginsToolBox->removeItem(i);
                delete w;
                break;
            }
        }
    }

    pluginPageEntries.erase(it);
    qDebug() << "[UI] Plugin page removed:" << QString::fromStdString(path);
}

// ─── Tracker table ────────────────────────────────────────────────────────────

void MainWindow::initTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table) {
    if (!table) return;
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, table]() {
            this->initTrackerTable(table);
        }, Qt::QueuedConnection);
        return;
    }
    if (table->empty()) return;
    _trackerTableCache = table;

    QTableWidget* tbl = ui->tableTrackerWidget;
    tbl->setUpdatesEnabled(false);
    tbl->clear();
    tbl->setRowCount(static_cast<int>(table->size()));
    tbl->setColumnCount(2);
    tbl->setHorizontalHeaderLabels({tr("Parameter"), tr("Value")});

    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tbl->verticalHeader()->setVisible(false);
    tbl->setAlternatingRowColors(true);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);

    int row = 0;
    for (const auto& [key, ptr] : *table) {
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(key));
        tbl->setItem(row, 0, nameItem);

        void* rawPtr = ptr.get();
        quint64 addr = reinterpret_cast<quint64>(rawPtr);
        nameItem->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(addr));

        tbl->setItem(row, 1, new QTableWidgetItem(""));
        row++;
    }
    tbl->setUpdatesEnabled(true);

    if (!_updateTimer) {
        _updateTimer = new QTimer(this);
        connect(_updateTimer, &QTimer::timeout, this, &MainWindow::updateTrackerTable);
        _updateTimer->start(16);
        qDebug() << "[TIMER] Tracker table update timer started.";
    }
}

void MainWindow::updateTrackerTable() {
    QTableWidget* tbl = ui->tableTrackerWidget;
    if (!tbl || tbl->rowCount() == 0) return;

    tbl->blockSignals(true);
    bool wasUpdatesEnabled = tbl->updatesEnabled();
    tbl->setUpdatesEnabled(false);

    for (int row = 0; row < tbl->rowCount(); ++row) {
        QTableWidgetItem* nameItem = tbl->item(row, 0);
        if (!nameItem) continue;

        QVariant var = nameItem->data(Qt::UserRole);
        if (!var.isValid()) continue;

        qulonglong addr = var.value<qulonglong>();
        if (addr == 0) continue;

        void* rawPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
        const std::string paramName = nameItem->text().toStdString();
        QString valueStr;

        try {
            if (paramName == "timestamp") {
                double val = *reinterpret_cast<double*>(rawPtr);
                valueStr = (val > 0.0) ? QString::number(val, 'f', 6) : "-";
            } else if (paramName == "faceId") {
                int val = *reinterpret_cast<int*>(rawPtr);
                valueStr = (val >= 0) ? QString::number(val) : "-";
            } else if (paramName == "success") {
                bool val = *reinterpret_cast<bool*>(rawPtr);
                valueStr = val ? "true" : "false";
            } else {
                float val = *reinterpret_cast<float*>(rawPtr);
                valueStr = (std::isnan(val) || std::isinf(val)) ? "NaN" : QString::number(val, 'f', 5);
            }
        } catch (...) {
            valueStr = "Err";
        }

        QTableWidgetItem* valItem = tbl->item(row, 1);
        if (!valItem) {
            valItem = new QTableWidgetItem(valueStr);
            tbl->setItem(row, 1, valItem);
        } else {
            if (valItem->text() != valueStr)
                valItem->setText(valueStr);
        }
    }

    tbl->setUpdatesEnabled(wasUpdatesEnabled);
    tbl->blockSignals(false);

    // Extract landmark points for tracker renderer
    std::vector<QPointF> points;
    if (_trackerTableCache) {
        int i = 0;
        while (true) {
            auto itX = _trackerTableCache->find("lm_" + std::to_string(i) + "_x");
            auto itY = _trackerTableCache->find("lm_" + std::to_string(i) + "_y");
            if (itX != _trackerTableCache->end() && itY != _trackerTableCache->end()) {
                float x = *reinterpret_cast<float*>(itX->second.get());
                float y = *reinterpret_cast<float*>(itY->second.get());
                points.push_back(QPointF(x, y));
                i++;
            } else {
                break;
            }
        }
    }

    if (trackerRenderer) {
        trackerRenderer->setPoints(points);
    }
}

// ─── Misc ─────────────────────────────────────────────────────────────────────

void MainWindow::onNewFileClicked() {
    core->getEventManager().sendMessage(AppMessage("UI", "new", 0));
}

void MainWindow::onSaveFileClicked() {
    core->getEventManager().sendMessage(AppMessage("UI", "save", 2147483646));
}

void MainWindow::setControlsTable(std::unordered_map<std::string, std::string> table) {}

void MainWindow::initDynamicUi(PluginUiEngineTrees submission) {
    if (!submission.pages)
        return;
    QMetaObject::invokeMethod(this,
        [this, submission = std::move(submission)]() mutable {
            if (!submission.pages)
                return;
            if (!lastRenderedEngineLibraryPath.empty()
                && lastRenderedEngineLibraryPath != submission.libraryPath) {
                removeTabsOwnedByLibraryPath(ui->leftPanel, lastRenderedEngineLibraryPath);
            }
            lastRenderedEngineLibraryPath = submission.libraryPath;
            removeTabsOwnedByLibraryPath(ui->leftPanel, submission.libraryPath);
            const std::string pathTag = submission.libraryPath;
            for (size_t i = 0; i < submission.pages->size(); ++i) {
                auto root = std::make_shared<RUI::UiPage>((*submission.pages)[i]);
                UiRenderer::renderToTabWidget(root, ui->leftPanel, pathTag);
            }
        }, Qt::QueuedConnection);
}

void MainWindow::updateResourceLabels()
{
#ifdef _WIN32
    // CPU: доля использования всех логических CPU процессом (0–100% ≈ доля мощности машины).
    static LARGE_INTEGER s_cpu_freq{};
    static bool s_have_cpu_freq = false;
    if (!s_have_cpu_freq) {
        s_have_cpu_freq = QueryPerformanceFrequency(&s_cpu_freq) && s_cpu_freq.QuadPart != 0;
    }
    LARGE_INTEGER qpc_now{};
    QueryPerformanceCounter(&qpc_now);

    static FILETIME s_prev_k{}, s_prev_u{};
    static LARGE_INTEGER s_prev_qpc{};
    static bool s_have_cpu_snap = false;

    FILETIME ft_cre{}, ft_ext{}, ft_kern{}, ft_user{};
    if (GetProcessTimes(GetCurrentProcess(), &ft_cre, &ft_ext, &ft_kern, &ft_user) && s_have_cpu_freq) {
        auto ft_to_u64 = [](const FILETIME& ft) -> uint64_t {
            return (uint64_t(ft.dwHighDateTime) << 32) | uint64_t(ft.dwLowDateTime);
        };
        if (s_have_cpu_snap) {
            const uint64_t dk = ft_to_u64(ft_kern) - ft_to_u64(s_prev_k);
            const uint64_t du = ft_to_u64(ft_user) - ft_to_u64(s_prev_u);
            const uint64_t dproc = dk + du;
            const double elapsed =
                double(qpc_now.QuadPart - s_prev_qpc.QuadPart) / double(s_cpu_freq.QuadPart);
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            const int ncpu = (std::max)(1, static_cast<int>(si.dwNumberOfProcessors));
            const float pct = (elapsed > 1e-6)
                ? float((100.0 * (dproc / 1e7)) / (elapsed * static_cast<double>(ncpu)))
                : 0.f;
            ui->cpu_label->setText(tr("CPU: %1%").arg(QString::number(int(pct + 0.5f))));
        }
        s_prev_k = ft_kern;
        s_prev_u = ft_user;
        s_prev_qpc = qpc_now;
        s_have_cpu_snap = true;
    }

    PROCESS_MEMORY_COUNTERS_EX mem{};
    mem.cb = sizeof(mem);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem),
                             sizeof(mem))) {
        const double mb = static_cast<double>(mem.WorkingSetSize) / (1024.0 * 1024.0);
        ui->ram_label->setText(tr("RAM: %1 MB").arg(QString::number(mb, 'f', mb >= 100.0 ? 0 : 1)));
    }
#else
    ui->cpu_label->setText(tr("CPU: —"));
    ui->ram_label->setText(tr("RAM: —"));
#endif

    try {
        IDataBus* bus = core->getEventManager().getBusPtr();
        if (auto* ch = bus_handle_cast_derived<AtomicFloatLiveChannel>(bus, "engine_gpu_load")) {
            const float v = ch->load_relaxed();
            ui->gpu_label->setText(tr("GPU: %1%").arg(QString::number(int(v + 0.5f))));
            ui->gpu_label->setVisible(true);
        } else {
            ui->gpu_label->setVisible(false);
        }
    } catch (...) {
        ui->gpu_label->setVisible(false);
    }
}

void MainWindow::initTrackerDynamicUi(PluginUiTrackerTrees submission) {
    if (!submission.trees)
        return;
    QMetaObject::invokeMethod(this,
        [this, submission = std::move(submission)]() mutable {
            if (!submission.trees)
                return;
            removeTabsOwnedByLibraryPath(ui->rightPanel, submission.libraryPath);
            const std::string pathTag = submission.libraryPath;
            for (const auto& [tabTitle, page] : *submission.trees) {
                (void)tabTitle;
                UiRenderer::renderToTabWidget(std::make_shared<RUI::UiPage>(page),
                                              ui->rightPanel,
                                              pathTag);
            }
        }, Qt::QueuedConnection);
}

void MainWindow::connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr) {}

void MainWindow::showCacheErrorMessage() {
    std::cout << "Cache doesn't exist" << std::endl;
}

void MainWindow::setRenderApi() {
    QString fileName = QFileDialog::getOpenFileName(
        ui->centralwidget,
        tr("Choose render API library"),
        QDir::homePath(),
        tr("Dynamic libraries (*.dll);;All files (*)")
    );
    if (!fileName.isEmpty())
        core->getEventManager().sendMessage(AppMessage("UI", "set_render_api", fileName.toStdString()));
}
