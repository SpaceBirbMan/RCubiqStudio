#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QQuickView>
#include "viewportwidget.h"
#include "devices.h"
#include "uirenderer.h"


//QQuickView* view = new QQuickView(); // через эту тему лучше пойдёт рендер
// надо размещать отдельно, должно пойти с любым rend-back

using namespace RUI;

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
    core->getEventManager().subscribe("initialize", &MainWindow::initialize, this);
    core->getEventManager().subscribe(name, "get_video_devices_respond", &MainWindow::setVideoDevices, this);
    core->getEventManager().subscribe(name, "active_camera_info", &MainWindow::setActiveCamera, this);
    core->getEventManager().subscribe(name, "active_camera_device", &MainWindow::startCamera, this);
    //core->getEventManager().subscribe("send_frame_queue", &MainWindow::connectFramesToViewport, this);
    core->getEventManager().subscribe("update_engines_combo", &MainWindow::updateEnginesCombo, this);
    // TODO: мб стоит делать отдельный класс для подписки на сообщения
    connect(ui->newFileMenuButton, &QAction::triggered, this, &MainWindow::onNewFileClicked);
    connect(ui->saveFileMenuButton, &QAction::triggered, this, &MainWindow::onSaveFileClicked);
    connect(ui->includeEngineMenuButton, &QAction::triggered, this, &MainWindow::addEngineFile);
    connect(ui->enginesComboBox, &QComboBox::currentTextChanged,
            this, &MainWindow::switchActiveEngine);
    connect(ui->action_Render_API, &QAction::triggered, this, &MainWindow::setRenderApi);
    connect(ui->videoDeviceComboBox, &QComboBox::currentTextChanged, this, &MainWindow::cameraChanged);

    QVBoxLayout *layout = new QVBoxLayout(ui->frameForFace);
    layout->setContentsMargins(0, 0, 0, 0);
    cameraLabel = new QLabel(ui->frameForFace);
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    layout->addWidget(cameraLabel);
    cameraLabel->show();

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

void MainWindow::setVideoDevices(std::vector<CameraInfo> cameras) {
    for (CameraInfo camera : cameras) {
        std::cout << "[1-1]"<< camera.name << std::endl;
        ui->videoDeviceComboBox->addItem(QString::fromStdString(camera.name));
    }
}

void MainWindow::cameraChanged() {
    this->core->getEventManager().sendMessage(AppMessage(name, "activate_camera", ui->videoDeviceComboBox->currentText().toStdString()));
}

void MainWindow::setActiveCamera(CameraInfo camera) {
    ui->maxFpsLabel->setText(QString::fromStdString(std::to_string(camera.maxFps)));
}

// TODO: с девайса перейти на шину из менеджера
void MainWindow::startCamera(std::shared_ptr<Device> camera) {
    if (!camera) return;

    // 1. Останавливаем и сбрасываем старую камеру
    if (currentCamera) {
        currentCamera->close();
        // Сбрасываем коллбэк, чтобы старые данные не пришли позже
        currentCamera->setDataCallback(nullptr);
        currentCamera.reset();
    }

    currentCamera = camera;

    // 2. Устанавливаем новый коллбэк
    camera->setDataCallback([this](const std::vector<uint8_t>& data) {
        QByteArray ba(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
        // Используем QueuedConnection, чтобы декодирование не блокировало поток устройства
        QMetaObject::invokeMethod(this, "onFrameReceived", Qt::QueuedConnection, Q_ARG(QByteArray, ba));
    });

    camera->open();
    if (!camera->isOpen()) {
        std::cerr << "Failed to open camera" << std::endl;
        currentCamera.reset(); // Сброс при ошибке
    }
}

void MainWindow::onFrameReceived(const QByteArray &jpegData) {
    if (jpegData.isEmpty()) return;

    std::vector<uchar> buf(jpegData.begin(), jpegData.end());
    cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);

    if (frame.empty()) return;

    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

    QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
    QImage imageCopy = img.copy();

    QPixmap pixmap = QPixmap::fromImage(imageCopy);

    cameraLabel->setPixmap(pixmap);

    cameraLabel->setScaledContents(true);

    cameraLabel->setAlignment(Qt::AlignCenter);
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

}

void MainWindow::initDynamicUi(shared_ptr<std::vector<RUI::UiPage>> pages) {
    QMetaObject::invokeMethod(this, [this, pages]() { // это нужно из-за того, что рендер может вызываться не из ui-потока qt
        for (UiPage root : *pages) {
            UiRenderer::renderToTabWidget(std::make_shared<RUI::UiPage>(root), ui->leftPanel);
        }
    }, Qt::QueuedConnection);
}

void MainWindow::connectFramesToViewport(std::shared_ptr<renderQueue> queuePtr) {
}

void MainWindow::renderNextFrame() {

}

void MainWindow::addEngineFile() {

    QWidget* parentWidget = ui->centralwidget;

    QStringList fileNames = QFileDialog::getOpenFileNames(
        parentWidget,
        "Выберите файл движка",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
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

void MainWindow::setRenderApi() {
    QWidget* parentWidget = ui->centralwidget;

    QString fileName = QFileDialog::getOpenFileName(
        parentWidget,
        "Выберите файл API",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
        );

    std::cout << fileName.toStdString() << std::endl;
    core->getEventManager().sendMessage(AppMessage("UI", "set_render_api", fileName.toStdString()));
}
