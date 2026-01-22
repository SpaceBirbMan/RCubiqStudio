#ifndef APPCORE_H
#define APPCORE_H

#include "eventmanager.h"
#include <iostream>
#include "icacheable.h"
#include <vector>
#include <thread>

class AppCore
{
public:
    AppCore();

    EventManager& getEventManager() {
        return eventManager;
    }

    // вынести subscribe, send и прочее

    void registerModule(std::string module) {
        modules.emplace_back(module);
    }

private:

    std::string name = "Core";

    std::vector<std::string> modules {};
    std::vector<string> readyModules {};

    EventQueue *eQueuePointer = nullptr;

    EventManager eventManager;

    void dummyFunction() {
        std::cout<< eQueuePointer->logQueue() << std::endl;
    }

    void startInitialization();
    void discardStartUp();

    void addToReady(std::string name);

    void askToReady() {

        eventManager.sendMessage(AppMessage(name, "pre_initialize", 0));

        // std::thread tw(wait, this); ломает всё, напрвильно создан
        // tw.join();

        eventManager.sendMessage(AppMessage(name, "ask_cache", 0));
    }

    void wait() {
        while (readyModules.size() < modules.size()) { // ожидание ответа от модулей
        }
    }

};

#endif // APPCORE_H
