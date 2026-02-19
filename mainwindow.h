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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, AppCore* appCorePointer = nullptr);
    ~MainWindow();


private:
    Ui::MainWindow *ui;
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

    std::shared_ptr<renderQueue> frameQueue = nullptr;

    void renderNextFrame();

private slots:
    void onNewFileClicked();
    void onSaveFileClicked();
    void cameraChanged();
    void onFrameReceived(const QByteArray &jpegData);
};
#endif // MAINWINDOW_H
