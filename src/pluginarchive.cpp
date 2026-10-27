#include "pluginarchive.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace {

bool readFilePrefix(const std::filesystem::path& path, unsigned char* out, std::streamsize n)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.read(reinterpret_cast<char*>(out), n);
    return in.gcount() == n;
}

uint32_t readU32(const unsigned char* p)
{
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readU16(const unsigned char* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

bool entryNameSafe(const std::string& name)
{
    if (name.empty())
        return false;
    if (name[0] == '/' || name[0] == '\\')
        return false;
    if (name.size() >= 2 && std::isalpha(static_cast<unsigned char>(name[0])) && name[1] == ':')
        return false;
    std::string part;
    for (size_t i = 0; i <= name.size(); ++i) {
        const char c = (i == name.size()) ? '/' : name[i];
        if (c == '/' || c == '\\') {
            if (part == "..")
                return false;
            part.clear();
        } else {
            part.push_back(c);
        }
    }
    return true;
}

bool centralDirectorySafe(const std::filesystem::path& archive, std::string& error)
{
    std::ifstream in(archive, std::ios::binary);
    if (!in) {
        error = "не удалось открыть архив";
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 22) {
        error = "архив слишком короткий";
        return false;
    }
    const std::streamoff window = size < 65557 ? size : static_cast<std::streamoff>(65557);
    std::vector<unsigned char> tail(static_cast<size_t>(window));
    in.seekg(size - window);
    in.read(reinterpret_cast<char*>(tail.data()), window);
    if (in.gcount() != window) {
        error = "не удалось прочитать конец архива";
        return false;
    }

    int eocd = -1;
    for (int i = static_cast<int>(tail.size()) - 22; i >= 0; --i) {
        if (tail[static_cast<size_t>(i)] == 0x50 && tail[static_cast<size_t>(i) + 1] == 0x4b
            && tail[static_cast<size_t>(i) + 2] == 0x05 && tail[static_cast<size_t>(i) + 3] == 0x06) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        error = "в архиве нет каталога zip";
        return false;
    }

    const unsigned char* e = tail.data() + eocd;
    const uint16_t count = readU16(e + 10);
    const uint32_t cdSize = readU32(e + 12);
    const uint32_t cdOffset = readU32(e + 16);
    if (cdOffset == 0xffffffffu || cdSize == 0xffffffffu) {
        error = "zip64 не поддерживается";
        return false;
    }

    std::vector<unsigned char> cd(cdSize);
    in.clear();
    in.seekg(static_cast<std::streamoff>(cdOffset));
    in.read(reinterpret_cast<char*>(cd.data()), static_cast<std::streamsize>(cdSize));
    if (static_cast<uint32_t>(in.gcount()) != cdSize) {
        error = "не удалось прочитать каталог архива";
        return false;
    }

    size_t pos = 0;
    for (uint16_t n = 0; n < count; ++n) {
        if (pos + 46 > cd.size()) {
            error = "каталог архива повреждён";
            return false;
        }
        if (!(cd[pos] == 0x50 && cd[pos + 1] == 0x4b && cd[pos + 2] == 0x01 && cd[pos + 3] == 0x02)) {
            error = "каталог архива повреждён";
            return false;
        }
        const uint16_t nameLen = readU16(cd.data() + pos + 28);
        const uint16_t extraLen = readU16(cd.data() + pos + 30);
        const uint16_t commentLen = readU16(cd.data() + pos + 32);
        if (pos + 46 + nameLen > cd.size()) {
            error = "каталог архива повреждён";
            return false;
        }
        const std::string name(reinterpret_cast<const char*>(cd.data() + pos + 46), nameLen);
        if (!entryNameSafe(name)) {
            error = "запись архива выходит за каталог распаковки: " + name;
            return false;
        }
        pos += 46u + nameLen + extraLen + commentLen;
    }
    return true;
}

bool runTar(const std::filesystem::path& archive, const std::filesystem::path& dest, std::string& error)
{
#ifdef _WIN32
    wchar_t sys[MAX_PATH];
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        error = "не найден каталог system";
        return false;
    }
    std::wstring tar = std::wstring(sys) + L"\\tar.exe";
    std::wstring cmd = L"\"" + tar + L"\" -xf \"" + archive.wstring() + L"\" -C \"" + dest.wstring() + L"\"";
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(tar.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        error = "не удалось запустить tar";
        return false;
    }
    const DWORD wait = WaitForSingleObject(pi.hProcess, 120000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (wait != WAIT_OBJECT_0 || code != 0) {
        error = "tar не распаковал архив";
        return false;
    }
    return true;
#else
    const std::string archiveUtf = archive.string();
    const std::string destUtf = dest.string();
    const char* argv[] = {"tar", "-xf", archiveUtf.c_str(), "-C", destUtf.c_str(), nullptr};
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, "/usr/bin/tar", nullptr, nullptr, const_cast<char**>(argv), environ);
    if (spawned != 0) {
        error = "не удалось запустить tar";
        return false;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = "tar не распаковал архив";
        return false;
    }
    return true;
#endif
}

std::filesystem::path findEntry(const std::filesystem::path& root)
{
#ifdef _WIN32
    const char* primary = "index.dll";
#else
    const char* primary = "index.so";
#endif
    const std::filesystem::path direct = root / primary;
    if (std::filesystem::exists(direct))
        return direct;
    std::error_code ec;
    for (const auto& ent : std::filesystem::directory_iterator(root, ec)) {
        if (ec || !ent.is_directory())
            continue;
        const std::filesystem::path nested = ent.path() / primary;
        if (std::filesystem::exists(nested))
            return nested;
    }
    return {};
}

uint64_t pathHash(const std::string& text)
{
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace

bool isPluginArchive(const std::string& path)
{
    unsigned char magic[4] = {};
    if (!readFilePrefix(std::filesystem::path(path), magic, 4))
        return false;
    return magic[0] == 0x50 && magic[1] == 0x4b
        && ((magic[2] == 0x03 && magic[3] == 0x04)
            || (magic[2] == 0x05 && magic[3] == 0x06)
            || (magic[2] == 0x07 && magic[3] == 0x08));
}

PluginArchiveExtract extractPluginArchive(const std::filesystem::path& archive,
                                          const std::filesystem::path& cacheRoot)
{
    PluginArchiveExtract result;
    if (!centralDirectorySafe(archive, result.error))
        return result;

    const std::string stem = archive.stem().string();
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(pathHash(archive.string())));
    const std::filesystem::path dest = cacheRoot / "packages" / (stem + "-" + hex);

    std::error_code ec;
    std::filesystem::remove_all(dest, ec);
    std::filesystem::create_directories(dest, ec);
    if (ec) {
        result.error = "не удалось создать каталог распаковки";
        return result;
    }
    if (!runTar(archive, dest, result.error)) {
        std::filesystem::remove_all(dest, ec);
        return result;
    }
    result.entry = findEntry(dest);
    if (result.entry.empty()) {
        result.error = "в архиве нет index.dll/so";
        return result;
    }
    result.ok = true;
    result.error.clear();
    return result;
}
