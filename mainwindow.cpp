#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QQuickView>
#include "ViewportWidget.h"

//QQuickView* view = new QQuickView(); // через эту тему лучше пойдёт рендер
// надо размещать отдельно, должно пойти с любым rend-back

MainWindow::MainWindow(QWidget *parent, AppCore *core) // есть подозрение, что интерфейс нужно поднимать через ленивую инициализацию, чтобы запуск был быстрый
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    this->core = core;
    ui->setupUi(this);

    auto* customViewport = new ViewportWidget(ui->viewport->parentWidget());
    ui->gridLayout_2->replaceWidget(ui->viewport, customViewport);
    delete ui->viewport;
    ui->viewport = customViewport;

    core->getEventManager().subscribe("cache_err", &MainWindow::showCacheErrorMessage, this);
    core->getEventManager().subscribe("send_control_table", &MainWindow::setControlsTable, this);
    core->getEventManager().subscribe("init_ui_eng", &MainWindow::initDynamicUi, this);
    core->getEventManager().subscribe("send_frame_queue", &MainWindow::connectFramesToViewport, this);

    connect(ui->newFileMenuButton, &QAction::triggered, this, MainWindow::onNewFileClicked);
    connect(ui->saveFileMenuButton, &QAction::triggered, this, MainWindow::onSaveFileClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showCacheErrorMessage() {
    std::cout << "Cache doesn't exists, idi peredelivay" << std::endl;
}

void MainWindow::onNewFileClicked() {
    this->core->getEventManager().sendMessage(AppMessage("UI", "new", 0));
}

void MainWindow::onSaveFileClicked() {
    this->core->getEventManager().sendMessage(AppMessage("UI", "save", 2147483646));
}

void MainWindow::setControlsTable(std::unordered_map<std::string, std::string> table) {
    // QTableWidget* tw = ui->tableWidget;

    // tw->clear(); // очищает данные и заголовки
    // tw->setColumnCount(2);
    // tw->setHorizontalHeaderLabels({"Контроллер модели", "Контроллер устройства"});
    // tw->setRowCount(static_cast<int>(table.size()));

    // int i = 0;
    // for (const auto& pair : table) {
    //     tw->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pair.first)));
    //     tw->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pair.second)));
    //     ++i;
    // }

    // tw->resizeColumnsToContents();
}

void MainWindow::initDynamicUi(std::shared_ptr<UiPage> root) {
    QMetaObject::invokeMethod(this, [this, root]() { // это нужно из-за того, что рендер может вызываться не из ui-потока qt
        UiRenderer::renderToTabWidget(root, ui->leftPanel);
    }, Qt::QueuedConnection);
}

void MainWindow::connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr) {
    QMetaObject::invokeMethod(this, [this, queuePtr]() {
        frameQueue = queuePtr;
        if (!renderTimer) {
            renderTimer = new QTimer(this);
            connect(renderTimer, &QTimer::timeout, this, &MainWindow::renderNextFrame);
            renderTimer->start(16); // ~60 FPS
        }
    }, Qt::QueuedConnection);
}

void MainWindow::renderNextFrame() {
    if (!frameQueue || frameQueue->empty()) return;

    Frame frame = std::move(frameQueue->front());
    frameQueue->pop_front();

    QImage img(frame.pixels.data(), frame.width, frame.height,
               frame.stride, QImage::Format_RGBA8888);

    QImage imgCopy = img.copy();

    QWidget* viewport = ui->viewport;
    static_cast<ViewportWidget*>(ui->viewport)->setImage(imgCopy);
    viewport->update();

    currentImage = imgCopy;
}
