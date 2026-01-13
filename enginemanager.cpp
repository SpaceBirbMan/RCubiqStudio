#include "enginemanager.h"

EngineManager::EngineManager(AppCore* acptr) {
    this->acptr = acptr;

    /*
     * Первый запрос на инициализацию требует ответ о готовности, затем
     * публикуется таблица данных из кеша, которую нужно "распарсить"
     * в конце инициализации таблица удаляется из оперативной памяти
     */

    // todo: А если данных в таблице инициализации будет очень много, насколько целесообразно копирование?

    // ответ на инициализацию
    acptr->getEventManager().subscribe("initialize", &EngineManager::initialize, this);
    // ответ на возврат ссылок плагина
    acptr->getEventManager().subscribe("engine_resolving_respond", &EngineManager::setActiveEngine, this);
    acptr->getEventManager().subscribe("general_init_ok", &EngineManager::setActiveEngine, this);

    acptr->getEventManager().subscribe("set_data", &EngineManager::deserializeCache, this);

    acptr->getEventManager().subscribe("start_drawing_frames", &EngineManager::getActiveFrames, this);
    acptr->getEventManager().subscribe("add_engines_names", &EngineManager::addNames, this);
}

void EngineManager::setFuncs(funcMap map) {
    // todd: верефикация таблицы (maybe)
    this->currentEngineFunctions = map;
}

void EngineManager::initialize() {
    acptr->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    auto deserialize_lambda = [this](const std::any& data) { this->deserializeCache(data); };
    // Changed: lambda now takes a const std::any& argument
    auto serialize_lambda = [this](const std::any& /* unused_data */) -> json { return this->serializeCache(); };

    // Explicitly constructing std::function objects might help if needed, though direct initialization usually works:
    std::function<void(const std::any&)> deserialize_wrapper = deserialize_lambda;
    // Changed: type now matches the expected signature with const std::any&
    std::function<json(const std::any&)> serialize_wrapper = serialize_lambda;

    cacheForm cf_instance;
    cf_instance.name = name;
    cf_instance.desfn = deserialize_wrapper;
    cf_instance.sefn = serialize_wrapper;
    acptr->getEventManager().sendMessage(AppMessage(name, "sub_to_cache", cf_instance));

    acptr->getEventManager().sendMessage(AppMessage(name, "build_gui", 0));
}

void EngineManager::setActiveEngine(std::string ename) {
    if (!enginesRegistry.empty()) {
        if (ename == "") {
            // резолв первой строки словаря
                auto pair = enginesRegistry.begin();
                //acptr->getEventManager().sendMessage(AppMessage(name, "engine_resolving_request", pair->second));

        } else {
            // резолв по имени
            acptr->getEventManager().sendMessage(AppMessage(name, "engine_resolving_request", enginesRegistry.find(ename))); // возможна ошибка - направильный доступ к элементу
        }
    } else {
        // уведомление об ошибке
        acptr->getEventManager().sendMessage(AppMessage(name, "error", "Данные об ошибке"));
    }

}

void EngineManager::getActiveFrames() {
    this->acptr->getEventManager().sendMessage(AppMessage(name, "extract_render_queue", 0));
}

void EngineManager::addNames(std::vector<std::string> names) {
    for (std::string name : names) {
        this->enginesRegistry.insert(name);
        std::cout << name << std::endl;
    }

    acptr->getEventManager().sendMessage(AppMessage(name, "added_names", names.size()));
}
