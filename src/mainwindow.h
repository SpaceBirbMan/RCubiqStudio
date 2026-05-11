#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include "appcore.h"
#include <QTableWidget>
#include <QTimer>
#include <QFileDialog>
#include <QTabWidget>
#include "devices.h"
#include <QLabel>
#include <QCheckBox>
#include <unordered_map>
#include <memory>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TrackerRenderer;
class RcqVirtualCamera;
class ObsVirtualCamera;

// Info stored per toolbox page
struct PluginPageEntry {
    std::string path;
    PluginUIType type;
    QCheckBox* checkBox = nullptr;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, AppCore* appCorePointer = nullptr);
    ~MainWindow();

protected:
    void changeEvent(QEvent* event) override;

private:
    Ui::MainWindow *ui;
    TrackerRenderer *trackerRenderer;
    AppCore* core;
    std::string name = "UI";

    QLabel *cameraLabel;

    std::shared_ptr<Device> currentCamera;

    /** Optional: stream viewport to akvirtualcamera (libvcam_capi.dll next to exe). */
    // std::unique_ptr<RcqVirtualCamera> m_rcqVirtualCam;

    /** Stream viewport to OBS Virtual Camera (DirectShow, shared memory NV12). */
    std::unique_ptr<ObsVirtualCamera> m_obsVirtualCam;

    // Plugin toolbox + динамические вкладки: путь DLL → вкладки с property m3_plugin_library_path
    std::unordered_map<std::string, PluginPageEntry> pluginPageEntries;
    /// Последний движок, отрисованный во вкладках слева (для смены активного без снятия панелей чужих путей).
    std::string lastRenderedEngineLibraryPath;
    // For engine checkbox exclusivity
    std::vector<QCheckBox*> engineCheckboxes;

    static void removeTabsOwnedByLibraryPath(QTabWidget* tabs, const std::string& libraryPath);

    /// Удалить вкладки, принадлежащие DLL (оба боковых таба); строка в toolbox не трогается.
    void uiTeardownPluginTabs(std::string path);

    void showCacheErrorMessage();
    void setControlsTable(std::unordered_map<std::string, std::string> table);
    void initDynamicUi(PluginUiEngineTrees submission);
    void initTrackerDynamicUi(PluginUiTrackerTrees submission);
    void connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr); // не актуально
    void setRenderApi();
    void initialize();
    void initTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table);
    void updateTrackerTable();

    // Plugin UI (all types)
    void uiAddPluginEntry(PluginUIInfo info);
    void uiRemovePluginEntry(std::string path);
    void uiSetPluginActive(std::string path);
    void uiSetPluginInactive(std::string path);
    void uiSetPluginName(std::string path, std::string pluginName);

    // Adding dialogs helpers
    void addEnginePlugin();
    void addTrackerPlugin();
    void addGenPlugin();
    void setupViewMenuDockToggles();

    QTimer* _updateTimer;
    std::unordered_map<std::string, std::shared_ptr<void>> *_trackerTableCache;

    std::shared_ptr<renderQueue> frameQueue = nullptr;

    void renderNextFrame();

private slots:
    void onNewFileClicked();
    void onSaveFileClicked();
    void addPlugin();
    void removePlugin();
    void reloadPlugin();
    void reloadAllPlugins();

    void updateResourceLabels();
};
#endif // MAINWINDOW_H
