#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "appcore.h"
#include <QTableWidget>
#include "uirenderer.h"
#include <QTimer>
#include <QFileDialog>

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

    void showCacheErrorMessage();
    void setControlsTable(std::unordered_map<std::string, std::string> table);
    void initDynamicUi(std::shared_ptr<UiPage> root);
    void connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr);
    void addEngineFile();

    std::shared_ptr<renderQueue> frameQueue = nullptr;
    QTimer* renderTimer = nullptr;

    QImage currentImage;

    void renderNextFrame();

private slots:
    void onNewFileClicked();
    void onSaveFileClicked();

};
#endif // MAINWINDOW_H
