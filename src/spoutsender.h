#pragma once

class IDataBus;

/// Если в include path есть Spout SDK (`SpoutDX/SpoutDX.h`), сборка автоматически шлёт кадр с `frames_buffer`.
/// `hostHwnd` на Windows — это HWND, без Windows-заголовков в API: например `reinterpret_cast<void*>(winId())`.
/// После выгрузки движка обязательно вызывается [#spoutNotifyEngineDeviceReset()] (вызывает ViewportWidget).
void spoutAfterRenderTick(IDataBus* bus, void* hostHwnd, int viewportW, int viewportH);

/// Освободить Spout до destroy движка (иначе Sender держит старое устройство/текстуру → «Failed to send»).
void spoutNotifyEngineDeviceReset();
