#include "keyboardhook.h"

#include <uiohook.h>

namespace KeyboardHook {

void applyPacket(KeyboardKeysState& kb, const std::vector<uint8_t>& packet)
{
    if (packet.empty())
        return;
    const uint8_t type = packet[0];

    if (type == EVENT_MOUSE_PRESSED || type == EVENT_MOUSE_RELEASED) {
        if (packet.size() < 2)
            return;
        const char* name = nullptr;
        switch (packet[1]) {
        case MOUSE_BUTTON1: name = "Left"; break;
        case MOUSE_BUTTON2: name = "Right"; break;
        case MOUSE_BUTTON3: name = "Middle"; break;
        case MOUSE_BUTTON4: name = "Back"; break;
        case MOUSE_BUTTON5: name = "Forward"; break;
        default: return;
        }
        const bool down = (type == EVENT_MOUSE_PRESSED);
        std::lock_guard<std::mutex> lk(kb.mutex);
        if (down)
            kb.mouseButtonsHeld.insert(name);
        else
            kb.mouseButtonsHeld.erase(name);
        return;
    }

    if (packet.size() < 7)
        return;
    if (type != EVENT_KEY_PRESSED && type != EVENT_KEY_RELEASED)
        return;

    const unsigned vk = static_cast<unsigned>(packet[5])
                      | (static_cast<unsigned>(packet[6]) << 8);
    if (vk == 0)
        return;

    const bool down = (type == EVENT_KEY_PRESSED);
    std::lock_guard<std::mutex> lk(kb.mutex);
#ifdef _WIN32
    if (down)
        kb.keysHeldVk.insert(static_cast<int>(vk));
    else
        kb.keysHeldVk.erase(static_cast<int>(vk));
#endif
}

} // namespace KeyboardHook
