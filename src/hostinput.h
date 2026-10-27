#ifndef HOSTINPUT_H
#define HOSTINPUT_H

#include "controltablehost.h"

#include <memory>
#include <string>
#include <unordered_map>
// Оно по моему не работает даже
/// Application-owned controllers (mouse X/Y, wheel) — always present in control_table.
class HostInputControllers {
public:
    static constexpr const char* kOwnerPath = "__host__";
    static constexpr const char* kSourceLabel = "Application";

    float mouseX = 0.5f;
    float mouseY = 0.5f;
    float wheel  = 0.0f;

    void install(ControlTableHost& host);
    void updateMouseNormalized(float nx, float ny);
    void addWheelDelta(float delta);

    const std::unordered_map<std::string, std::string>& keySources() const { return keySources_; }

private:
    std::unordered_map<std::string, std::shared_ptr<void>> entries_;
    std::unordered_map<std::string, std::string> keySources_;
};

#endif // HOSTINPUT_H
