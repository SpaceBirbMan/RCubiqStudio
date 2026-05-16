#ifndef APPCORE_H
#define APPCORE_H

#include "eventmanager.h"
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include "crashhandler.h"

class AppCore
{
public:
    AppCore();

    EventManager& getEventManager() {
        return eventManager;
    }

    CrashHandler& getCrashHandler() { return *crashHandler; }

    /// Цепочка: синхронно разослать persist_modules подписчикам и записать session/cache (устанавливает DataManager).
    void setPersistPipeline(std::function<void(const std::string& dllPathHint)> fn);
    void persistPluginsAndWriteSessionCache(const std::string& dllPathHint);

    // вынести subscribe, send и прочее

    void registerModule(std::string module) {
        modules.emplace_back(module);
    }

private:

    std::string name = "Core";

    std::vector<std::string> modules {}; // модули, участвующие в учётах
    std::vector<string> readyModules {}; // готовые к получению кеша модули
    std::vector<string> initReadyModules {}; // готовые к инициализации модули

    bool complition_flag = false;
    bool pre_init_flag = false;

    EventQueue *eQueuePointer = nullptr;
    EventManager eventManager;
    std::unique_ptr<CrashHandler> crashHandler;
    std::function<void(const std::string&)> persistPipeline_;

    void dummyFunction() {
        std::cout<< eQueuePointer->logQueue() << std::endl;
    }

    void startInitialization();
    void discardStartUp(); // не найдено применение

    // функции отправки указателей на шину, подлежат пересмотру
    void sendDataBusE(); // в движки
    void sendDataBusP(); // в общие плагины

    // функции учёта
    void addToReady(std::string name);
    void addToInitReady(std::string name);

    // старт инициализации всего идёт с этой функции
    void askToPreInit() {
        eventManager.sendMessage(AppMessage(name, "pre_initialize", 0));
    }

};

#endif // APPCORE_H
