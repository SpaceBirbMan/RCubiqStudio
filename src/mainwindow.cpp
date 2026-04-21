#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QQuickView>
#include <QTableWidget>
#include <QHeaderView>
#include "viewportwidget.h"
#include "uirenderer.h"
#include "trackerrenderer.h"
#include <QDebug>
#include <QTimer>
#include <qobjectdefs.h>
#include <qthread.h>
#include <QMetaObject>
#include <QLabel>
#include <QVBoxLayout>
#include <QMenu>
#include <QCursor>
#include <QPalette>

using namespace RUI;

MainWindow::MainWindow(QWidget *parent, AppCore *core)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    this->core = core;
    ui->setupUi(this);

    _updateTimer = nullptr;
    _trackerTableCache = nullptr;
    trackerRenderer = nullptr;

    // Event subscriptions
    core->getEventManager().subscribe("cache_err", &MainWindow::showCacheErrorMessage, this);
    core->getEventManager().subscribe("send_control_table", &MainWindow::setControlsTable, this);
    core->getEventManager().subscribe("init_ui_eng", &MainWindow::initDynamicUi, this);
    core->getEventManager().subscribe<std::unordered_map<std::string, RUI::UiPage>*>("init_ui_tracker", &MainWindow::initTrackerDynamicUi, this);
    core->getEventManager().subscribe("initialize", &MainWindow::initialize, this);
    core->getEventManager().subscribe(name, "send_table", &MainWindow::initTrackerTable, this);

    // Plugin UI subscriptions (all three types)
    core->getEventManager().subscribe(name, "engine_ui_ready",      &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "tracker_ui_ready",     &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "gen_plugin_ui_ready",  &MainWindow::uiAddPluginEntry, this);
    core->getEventManager().subscribe(name, "engine_ui_removed",    &MainWindow::uiRemovePluginEntry, this);
    core->getEventManager().subscribe(name, "tracker_ui_removed",   &MainWindow::uiRemovePluginEntry, this);
    core->getEventManager().subscribe(name, "gen_plugin_ui_removed",&MainWindow::uiRemovePluginEntry, this);

    // Active state restore signals
    core->getEventManager().subscribe(name, "engine_set_active",       &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "tracker_set_active",      &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "tracker_set_inactive",    &MainWindow::uiSetPluginInactive, this);
    core->getEventManager().subscribe(name, "gen_plugin_activated",    &MainWindow::uiSetPluginActive, this);
    core->getEventManager().subscribe(name, "gen_plugin_deactivated",  &MainWindow::uiSetPluginInactive, this);

    // Button connections
    connect(ui->newFileMenuButton,    &QAction::triggered,  this, &MainWindow::onNewFileClicked);
    connect(ui->saveFileMenuButton,   &QAction::triggered,  this, &MainWindow::onSaveFileClicked);
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
    ui->gridLayout_2->replaceWidget(ui->viewport, vw);
    delete ui->viewport;
    ui->viewport = vw;
}

MainWindow::~MainWindow()
{
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
    QAction* actEngine    = menu.addAction("Добавить движок");
    QAction* actTracker   = menu.addAction("Добавить трекер");
    QAction* actGenPlugin = menu.addAction("Добавить плагин");

    QAction* chosen = menu.exec(QCursor::pos());

    if      (chosen == actEngine)    addEnginePlugin();
    else if (chosen == actTracker)   addTrackerPlugin();
    else if (chosen == actGenPlugin) addGenPlugin();
}

void MainWindow::addEnginePlugin() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        ui->centralwidget,
        "Выберите файл движка",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
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
        "Выберите файл трекера",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
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
        "Выберите файл плагина",
        QDir::homePath(),
        "Файлы плагинов (*.ofp);;Все файлы (*)"
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
    QCheckBox* checkBox = new QCheckBox("Активен");
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
                // Exclusive: uncheck all other engine checkboxes
                for (QCheckBox* cb : engineCheckboxes) {
                    if (cb != checkBox && cb->isChecked()) {
                        cb->blockSignals(true);
                        cb->setChecked(false);
                        cb->blockSignals(false);
                    }
                }
                core->getEventManager().sendMessage(AppMessage(name, "activate_engine_by_path", path));
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
        }, Qt::QueuedConnection);
        return;
    }

    auto it = pluginPageEntries.find(path);
    if (it == pluginPageEntries.end()) {
        qDebug() << "[UI] uiRemovePluginEntry: path not found:" << QString::fromStdString(path);
        return;
    }

    const PluginPageEntry& entry = it->second;

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
    tbl->setHorizontalHeaderLabels({"Parameter", "Value"});

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

void MainWindow::initDynamicUi(std::shared_ptr<std::vector<RUI::UiPage>> pages) {
    QMetaObject::invokeMethod(this, [this, pages]() {
        for (UiPage root : *pages) {
            UiRenderer::renderToTabWidget(std::make_shared<RUI::UiPage>(root), ui->leftPanel);
        }
    }, Qt::QueuedConnection);
}

void MainWindow::initTrackerDynamicUi(std::unordered_map<std::string, RUI::UiPage>* pages) {
    if (!pages) return;
    QMetaObject::invokeMethod(this, [this, pages]() {
        for (const auto& [pageName, page] : *pages) {
            UiRenderer::renderToTabWidget(std::make_shared<RUI::UiPage>(page), ui->rightPanel);
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
        "Выберите файл API",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
    );
    if (!fileName.isEmpty())
        core->getEventManager().sendMessage(AppMessage("UI", "set_render_api", fileName.toStdString()));
}
