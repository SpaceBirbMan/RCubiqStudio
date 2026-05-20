#include "pluginfilebroker.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <cstdint>
#include <climits>
#include <mach-o/dyld.h>
#endif
#endif

namespace {

namespace fs = std::filesystem;

uint64_t fnv1a64_utf8(const std::string& str)
{
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : str) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

/// Короткий префикс для каталога (имя файла без пути); полный ключ кодируется в FNV — иначе путь к кешу
/// на Windows упирается в MAX_PATH если подставить весь `library_path_`.
std::string scope_key_short_tag(const std::string& scopeKey)
{
    const auto pos = scopeKey.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? scopeKey : scopeKey.substr(pos + 1);
    std::string out;
    out.reserve((std::min)(base.size(), std::size_t(28)));
    for (unsigned char c : base) {
        if (std::isalnum(c) != 0 || c == '_' || c == '.' || c == '-')
            out += static_cast<char>(c);
        else if (out.empty() || out.back() != '_')
            out += '_';
    }
    while (!out.empty() && out.front() == '_')
        out.erase(out.begin());
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    if (out.size() > 28)
        out.resize(28);
    if (out.empty())
        out.assign("scope");
    return out;
}

std::string effective_sub_path_for_scope(const std::unordered_map<std::string, std::string>& scopeRel,
                                         const std::string& scopeKey)
{
    std::string pluginSeg = PluginFileBrokerImpl::scopeToDirSegment(scopeKey);
    auto it = scopeRel.find(scopeKey);
    if (it == scopeRel.end() || it->second.empty())
        return pluginSeg;
    return it->second + "/" + pluginSeg;
}

fs::path executable_directory()
{
#ifdef _WIN32
    std::wstring path;
    DWORD cap = MAX_PATH;
    for (int i = 0; i < 8; ++i) {
        path.assign(cap, L'\0');
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), cap);
        if (n == 0)
            return {};
        if (n < cap) {
            path.resize(n);
            return fs::path(path).parent_path();
        }
        if (cap >= 65536)
            return {};
        cap *= 2;
    }
    return {};
#elif defined(__APPLE__)
    char buf[PATH_MAX]{};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return {};
    std::error_code ec;
    return fs::weakly_canonical(fs::path(buf), ec).parent_path();
#else
    std::error_code ec;
    fs::path p = fs::weakly_canonical("/proc/self/exe", ec);
    if (ec)
        return {};
    return p.parent_path();
#endif
}

fs::path default_plugins_cache_root()
{
    std::error_code ec;
    fs::path base = executable_directory();
    if (base.empty())
        base = fs::current_path(ec);
    if (ec || base.empty())
        base = fs::path(".");
    fs::path root = (base / "plugins_cache").lexically_normal();
    if (!root.is_absolute()) {
        fs::path cwd = fs::current_path(ec);
        if (!ec)
            root = (cwd / root).lexically_normal();
    }
    return root;
}

std::string trim_str(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
    return s;
}

} // namespace

void PluginFileBrokerImpl::setPluginsCacheRoot(const fs::path& absoluteRoot)
{
    std::error_code ec;
    fs::path norm = absoluteRoot.lexically_normal();
    if (norm.empty())
        return;
    if (!norm.is_absolute()) {
        fs::path cwd = fs::current_path(ec);
        if (ec)
            return;
        norm = (cwd / norm).lexically_normal();
    }
    fs::create_directories(norm, ec);
    if (ec) {
        std::cerr << "[PluginFileBroker] setPluginsCacheRoot: create_directories failed for "
                  << norm.generic_string() << ": " << ec.message() << '\n';
        return;
    }
    std::lock_guard<std::mutex> lk(mu_);
    root_override_ = norm;
    has_root_override_ = true;
}

std::string PluginFileBrokerImpl::sanitizedBaseName(const std::string& fileName)
{
    const fs::path p(fileName);
    std::string base = p.filename().string();
    if (base.empty() || base == "." || base == "..")
        return {};
    return base;
}

