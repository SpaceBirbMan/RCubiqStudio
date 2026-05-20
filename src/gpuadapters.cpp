#include "gpuadapters.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef INITGUID
#define INITGUID
#endif
#include <windows.h>
#include <dxgi.h>
#endif

#ifdef _MSC_VER
#pragma comment(lib, "dxgi.lib")
#endif

namespace GpuAdapters {

QVector<Entry> enumerateAdapters()
{
    QVector<Entry> out;
#ifdef _WIN32
    IDXGIFactory* factory = nullptr;
    const HRESULT hc = CreateDXGIFactory(IID_IDXGIFactory, reinterpret_cast<void**>(&factory));
    if (!SUCCEEDED(hc) || !factory)
        return out;

    for (UINT ai = 0;; ++ai) {
        IDXGIAdapter* adapter = nullptr;
        if (FAILED(factory->EnumAdapters(ai, &adapter)))
            break;
        if (!adapter)
            continue;
        DXGI_ADAPTER_DESC desc{};
        if (FAILED(adapter->GetDesc(&desc))) {
            adapter->Release();
            continue;
        }
        Entry e;
        e.index = static_cast<std::uintptr_t>(ai);
        e.name = QString::fromWCharArray(desc.Description);
        if (e.name.trimmed().isEmpty())
            e.name = QLatin1String("Adapter ") + QString::number(ai);
        out.push_back(std::move(e));
        adapter->Release();
    }
    factory->Release();
#endif
    return out;
}

} // namespace GpuAdapters
