#include "virtualcamera.h"

VirtualCamera::VirtualCamera() {}

VirtualCamera::~VirtualCamera()
{
    shutdown();
}

bool VirtualCamera::init(const std::string& name,
                               int w,
                               int h)
{
    width = w;
    height = h;

    frameSize = width * height * 4;

    mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        (DWORD)frameSize,
        name.c_str()
        );

    if (!mapping)
        return false;

    buffer = (uint8_t*)MapViewOfFile(
        mapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        frameSize
        );

    return buffer != nullptr;
}

void VirtualCamera::pushFrame(const uint8_t* rgba)
{
    if (!buffer || !rgba)
        return;

    memcpy(buffer, rgba, frameSize);
}

void VirtualCamera::shutdown()
{
    if (buffer)
    {
        UnmapViewOfFile(buffer);
        buffer = nullptr;
    }

    if (mapping)
    {
        CloseHandle(mapping);
        mapping = nullptr;
    }
}