std::string PluginFileBrokerImpl::scopeToDirSegment(const std::string& scopeKey)
{
    if (scopeKey.empty())
        return "_unnamed";
    const uint64_t h = fnv1a64_utf8(scopeKey);
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(h));
    return scope_key_short_tag(scopeKey) + '_' + hex;
}

std::string PluginFileBrokerImpl::sanitizeRelativePath(const std::string& rel)
{
    if (rel.empty())
        return {};
    std::string q = trim_str(rel);
    std::replace(q.begin(), q.end(), '\\', '/');
    while (!q.empty() && q.front() == '/')
        q.erase(q.begin());

    std::string out;
    std::size_t pos = 0;
    while (pos < q.size()) {
        const auto slash = q.find('/', pos);
        const std::string part = q.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
        pos = (slash == std::string::npos) ? q.size() : slash + 1;
        if (part.empty())
            continue;
        if (part == "." || part == "..")
            return {};
        std::string seg;
        seg.reserve(part.size());
        for (unsigned char c : part) {
            if (std::isalnum(c) != 0 || c == '_' || c == '.' || c == '-')
                seg += static_cast<char>(c);
            else
                seg += '_';
        }
        if (seg.empty())
            continue;
        if (!out.empty())
            out += '/';
        out += seg;
    }
    return out;
}

void PluginFileBrokerImpl::setPluginStorageRelativePath(const std::string& scopeKey,
                                                       const std::string& relativePathUnderPluginsCache)
{
    if (scopeKey.empty())
        return;
    const std::string srel = sanitizeRelativePath(relativePathUnderPluginsCache);
    std::lock_guard<std::mutex> lk(mu_);
    if (srel.empty())
        scopeRel_.erase(scopeKey);
    else
        scopeRel_[scopeKey] = srel;
}

std::string PluginFileBrokerImpl::effectiveSubPath(const std::string& scopeKey)
{
    std::lock_guard<std::mutex> lk(mu_);
    return effective_sub_path_for_scope(scopeRel_, scopeKey);
}

fs::path PluginFileBrokerImpl::absoluteScopeDir(const std::string& scopeKey)
{
    std::lock_guard<std::mutex> lk(mu_);
    const std::string sub = effective_sub_path_for_scope(scopeRel_, scopeKey);
    const fs::path root = has_root_override_ ? root_override_ : default_plugins_cache_root();
    return root / fs::path(sub);
}

PluginFileReadResult PluginFileBrokerImpl::read(const std::string& scopeKey, const std::string& fileName)
{
    PluginFileReadResult r;
    const std::string base = sanitizedBaseName(fileName);
    if (base.empty() || scopeKey.empty())
        return r;

    const fs::path path = absoluteScopeDir(scopeKey) / fs::path(base);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return r;
    const auto end = f.tellg();
    if (end < 0)
        return r;
    const auto size = static_cast<std::size_t>(end);
    r.bytes.resize(size);
    if (size > 0) {
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(r.bytes.data()), static_cast<std::streamsize>(size));
        if (!f)
            return PluginFileReadResult{};
    }
    r.ok = true;
    return r;
}

bool PluginFileBrokerImpl::write(const std::string& scopeKey, const std::string& fileName,
                                 const uint8_t* data, size_t size)
{
    const std::string base = sanitizedBaseName(fileName);
    if (base.empty() || scopeKey.empty() || !data)
        return false;

    const fs::path dir = absoluteScopeDir(scopeKey);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        std::cerr << "[PluginFileBroker] create_directories failed for " << dir.generic_string()
                  << ": " << ec.message() << '\n';
        return false;
    }

    const fs::path path = dir / fs::path(base);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "[PluginFileBroker] open write failed: " << path.generic_string() << '\n';
        return false;
    }
    f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return f.good();
}
