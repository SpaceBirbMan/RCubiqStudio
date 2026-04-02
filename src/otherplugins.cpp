#include "otherplugins.h"
#include "appcore.h"

OtherPlugins::OtherPlugins(AppCore *appcore) {
    this->core = appcore;

    core->getEventManager().subscribe(name, "initialize", &OtherPlugins::initialize, this);
    core->getEventManager().subscribe(name, "pre_initialize", &OtherPlugins::preInitialize, this);
    core->getEventManager().subscribe(name, "plugin_resolving_respond", &OtherPlugins::registerPlugin, this);
    core->getEventManager().subscribe(name, "add_plugins_to_registry", &OtherPlugins::addPaths, this);
    core->getEventManager().subscribe(name, "send_dbus_p" , &OtherPlugins::setDataBus, this);
    core->getEventManager().subscribe(name, "init_complete" , &OtherPlugins::postInitialize, this);

}

void OtherPlugins::preInitialize() {

    auto deserialize_lambda = [this](const nlohmann::json& data) { this->deserializeCache(data); };
    auto serialize_lambda = [this]() -> json { return this->serializeCache(); };
    std::function<void(const nlohmann::json&)> deserialize_wrapper = deserialize_lambda;
    std::function<json()> serialize_wrapper = serialize_lambda;

    cacheForm cf_instance;
    cf_instance.name = name;
    cf_instance.desfn = deserialize_wrapper;
    cf_instance.sefn = serialize_wrapper;

    core->getEventManager().sendMessage(AppMessage(name, "sub_to_cache", cf_instance));
    core->getEventManager().sendMessage(AppMessage(name, "module_subscribed", name));
}

void OtherPlugins::initialize() {
    core->getEventManager().sendMessage(AppMessage(name, "init_started", 0));
    std::vector<std::string> tmp_names {};
    for (std::string name : pluginsRegistry) {
        tmp_names.emplace_back(name);
    }
    core->getEventManager().sendMessage(AppMessage(name, "plugin_manager_ready", 0));
    core->getEventManager().sendMessage(AppMessage(name, "module_initialized", name));
}

void OtherPlugins::postInitialize() {
    std::cout << name << " POST\n";
    std::vector<std::string> tmp_names {};
    for (std::string name : pluginsRegistry) {
        tmp_names.emplace_back(name);
    }
    core->getEventManager().sendMessage(AppMessage(name, "add_plugins_to_registry", tmp_names));
}

void OtherPlugins::addPaths(std::vector<std::string> paths) {
    for (std::string& path : paths) {
        this->pluginsRegistry.emplace(path);
        Meta meta;
        meta.path = path;
        meta.func_names = {"create", "destroy"};
        core->getEventManager().sendMessage(AppMessage(name, "plugin_resolving_request", meta));
    }
}

void OtherPlugins::setDataBus(IDataBus* dbp) {
    if (!dbp) {
        std::cerr << "Invalid handle on DataBus\n";
    }
    this->data_bus = dbp;
    if (this->need_to_set_db) {
        postSetDataBus();
    }
}

// FIXME: Возможна ситуация, при которой плагины не разрезолвились до конца, а данные по шине пришли. Вдруг, пока будет идти резолв, некоторые плагины останутся без шины, ибо попадут в промежуток где происходила настройка плагинов без шины
// мейби нужно сделать plugins потокобезопасным
void OtherPlugins::postSetDataBus() {
    std::cout << "Post set requested" << std::endl;
    if (plugins.empty()) {
        std::cerr << "plugins empty, but post setting was reuested. check resolving pipeline\n";
        return;
    }
    for (std::pair p : plugins) {
        p.second->setDataBus(this->data_bus);
    }
    this->need_to_set_db = false;
}

void OtherPlugins::registerPlugin(std::vector<void*> pointers) {
    if (pointers.empty()) {
        std::cerr << "No function pointers provided";
        return;
    }

    for (size_t i = 0; i < pointers.size(); ++i) {
        if (pointers[i] == nullptr) {
            std::cerr << "Pointer at index " << i << " is null";
            return;
        }
    }

    if (pointers.size() < 1 || pointers[0] == nullptr) {
        std::cerr << "Invalid engine parameters";
        return;
    }

    auto cp = reinterpret_cast<CreatePlugin>(pointers[0]);
    if (!cp) {
        std::cerr << "Invalid CreatePlugin pointer";
        return;
    }

    IGenPlugin* plugin = cp(&(core->getEventManager()), core->getEventManager().getBusPtr());
    if (!plugin) {
        std::cerr << "Plugin creation failed";
        return;
    }

    try {
        if (this->data_bus) {
            plugin->setDataBus(this->data_bus);
        } else {
            this->need_to_set_db = true;
        }
        this->plugins.emplace(plugin->getName(), plugin);
        core->getEventManager().sendMessage(AppMessage(name, "plugin_ready", 0));
        core->getEventManager().sendMessage(AppMessage(name, "plugin_started", plugin->getName())); // тут нужна запись со всяким, но потом

    } catch (const char* error_message) {
        std::cout << "this thing not gonna work u da\n";
        std::cout << error_message << std::endl;
    }
}

void OtherPlugins::deletePlugin(std::string name) {

}
