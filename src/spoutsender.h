// #ifndef SPOUTSENDER_H
// #define SPOUTSENDER_H

// #ifdef WIN32
// #if defined(USE_SPOUT_DX) || defined(USE_SPOUT_DX12) || defined(USE_SPOUT_DX9)
// #include <d3d11.h>
// #include "SpoutDX/SpoutDX.h"
// #elif defined(USE_SPOUT_GL)
// #include <GL/gl.h>
// #include "SpoutGL/Spout.h"
// #endif
// #include <string>
// #endif

// class SpoutSenderWrapper
// {
// public:
//     SpoutSenderWrapper();
//     ~SpoutSenderWrapper();

// #ifdef WIN32
// #if defined(USE_SPOUT_DX) || defined(USE_SPOUT_DX12) || defined(USE_SPOUT_DX9) || defined(USE_SPOUT_GL)
//     bool init(ID3D11Device* device, HWND hwnd,
//               int width,
//               int height,
//               const std::string& name);

//     void send(ID3D11Texture2D* texture);

//     void shutdown();

// private:

//     spoutDX sender = spoutDX();

//     HWND hwnd = nullptr;
//     ID3D11Device* device = nullptr;

//     int width = 0;
//     int height = 0;

//     bool initialized = false;
// #endif // defined(USE_SPOUT_...)
// #endif // WIN32
// };

// #endif // SPOUTSENDER_H
