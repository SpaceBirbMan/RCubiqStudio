#include "hostinput.h"

#include <vector>

void HostInputControllers::install(ControlTableHost& host)
{
    entries_.clear();
    keySources_.clear();

    auto add = [&](const char* key, float* addr) {
        entries_[key] = std::shared_ptr<void>(reinterpret_cast<void*>(addr), [](void*) {});
        keySources_[key] = kSourceLabel;
    };
    add("MouseX", &mouseX);
    add("MouseY", &mouseY);
    add("Wheel", &wheel);

    std::vector<std::string> added;
    host.mergeHost(kOwnerPath, entries_, added);
}

void HostInputControllers::updateMouseNormalized(float nx, float ny)
{
    mouseX = nx;
    mouseY = ny;
}

void HostInputControllers::addWheelDelta(float delta)
{
    // Normalized scroll delta; bindings can map in_min/in_max as needed.
    wheel += delta / 120.0f;
    if (wheel > 1.0f) wheel = 1.0f;
    if (wheel < -1.0f) wheel = -1.0f;
}
