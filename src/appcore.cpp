#include "appcore.h"
#include "eventmanager.h"

AppCore::AppCore() : eventManager() {
    eQueuePointer = &(this->eventManager.getQueue());

    eventManager.subscribe(name, "cache_ok", &AppCore::startInitialization, this);
    eventManager.subscribe(name, "askToPreInit", &AppCore::askToPreInit, this);
    eventManager.subscribe(name, "cache_err", &AppCore::discardStartUp, this);
    eventManager.subscribe(name, "module_subscribed", &AppCore::addToReady, this);
    eventManager.subscribe(name, "engine_ready", &AppCore::sendDataBusE, this);
    eventManager.subscribe(name, "plugin_manager_ready", &AppCore::sendDataBusP, this);
    eventManager.subscribe(name, "module_initialized", &AppCore::addToInitReady, this);

    this->eventManager.getBusPtr()->registerData("render_pipeline", std::vector<function<void()>> {}); // а может быть такие зарезервированные штуки стоит в enum поместить?
    this->eventManager.getBusPtr()->registerData("control_table", std::unordered_map<std::string, std::shared_ptr<void>> {});
    this->eventManager.getBusPtr()->registerData("engines_gui_pages", std::unordered_map<std::string, RUI::UiPage> {});
    this->eventManager.getBusPtr()->registerData("trakers_gui_pages", std::unordered_map<std::string, RUI::UiPage> {});
}

void AppCore::startInitialization() {
    // ... //
    eventManager.sendMessage(AppMessage(name, "initialize", 0));
}

void AppCore::discardStartUp() {
}

void AppCore::addToReady(std::string name) {
    if (!pre_init_flag) {
    this->readyModules.emplace_back(name);
    if(readyModules.size() >= modules.size()) {
        this->pre_init_flag = true;
        eventManager.sendMessage(AppMessage(this->name, "ask_cache", 0));
    }
    }
}

void AppCore::addToInitReady(std::string name) {
    if (!complition_flag) {
    this->initReadyModules.emplace_back(name);
    if(initReadyModules.size() >= modules.size()) {
        this->complition_flag = true;
        eventManager.sendMessage(AppMessage(this->name, "init_complete", 0));
    }
    }
}

void AppCore::sendDataBusE() {
    this->eventManager.sendMessage(AppMessage(name, "send_dbus_e", this->eventManager.getBusPtr()));
}

void AppCore::sendDataBusP() {
    this->eventManager.sendMessage(AppMessage(name, "send_dbus_p", this->eventManager.getBusPtr()));
}
