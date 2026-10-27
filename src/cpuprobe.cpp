#include "cpuprobe.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

struct Acc {
    double ms = 0;
    int calls = 0;
};

std::mutex& probeMutex()
{
    static std::mutex mu;
    return mu;
}

std::unordered_map<std::string, Acc>& acc()
{
    static std::unordered_map<std::string, Acc> map;
    return map;
}

std::chrono::steady_clock::time_point& windowStart()
{
    static auto t = std::chrono::steady_clock::now();
    return t;
}

std::string& report()
{
    static std::string text;
    return text;
}

std::string& hot()
{
    static std::string text;
    return text;
}

long long qpc()
{
#ifdef _WIN32
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return c.QuadPart;
#else
    return std::chrono::steady_clock::now().time_since_epoch().count();
#endif
}

double qpcToMs(long long delta)
{
#ifdef _WIN32
    static const double inv = []() {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return (f.QuadPart > 0) ? (1000.0 / double(f.QuadPart)) : 0.0;
    }();
    return double(delta) * inv;
#else
    return double(delta) / 1e6;
#endif
}

void publishLocked(double windowMs)
{
    std::vector<std::pair<std::string, Acc>> rows;
    rows.reserve(acc().size());
    for (const auto& entry : acc()) {
        if (entry.second.calls > 0 && entry.second.ms >= 0.05)
            rows.push_back(entry);
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second.ms > b.second.ms;
    });

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    const double windowSec = windowMs > 1.0 ? windowMs / 1000.0 : 1.0;
    out << "За " << windowSec << " с, доля одного ядра\n";
    if (rows.empty())
        out << "Никто не выделяется";
    for (const auto& row : rows) {
        const double perCall = row.second.ms / double(row.second.calls);
        const double core = (row.second.ms / windowMs) * 100.0;
        out << row.first << " — " << perCall << " мс/вызов, "
            << core << "% ядра (" << row.second.calls << ")\n";
    }
    report() = out.str();
    hot().clear();
    if (!rows.empty()) {
        const double core = (rows.front().second.ms / windowMs) * 100.0;
        const double perCall = rows.front().second.ms / double(rows.front().second.calls);
        if (core >= 8.0 || perCall >= 2.0)
            hot() = rows.front().first;
    }
    acc().clear();
}

} // namespace

CpuSpan::CpuSpan(std::string label)
    : label_(std::move(label))
    , start_(qpc())
{
}

CpuSpan::~CpuSpan()
{
    const double ms = qpcToMs(qpc() - start_);
    std::lock_guard<std::mutex> lock(probeMutex());
    auto& slot = acc()[label_];
    slot.ms += ms;
    slot.calls += 1;
    const auto now = std::chrono::steady_clock::now();
    const double windowMs = std::chrono::duration<double, std::milli>(now - windowStart()).count();
    if (windowMs >= 1000.0) {
        publishLocked(windowMs);
        windowStart() = now;
    }
}

std::string cpuProbeReport()
{
    std::lock_guard<std::mutex> lock(probeMutex());
    if (report().empty())
        return "Замер копится около секунды. Наведите снова: кто занял ядро и на каком участке.";
    return report();
}

std::string cpuProbeHotLabel()
{
    std::lock_guard<std::mutex> lock(probeMutex());
    return hot();
}
