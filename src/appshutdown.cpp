#include "appshutdown.h"

#include "datamanager.h"
#include "enginemanager.h"
#include "consts.h"
#include "logger.h"

#include <atomic>

namespace {
std::atomic<bool> g_shutdownOnce{false};
}

bool isApplicationShuttingDown()
{
    return g_shutdownOnce.load(std::memory_order_acquire);
}

void beginApplicationShutdown(AppCore* core)
{
    if (!core)
        return;
    if (g_shutdownOnce.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    EventManager& em = core->getEventManager();
    auto log = em.getLogger().module("main");
    log.info("application shutdown initiated (aboutToQuit)");

    log.info("phase: stop_hid_listeners (libuiohook)");
    em.dispatchImmediately(AppMessage("DeviceManager", AppShutdownEvents::kStopHidListeners, 0));
    em.dispatchImmediately(AppMessage("UI", AppShutdownEvents::kReleaseUiInputCapture, 0));

    log.info("phase: stop_engine_tick");
    em.dispatchImmediately(AppMessage("main", AppShutdownEvents::kStopEngineTick, 0));

    log.info("phase: persist session cache (plugins still loaded)");
    core->persistPluginsAndWriteSessionCache({});

    log.info("phase: stop_tracker");
    em.dispatchImmediately(AppMessage("main", "stop_tracker", 0));

    log.info("phase: shutdown_all_engines");
    em.dispatchImmediately(AppMessage("EngineManager", AppShutdownEvents::kShutdownAllEngines, 0));

    log.info("phase: flush_engine_deletes");
    em.dispatchImmediately(AppMessage("ViewportWidget", "flush_engine_deletes", 0));

    log.info("application shutdown sequence complete");
}
