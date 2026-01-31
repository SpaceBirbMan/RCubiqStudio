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

    void askToPreInit() {
        eventManager.sendMessage(AppMessage(name, "pre_initialize", 0));
    }

};

#endif // APPCORE_H
