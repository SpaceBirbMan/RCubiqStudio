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
    core->getEventManager().subscribe("update_engines_combo", &MainWindow::updateEnginesCombo, this);

    connect(ui->newFileMenuButton, &QAction::triggered, this, MainWindow::onNewFileClicked);
    connect(ui->saveFileMenuButton, &QAction::triggered, this, MainWindow::onSaveFileClicked);
    connect(ui->includeEngineMenuButton, &QAction::triggered, this, MainWindow::addEngineFile);
    connect(ui->enginesComboBox, &QComboBox::currentTextChanged,
            this, &MainWindow::switchActiveEngine);
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

void MainWindow::initDynamicUi(shared_ptr<std::vector<UiPage>> pages) {
    QMetaObject::invokeMethod(this, [this, pages]() { // это нужно из-за того, что рендер может вызываться не из ui-потока qt
        for (UiPage root : *pages) {
            UiRenderer::renderToTabWidget(std::make_shared<UiPage>(root), ui->leftPanel);
        }
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

void MainWindow::addEngineFile() {

    QWidget* parentWidget = ui->centralwidget;

    QStringList fileNames = QFileDialog::getOpenFileNames(
        parentWidget,
        "Выберите файл движка",
        QDir::homePath(),
        "Файлы движка (*.dll);;Все файлы (*)"
        );

    std::vector<std::string> names {};

    for (QString qstr : fileNames) {
        names.emplace_back(qstr.toStdString());
    }

    core->getEventManager().sendMessage(AppMessage("UI", "add_engines_names", names));
    // имена движков, по идее, надо кешировать и вписать в дроплист
}

void MainWindow::switchActiveEngine(const QString& engine) {

    LibMeta meta;
    meta.path = engine.toStdString();
    meta.func_names.emplace_back("create_engine");
    //meta.func_names.emplace_back("destroy");
    core->getEventManager().sendMessage(AppMessage("UI", "engine_resolving_request", meta));

}

void MainWindow::updateEnginesCombo(const std::set<std::string> &names) {
    for (std::string name : names) {
        ui->enginesComboBox->addItem(QString::fromStdString(name));
    }

}
