#include "spoutsender.h"

#ifndef _WIN32

void spoutAfterRenderTick(IDataBus*, void*, int, int) {}
void spoutNotifyEngineDeviceReset() {}

#else

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "databus.h"
#include <d3d11.h>
#include <iostream>

#if defined(__has_include)
#if __has_include("SpoutDX/SpoutDX.h")
#define SPOUT_IMPL_DX11 1
#include "SpoutDX/SpoutDX.h"
#endif
#endif

#ifdef SPOUT_IMPL_DX11

namespace {

constexpr const char kSenderName[] = "RCubiQWSender";

struct SpoutState {
    spoutDX sender{};
    HWND hwnd{};
    uintptr_t boundDevice = 0;
    bool ready = false;

    void shutdown()
    {
        if (!ready)
            return;
        sender.ReleaseSender();
        boundDevice = 0;
        ready = false;
    }

    ID3D11Device* acquireDevice(ID3D11Texture2D* tex) const
    {
        ID3D11Device* dev = nullptr;
        if (!tex)
            return nullptr;
        tex->GetDevice(&dev);
        return dev;
    }

    bool rebindTo(ID3D11Texture2D* tex, HWND hwndIn)
    {
        hwnd = hwndIn;
        if (!hwnd || !tex)
            return false;

        ID3D11Device* dev = acquireDevice(tex);
        if (!dev)
            return false;
        const uintptr_t devAddr = reinterpret_cast<uintptr_t>(static_cast<void*>(dev));
        dev->Release();

        shutdown();

        dev = acquireDevice(tex);
        if (!dev)
            return false;

        sender.OpenDirectX11(dev);
        dev->Release();

        sender.SelectSender(hwnd);
        sender.SetSenderName(kSenderName);

        boundDevice = devAddr;
        ready = true;
        return true;
    }

    bool ensureBound(ID3D11Texture2D* tex, HWND hwndIn)
    {
        hwnd = hwndIn;
        if (!hwnd || !tex)
            return false;

        ID3D11Device* dev = acquireDevice(tex);
        if (!dev)
            return false;
        const uintptr_t devAddr = reinterpret_cast<uintptr_t>(static_cast<void*>(dev));
        dev->Release();

        if (ready && boundDevice == devAddr)
            return true;

        return rebindTo(tex, hwndIn);
    }

    bool send(ID3D11Texture2D* tex, HWND hwndIn)
    {
        if (!ensureBound(tex, hwndIn))
            return false;

        if (sender.SendTexture(tex))
            return true;

        std::cerr << "SpoutSenderWrapper: Failed to send texture.\n";
        shutdown();
        if (!rebindTo(tex, hwndIn))
            return false;
        return sender.SendTexture(tex);
    }
};

SpoutState g_spout;

} // namespace

void spoutNotifyEngineDeviceReset()
{
    g_spout.shutdown();
}

void spoutAfterRenderTick(IDataBus* bus, void* hostHwnd, int viewportW, int viewportH)
{
    (void)viewportW;
    (void)viewportH;
    if (!bus || !hostHwnd)
        return;

    auto* hf = bus_handle_cast<std::deque<void*>>(bus, "frames_buffer");
    if (!hf || !hf->hasLive())
        return;

    auto& dq = **hf;
    if (dq.empty())
        return;

    void* p = dq.back();
    if (!p)
        return;

    auto* tex = reinterpret_cast<ID3D11Texture2D*>(p);
    g_spout.send(tex, reinterpret_cast<HWND>(hostHwnd));
}

#else // no SpoutDX/SpoutDX.h — проект без SDK

void spoutNotifyEngineDeviceReset() {}

void spoutAfterRenderTick(IDataBus*, void*, int, int) {}

#endif

#endif // _WIN32
