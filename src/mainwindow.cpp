#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QQuickView>
#include <QTableWidget>
#include <QHeaderView>
#include "viewportwidget.h"
#include "devices.h"
#include "uirenderer.h"
#include "trackerrenderer.h"
#include <QDebug>
#include <QTimer>
#include <qobjectdefs.h>
#include <qthread.h>
#include <QMetaObject>

using namespace RUI;

MainWindow::MainWindow(QWidget *parent, AppCore *core)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    this->core = core;
    ui->setupUi(this);

    _updateTimer = nullptr;

    core->getEventManager().subscribe("cache_err", &MainWindow::showCacheErrorMessage, this);
    core->getEventManager().subscribe("send_control_table", &MainWindow::setControlsTable, this);
    core->getEventManager().subscribe("init_ui_eng", &MainWindow::initDynamicUi, this);
    core->getEventManager().subscribe("initialize", &MainWindow::initialize, this);
    core->getEventManager().subscribe(name, "get_video_devices_respond", &MainWindow::setVideoDevices, this);
    core->getEventManager().subscribe(name, "active_camera_info", &MainWindow::setActiveCamera, this);
    core->getEventManager().subscribe(name, "active_camera_device", &MainWindow::startCamera, this);
    core->getEventManager().subscribe(name, "send_table", &MainWindow::initTrackerTable, this);
    //core->getEventManager().subscribe("send_frame_queue", &MainWindow::connectFramesToViewport, this);
    core->getEventManager().subscribe("update_engines_combo", &MainWindow::updateEnginesCombo, this);
    core->getEventManager().subscribe("update_trackers_combo", &MainWindow::updateTrackersCombo, this);
    // TODO: мб стоит делать отдельный класс для подписки на сообщения
    connect(ui->newFileMenuButton, &QAction::triggered, this, &MainWindow::onNewFileClicked);
    connect(ui->saveFileMenuButton, &QAction::triggered, this, &MainWindow::onSaveFileClicked);
    connect(ui->includeEngineMenuButton, &QAction::triggered, this, &MainWindow::addEngineFile);
    connect(ui->enginesComboBox, &QComboBox::currentTextChanged,
            this, &MainWindow::switchActiveEngine);
    connect(ui->action_Render_API, &QAction::triggered, this, &MainWindow::setRenderApi);
    connect(ui->videoDeviceComboBox, &QComboBox::currentTextChanged, this, &MainWindow::cameraChanged);
    connect(ui->trackerComboBox, &QComboBox::currentTextChanged, this, &MainWindow::trackerChanged);
    connect(ui->addTrackerButton, &QPushButton::clicked, this, &MainWindow::addTrackers);
    connect(ui->startTracker, &QPushButton::clicked, this, &MainWindow::startTracker);
    connect(ui->stopTracker, &QPushButton::clicked, this, &MainWindow::stopTracker);

    ViewportWidget* vw = new ViewportWidget(core, this);

    ui->gridLayout_2->replaceWidget(ui->viewport, vw);
    delete ui->viewport;
    ui->viewport = vw;

    trackerRenderer = new TrackerRenderer(this);
    trackerRenderer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->verticalLayout_4->replaceWidget(ui->faceDotsViewport, trackerRenderer);
    delete ui->faceDotsViewport;
    ui->faceDotsViewport = trackerRenderer;

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
    // std::cout << "UXZ-45: " << camera.to_string();
}

