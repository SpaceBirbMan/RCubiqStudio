#pragma once

#include <string>

/// Замер куска кадра в процессе студии. Состояние одно на exe, плагины его не зовут:
/// хост оборачивает update и функции конвейера.
class CpuSpan {
public:
    explicit CpuSpan(std::string label);
    ~CpuSpan();
    CpuSpan(const CpuSpan&) = delete;
    CpuSpan& operator=(const CpuSpan&) = delete;

private:
    std::string label_;
    long long start_ = 0;
};

/// Текст за последнюю полную секунду: кто сколько занял и на каком участке.
std::string cpuProbeReport();
/// Короткая подпись самого тяжёлого участка, если он заметен на ядре.
std::string cpuProbeHotLabel();
