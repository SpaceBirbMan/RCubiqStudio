#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include "appcore.h"
#include <QTableWidget>
#include <QTimer>
#include <QFileDialog>
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

    // Plugin page tracking: path -> entry info
    std::unordered_map<std::string, PluginPageEntry> pluginPageEntries;
    // For engine checkbox exclusivity
    std::vector<QCheckBox*> engineCheckboxes;

    void showCacheErrorMessage();
    void setControlsTable(std::unordered_map<std::string, std::string> table);
    void initDynamicUi(std::shared_ptr<std::vector<RUI::UiPage>> pages);
    void initTrackerDynamicUi(std::unordered_map<std::string, RUI::UiPage>* pages);
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
