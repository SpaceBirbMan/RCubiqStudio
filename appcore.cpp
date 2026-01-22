#include "appcore.h"
#include "messageprocessor.h"
#include "eventmanager.h"

AppCore::AppCore() : eventManager() {
    eQueuePointer = &(this->eventManager.getQueue());

    eventManager.subscribe("cache_ok", AppCore::startInitialization, this);
    eventManager.subscribe("askToReady", AppCore::askToReady, this);
    eventManager.subscribe("cache_err", AppCore::discardStartUp, this);
    eventManager.subscribe("module_subscribed", AppCore::addToReady, this);

}

void AppCore::startInitialization() {

    // ... //
    eventManager.sendMessage(AppMessage(name, "initialize", 0));
}

void AppCore::discardStartUp() {


}

void AppCore::addToReady(std::string name) {
    this->readyModules.emplace_back(name);
}
