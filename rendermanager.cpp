#include "rendermanager.h"

RenderManager::RenderManager(AppCore* acptr) {

    this->core = acptr;

    core->getEventManager().subscribe(name, "set_render_api", &RenderManager::setRenderApi, this);
    core->getEventManager().subscribe(name, "resolve_render_api_respond", &RenderManager::createRenderer, this);
    core->getEventManager().subscribe(name, "pre_initialize", &RenderManager::preInitialize, this);

}

void RenderManager::setRenderApi(std::string apiPath) {
    this->renderApi = apiPath;

    LibMeta meta;

    meta.path = apiPath;
    meta.func_names.emplace_back("create_renderer");
    //meta.func_names.emplace_back("destroy_renderer");

    core->getEventManager().sendMessage(AppMessage(name, "resolve_render_api_request", meta));
}

void RenderManager::createRenderer(std::vector<void*> pointers) {
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
        std::cerr << "Invalid renderer parameters";
        return;
    }

    auto cr = reinterpret_cast<CreateRenderer>(pointers[0]);
    if (!cr) {
        std::cerr << "Invalid create pointer";
        return;
    }

    renderer = cr();
    if (!renderer) {
        std::cerr << "Renderer creation failed";
        return;
    }

    renderer->test();
}
