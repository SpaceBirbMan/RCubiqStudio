#ifndef BGFXQUIET_H
#define BGFXQUIET_H

#include <bgfx/bgfx.h>

#include <cstdarg>
#include <cstdio>

// Здесь этого быть не должно, приложение о bgfx не знает
/// Suppresses bgfx trace spam (shader const dumps, texture creation, etc.).
/// Fatal errors still go to stderr.
struct BgfxQuietCallback final : public bgfx::CallbackI {
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override
    {
        std::fprintf(stderr, "BGFX FATAL %s:%u: 0x%x %s\n", filePath, line, unsigned(code), str);
        if (code != bgfx::Fatal::DebugCheck)
            std::abort();
    }

    void traceVargs(const char*, uint16_t, const char*, va_list) override {}

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}

    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}

    void screenShot(const char*, uint32_t, uint32_t, uint32_t,
                    const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

inline bgfx::CallbackI* bgfxQuietCallback()
{
    static BgfxQuietCallback instance;
    return &instance;
}

#endif // BGFXQUIET_H
