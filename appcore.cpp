#include "appcore.h"
#include "messageprocessor.h"
#include "eventmanager.h"

AppCore::AppCore() : eventManager() {
    eQueuePointer = &(this->eventManager.getQueue());

    eventManager.subscribe("cache_ok", AppCore::startInitialization, this);
    eventManager.subscribe("askToReady", AppCore::askToReady, this);
    eventManager.subscribe("cache_err", AppCore::discardStartUp, this);

}

void AppCore::startInitialization() {

    // ... //

    eventManager.sendMessage(AppMessage(name, "initialize", 0));
}

void AppCore::discardStartUp() {


}
