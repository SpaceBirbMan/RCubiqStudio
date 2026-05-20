#pragma once

#include <QString>
#include <QVector>

#include <cstdint>

namespace GpuAdapters {

struct Entry {
    std::uintptr_t index{};
    QString name;
};

QVector<Entry> enumerateAdapters();

} // namespace GpuAdapters
