#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/// Один счётчик на процесс. Хост передаёт указатель тем местам, где уже вызывает плагин:
/// update, шаг кадра, обработчик события. Плагин свой код к студии не привязывает.
class CpuMeter {
public:
    void add(const std::string& owner, const std::string& work, double milliseconds);

    struct Row {
        std::string owner;
        std::string work;
        double totalMs = 0;
        double avgMs = 0;
        uint32_t calls = 0;
    };

    /// Забирает окно замеров и сортирует по сумме времени.
    std::vector<Row> takeRows();

private:
    struct Acc {
        double ms = 0;
        uint32_t calls = 0;
    };
    struct Key {
        std::string owner;
        std::string work;
        bool operator==(const Key& other) const
        {
            return owner == other.owner && work == other.work;
        }
    };
    struct Hash {
        size_t operator()(const Key& key) const noexcept
        {
            return std::hash<std::string>{}(key.owner) ^ (std::hash<std::string>{}(key.work) * 1315423911u);
        }
    };

    std::mutex mu_;
    std::unordered_map<Key, Acc, Hash> window_;
};
