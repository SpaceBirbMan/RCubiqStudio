#include "dynamiclibrary.h"
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

DynamicLibrary::DynamicLibrary(const std::string& path) {
#ifdef _WIN32
    handle_ = LoadLibraryA(path.c_str());
    if (!handle_) {
        throw std::runtime_error("Failed to load library: " + path);
    }
#else
    handle_ = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle_) {
        throw std::runtime_error("Failed to load library: " + std::string(dlerror()));
    }
#endif
}

DynamicLibrary::~DynamicLibrary() {
    if (handle_) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
    }
}

void* DynamicLibrary::getSymbol(const std::string& name) {
#ifdef _WIN32
    FARPROC proc = GetProcAddress(static_cast<HMODULE>(handle_), name.c_str());
    if (!proc) {
        throw std::runtime_error("Symbol not found: " + name);
    }
    // Безопасное преобразование FARPROC → void*
    void* sym = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(proc));
#else
    void* sym = dlsym(handle_, name.c_str());
    if (!sym) {
        throw std::runtime_error("Symbol not found: " + std::string(dlerror()));
    }
#endif
    return sym;
}
