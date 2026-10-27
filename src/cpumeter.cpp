#include "cpumeter.h"

#include <algorithm>

void CpuMeter::add(const std::string& owner, const std::string& work, double milliseconds)
{
    if (!(milliseconds > 0.0))
        return;
    std::lock_guard<std::mutex> lock(mu_);
    auto& acc = window_[Key{owner, work}];
    acc.ms += milliseconds;
    acc.calls += 1;
}

std::vector<CpuMeter::Row> CpuMeter::takeRows()
{
    std::unordered_map<Key, Acc, Hash> snap;
    {
        std::lock_guard<std::mutex> lock(mu_);
        snap.swap(window_);
    }
    std::vector<Row> rows;
    rows.reserve(snap.size());
    for (auto& [key, acc] : snap) {
        if (acc.calls == 0)
            continue;
        Row row;
        row.owner = key.owner;
        row.work = key.work;
        row.totalMs = acc.ms;
        row.avgMs = acc.ms / double(acc.calls);
        row.calls = acc.calls;
        rows.push_back(std::move(row));
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.totalMs > b.totalMs;
    });
    return rows;
}
