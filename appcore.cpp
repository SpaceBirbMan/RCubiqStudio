#include "appcore.h"
#include "messageprocessor.h"
#include "eventmanager.h"

AppCore::AppCore() : eventManager() {
    eQueuePointer = &(this->eventManager.getQueue());

    eventManager.subscribe(name, "cache_ok", &AppCore::startInitialization, this);
    eventManager.subscribe(name, "askToPreInit", &AppCore::askToPreInit, this);
    eventManager.subscribe(name, "cache_err", &AppCore::discardStartUp, this);
    eventManager.subscribe(name, "module_subscribed", &AppCore::addToReady, this);
}

void AppCore::startInitialization() {

    // ... //
    eventManager.sendMessage(AppMessage(name, "initialize", 0));
}

void AppCore::discardStartUp() {


}

void AppCore::addToReady(std::string name) {
    this->readyModules.emplace_back(name);
    if(readyModules.size() >= modules.size()) {
        eventManager.sendMessage(AppMessage(name, "ask_cache", 0));
    }
}
