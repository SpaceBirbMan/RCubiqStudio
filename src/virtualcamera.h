#ifndef VIRTUALCAMERA_H
#define VIRTUALCAMERA_H

#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

class VirtualCamera
{
public:
    VirtualCamera();
    ~VirtualCamera();

    bool init(const std::string& sharedName,
              int width,
              int height);

    void pushFrame(const uint8_t* rgba);

    void shutdown();

private:
    HANDLE mapping = nullptr;
    uint8_t* buffer = nullptr;

    int width = 0;
    int height = 0;
    size_t frameSize = 0;
};

#endif // VIRTUALCAMERA_H
