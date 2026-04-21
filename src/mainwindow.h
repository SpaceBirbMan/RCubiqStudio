#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "appcore.h"
#include <QTableWidget>
#include <QTimer>
#include <QFileDialog>
#include "devices.h"
#include <QLabel>
#include <QCheckBox>
#include <unordered_map>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TrackerRenderer;

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

private:
    Ui::MainWindow *ui;
    TrackerRenderer *trackerRenderer;
    AppCore* core;
    std::string name = "UI";

    QLabel *cameraLabel;

    std::shared_ptr<Device> currentCamera;

    // Plugin page tracking: path -> entry info
    std::unordered_map<std::string, PluginPageEntry> pluginPageEntries;
    // For engine checkbox exclusivity
    std::vector<QCheckBox*> engineCheckboxes;

    void showCacheErrorMessage();
    void setControlsTable(std::unordered_map<std::string, std::string> table);
    void initDynamicUi(std::shared_ptr<std::vector<RUI::UiPage>> pages);
    void initTrackerDynamicUi(std::unordered_map<std::string, RUI::UiPage>* pages);
    void connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr);
    void setRenderApi();
    void initialize();
    void initTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table);
    void updateTrackerTable();

    // Plugin UI (all types)
    void uiAddPluginEntry(PluginUIInfo info);
    void uiRemovePluginEntry(std::string path);
    void uiSetPluginActive(std::string path);
    void uiSetPluginInactive(std::string path);
    void uiSetPluginName(std::string path, std::string pluginName);  // for when name becomes known after loading

    // Adding dialogs helpers
    void addEnginePlugin();
    void addTrackerPlugin();
    void addGenPlugin();

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
};
#endif // MAINWINDOW_H