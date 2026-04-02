#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "appcore.h"
#include <QTableWidget>
#include <QTimer>
#include <QFileDialog>
#include "devices.h"
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TrackerRenderer;

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

    void showCacheErrorMessage();
    void setControlsTable(std::unordered_map<std::string, std::string> table);
    void initDynamicUi(shared_ptr<std::vector<RUI::UiPage>> pages);
    void connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr);
    void addEngineFile();
    void switchActiveEngine(const QString& engine);
    void updateEnginesCombo(const std::set<std::string> &names);
    void setRenderApi();
    void initialize();
    void setVideoDevices(std::vector<CameraInfo> cameras);
    void startCamera(std::shared_ptr<Device> camera);
    void setActiveCamera(CameraInfo camera);
    void setActiveTracker(TrackerInfo info);
    void trackerChanged(const QString& tracker);
    void updateTrackersCombo(const std::set<std::string> &names);
    void initTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table);
    void updateTrackerTable();
    void uiAddPlugin(std::string name);
    void uiRemovePlugin();

    QTimer* _updateTimer;
    std::unordered_map<std::string, std::shared_ptr<void>> *_trackerTableCache;

    std::shared_ptr<renderQueue> frameQueue = nullptr;

    void renderNextFrame();

private slots:
    void onNewFileClicked();
    void onSaveFileClicked();
    void cameraChanged();
    void onFrameReceived(const QByteArray &jpegData);
    void addTrackers();
    void startTracker();
    void stopTracker();
    void addPlugin();
    void removePlugin();
};
#endif // MAINWINDOW_H
