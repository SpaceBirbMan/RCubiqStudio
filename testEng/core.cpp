#include "core.h"


Core::Core(AppCore* appcptr) {
    this->acptr = appcptr;

    this->acptr->getEventManager().subscribe("build_model", &Core::modelDataRequest, this);
    this->acptr->getEventManager().subscribe("model_markdown_respond", &Core::buildModel, this);

    this->acptr->getEventManager().subscribe("build_gui", &Core::buildGui, this);

    this->puppet->addFileExt("tExt");

    this->acptr->getEventManager().subscribe("set_data", &Core::deserializeCache, this);
    this->acptr->getEventManager().subscribe("extract_render_queue", &Core::sendQueueToUi, this);

    this->acptr->getEventManager().sendMessage(AppMessage(name, "send_control_table", this->puppet->getControls()));

    // send_frame_queue
}

void Core::modelDataRequest() {
    this->acptr->getEventManager().sendMessage(AppMessage(name, "model_markdown_request", this->puppet->getFileExts()));
}

void Core::buildModel(std::any data) {
///
}

// штука из obs

void Core::buildGui() {

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

    UiButton button = UiButton();
    button.name = "fst";
    button.text = "idk";

    button.onClick = [this]() { this->startRendering(); };

    // todo: смысл контейнера вместо страницы - передавать страницы в контейнере, чтобы не делать это много раз по отдельности

    rootPage = std::make_shared<UiPage>();
    rootPage->children.emplace_back(std::make_unique<UiButton>(button));

    acptr->getEventManager().sendMessage(AppMessage(name, "init_ui_eng", rootPage));

}

void Core::startRendering() {
    this->acptr->getEventManager().sendMessage(AppMessage(name, "start_drawing_frames", 0));
}

void Core::sendQueueToUi() {


    this->acptr->getEventManager().sendMessage(AppMessage(name, "send_frame_queue", rQueue));

    // todo: Запрет на повторный запуск + отчёт об ошибке

    // запуск рендера тут же

    renderingActive = true;
    renderThread = std::thread(&Core::renderLoop, this);

}

void Core::renderLoop() {
    int frameCount = 0;
    while (renderingActive) {
        Frame frame;
        frame.width = 640;
        frame.height = 480;
        frame.stride = frame.width * 4; // RGBA
        frame.pixels.resize(frame.stride * frame.height, 0);

        uint8_t r = static_cast<uint8_t>((frameCount * 10) % 256);
        uint8_t g = static_cast<uint8_t>((frameCount * 7) % 256);
        uint8_t b = static_cast<uint8_t>((frameCount * 5) % 256);
        for (size_t i = 0; i < frame.pixels.size(); i += 4) {
            frame.pixels[i + 0] = r;     // R
            frame.pixels[i + 1] = g;     // G
            frame.pixels[i + 2] = b;     // B
            frame.pixels[i + 3] = 255;   // A
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            rQueue->push_back(std::move(frame));
            if (rQueue->size() > 3) {
                rQueue->pop_front();
            }
        }

        ++frameCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}
