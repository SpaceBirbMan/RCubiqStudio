#include "mainwindow.h"
#include "datamanager.h"
#include "enginemanager.h"
#include "testEng/core.h"
#include "rendermanager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSplashScreen>
#include <QThread>

#include <QApplication>
#include <QQuickView>
#include <QQmlEngine>


// todo: Метод, просящий модуль подписаться на определённое событие (можно ввести уровни доверия, чтобы нельзя было втыкать опасные методы)
// Вспомогательная функция для отправки сообщения о сохранении кэша
void sendSaveCacheMessage(AppCore* core) {
    if (core) {
        core->getEventManager().sendMessage(AppMessage("main", "save_cache", 0));
    }
}

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);


    QPixmap pixmap(":/splash.png");
    QSplashScreen splash(pixmap);
    splash.show();
    a.processEvents(); // показать немедленно

    // имитация загрузки модулей
    for (int i = 0; i <= 100; i += 25) {
        splash.showMessage(QString("Загрузка... %1%").arg(i),
                           Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        QThread::msleep(300);
        a.processEvents();
    }
/////////////////////////////////////////////////
    AppCore *core = new AppCore;

    DataManager *dtm = new DataManager(core);
    EngineManager *egm = new EngineManager(core);
    MainWindow mainWindow(nullptr, core);
    RenderManager *renm = new RenderManager(core);

    //core->registerModule(dtm->name);
    core->registerModule(egm->name);
    core->registerModule(renm->cacheKey());

    core->getEventManager().sendMessage(AppMessage("main", "askToPreInit", 0)); // вместо нуля можно аргументы
#ifndef QML
    mainWindow.show();
#endif
#ifdef QML
    // QQuickView view; view.setSource(QUrl("qrc:/main.qml"));
    // view.setResizeMode(QQuickView::SizeRootObjectToView);
    // view.show();
#endif
/////////////////////////////////////////////////
    splash.finish(&mainWindow); // закрыть сплеш после показа окна
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "M3_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QObject::connect(&a, &QApplication::aboutToQuit, [&core](){
        sendSaveCacheMessage(core);
    });

    return a.exec();
}