// TODO: с девайса перейти на шину из менеджера
void MainWindow::startCamera(std::shared_ptr<Device> camera) {
    if (!camera) return;

    if (currentCamera) {
        currentCamera->close();
        currentCamera->setDataCallback(nullptr);
        currentCamera.reset();
    }

    currentCamera = camera;

    camera->setDataCallback([this](const std::vector<uint8_t>& data) {
        QByteArray ba(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
        QMetaObject::invokeMethod(this, "onFrameReceived", Qt::QueuedConnection, Q_ARG(QByteArray, ba));
    });

    camera->open();
    if (!camera->isOpen()) {
        std::cerr << "Failed to open camera" << std::endl;
        currentCamera.reset();
    }
}

void MainWindow::onFrameReceived(const QByteArray &jpegData) {
    if (jpegData.isEmpty()) return;

    std::vector<uchar> buf(jpegData.begin(), jpegData.end());
    cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR); // !!! CV лучше не юзать тут

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

void MainWindow::initTrackerTable(std::unordered_map<std::string, std::shared_ptr<void>>* table) {
    if (table != nullptr) {
    if (QThread::currentThread() != qApp->thread()) {
        qDebug() << "[THREAD] Wrong thread! Re-invoking via QueuedConnection with Lambda...";

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
    for (const auto& [name, ptr] : *table) {
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(name));
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
        qDebug() << "[TIMER] Timer created and started. Event loop should be running.";
    }
    }
}

void MainWindow::updateTrackerTable() {
    static int callCount = 0;
    callCount++;

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
        bool hasData = false;

        try {
            if (paramName == "timestamp") {
                double val = *reinterpret_cast<double*>(rawPtr);

                if (val > 0.0) {
                    valueStr = QString::number(val, 'f', 6);
                    hasData = true;
                } else {
                    valueStr = "-";
                }
            }
            else if (paramName == "faceId") {
                int val = *reinterpret_cast<int*>(rawPtr);

                if (val >= 0) {
                    valueStr = QString::number(val);
                    hasData = true;
                } else {
                    valueStr = "-";
                }
            }
            else if (paramName == "success") {
                bool val = *reinterpret_cast<bool*>(rawPtr);
                valueStr = val ? "true" : "false";
                hasData = true;
            }
            else {
                float val = *reinterpret_cast<float*>(rawPtr);


                if (std::isnan(val) || std::isinf(val)) {
                    valueStr = "NaN";
                    hasData = true;
                }

                else {
                    valueStr = QString::number(val, 'f', 5);
                    hasData = true;
                }
            }
        } catch (...) {
            valueStr = "Err";
        }

        QTableWidgetItem* valItem = tbl->item(row, 1);
        if (!valItem) {
            valItem = new QTableWidgetItem(valueStr);
            tbl->setItem(row, 1, valItem);
        } else {

            if (valItem->text() != valueStr) {
                valItem->setText(valueStr);

            }
        }
    }

    tbl->setUpdatesEnabled(wasUpdatesEnabled);
    tbl->blockSignals(false);

    // Extract points
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

void MainWindow::addTrackers() {
    QWidget* parentWidget = ui->centralwidget;

    QStringList fileNames = QFileDialog::getOpenFileNames(
        parentWidget,
        "Выберите файл трекера",
        QDir::homePath(),
        "Динамические библиотеки (*.dll);;Все файлы (*)"
        );

    std::vector<std::string> names {};

    for (QString qstr : fileNames) {
        names.emplace_back(qstr.toStdString());
    }

    for (std::string name : names) {
        ui->trackerComboBox->addItem(QString::fromStdString(name));
    }

    core->getEventManager().sendMessage(AppMessage(name, "add_trackers_names", names)); // FIXME: Не вызывается сообщение, почему? Добавить через местную систему логгирование ошибок
}

// TODO: Заменить логгер, объединить некоторые сообщения и методы (dll разрешаются одинаково), можно заменить расширения с dll на конкретные + потом их иконки менять

void MainWindow::trackerChanged(const QString& tracker) {

    Meta meta;

    meta.path = tracker.toStdString();
    meta.func_names.emplace_back("create");

    core->getEventManager().sendMessage(AppMessage("UI", "tracking_resolving_request", meta));
}

void MainWindow::setActiveTracker(TrackerInfo info) {

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
    std::cout << "Eng" << '\n';
    for (std::string name : names) {
        std::cout << name << '\n';
        ui->enginesComboBox->addItem(QString::fromStdString(name));
    }
}

void MainWindow::updateTrackersCombo(const std::set<std::string> &names) {
    std::cout << "Tra" << '\n';
    for (std::string name : names) {
        std::cout << name << '\n';
        ui->trackerComboBox->addItem(QString::fromStdString(name));
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

void MainWindow::startTracker() {
    core->getEventManager().sendMessage(AppMessage(name, "start_tracker", 0));
}

void MainWindow::stopTracker() {
    core->getEventManager().sendMessage(AppMessage(name, "stop_tracker", 0));
}
