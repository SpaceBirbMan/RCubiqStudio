#include "mainwindow.h"
#include "datamanager.h"
#include "enginemanager.h"
#include "rendermanager.h"
#include "devicemanager.h"
#include "trackermanager.h"
#include "otherplugins.h"

#include "appsettings.h"
#include "consts.h"
#include "logger.h"
#include "appshutdown.h"
#include "devicehotplugnotifier.h"

#include <QApplication>

#ifdef QML
#include <QQmlEngine>
#include <QQuickView>
#endif

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    AppSettings::bootstrap(a);
    // QPixmap pixmap(":/splash.png");
    // QSplashScreen splash(pixmap);
    // splash.show();
    // a.processEvents();

    // // имитация загрузки модулей
    // for (int i = 0; i <= 100; i += 25) {
    //     splash.showMessage(QString("Загрузка... %1%").arg(i),
    //                        Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    //     QThread::msleep(300);
    //     a.processEvents();
    // }
    /////////////////////////////////////////////////
    AppCore *core = new AppCore;

    DataManager *dtm = new DataManager(core);

    EngineManager *egm = new EngineManager(core);  // нужно будет поправить порядок создания модулей
    MainWindow mainWindow(nullptr, core);

    RenderManager *renm = new RenderManager(core);
    DeviceManager *dvm = new DeviceManager(core);
    TrackerManager *tkm = new TrackerManager(core);
    OtherPlugins *op = new OtherPlugins(core); // <-- должен создаваться последним
    DeviceHotplugNotifier hotplugNotifier(core, &a);
    hotplugNotifier.installOn(&a);

    core->getCrashHandler().publishPendingIfAny();

    // core->registerModule(dtm->name);
    core->registerModule(egm->name);
    //core->registerModule(renm->cacheKey());
    core->registerModule(tkm->cacheKey());
    core->registerModule(op->cacheKey());

    core->getEventManager().sendMessage(AppMessage("main", "askToPreInit", 0)); // вместо нуля можно аргументы

#ifndef QML
    mainWindow.show();
#endif
#ifdef QML
    // QQuickView view; view.setSource(QUrl("qrc:/main.qml"));
    // view.setResizeMode(QQuickView::SizeRootObjectToView);
    // view.show();
#endif

    QObject::connect(&a, &QApplication::aboutToQuit, [&core]() { beginApplicationShutdown(core); });

    return a.exec();
}
