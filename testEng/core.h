#ifndef CORE_H
#define CORE_H

#include "../appcore.h"
#include <string>
#include "puppetmodel.h"
#include "../abstractuinodes.h"
#include "../icacheable.h"
#include <nlohmann/json.hpp>
#include "../misc.h"

class Core : public ICacheable
{
public:
    Core(AppCore* appcptr);

private:

    AppCore* acptr;

    std::string name = "engcore";

    PuppetModel *puppet = new PuppetModel(); // todo: ВНИМАНИЕ, указатели лучше не писать по простому
    //std::unique_ptr<PuppetModel> puppet = std::make_unique<PuppetModel>();

    void buildModel(std::any data);

    void modelDataRequest();

    void startRendering();

    void buildGui();

    void sendQueueToUi();

    std::shared_ptr<renderQueue> rQueue = std::make_shared<renderQueue>();

        std::string cacheKey() const override { return name; }

    std::shared_ptr<UiPage> rootPage;

    nlohmann::json serializeCache() const override {
        nlohmann::json j = {

        };
        return j;
    }

    void deserializeCache(const std::any& data) override {
    }

    std::thread renderThread;
    std::atomic<bool> renderingActive;
    std::mutex queueMutex;

    void renderLoop();
};

#endif // CORE_H

// здесь можно сделать всю логику вообще, на время
