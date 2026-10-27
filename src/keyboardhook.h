#ifndef KEYBOARDHOOK_H
#define KEYBOARDHOOK_H

#include "rcqapi.h"
#include <vector>
// убрать
/// libuiohook (WH_KEYBOARD_LL) → keyboard_state bridge (Win32 VK only).
namespace KeyboardHook {

/// Apply one OsInputDevice packet.
/// Keyboard (7 B): type, keycode×2, mods×2, raw VK×2.
/// Mouse (2 B): type, button id (MOUSE_BUTTON1…5).
void applyPacket(KeyboardKeysState& kb, const std::vector<uint8_t>& packet);

} // namespace KeyboardHook

#endif // KEYBOARDHOOK_H
