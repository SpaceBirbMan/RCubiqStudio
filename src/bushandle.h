#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>

/// Базовый type-erased дескриптор канала (для хранения в unordered_map без шаблонов).
class BusHandleBase {
public:
    virtual ~BusHandleBase() = default;
    /// Снять живой указатель (автор выгружен → чтение идёт через встроенный дефолт шины).
    virtual void clearLive() noexcept = 0;
    virtual void setLiveRaw(void* ptr) noexcept = 0;
};

/// Общий узел канала типа «указатель внутрь автора или дефолт на самой шине».
///
/// Объект дескриптора живёт в шине всё время сессии; адрес его default_val_
/// и указатель storagePtr() стабильны. При clearLive указатель автора становится nullptr,
/// а storagePtr()/оператор* переключаются на локальный безопасный буфер.
template<typename T>
class BusHandle : public BusHandleBase {
public:
    BusHandle() : live_ptr_(nullptr) {}
    explicit BusHandle(T defaultVal)
        : default_val_(std::move(defaultVal))
        , live_ptr_(nullptr)
    {}

    void setLive(T* ptr) noexcept { live_ptr_ = ptr; }
    void setLiveRaw(void* ptr) noexcept override { live_ptr_ = static_cast<T*>(ptr); }
    void clearLive() noexcept override { live_ptr_ = nullptr; }
    bool hasLive() const noexcept { return live_ptr_ != nullptr; }

    T&       operator*() noexcept { return live_ptr_ ? *live_ptr_ : default_val_; }
    const T& operator*() const noexcept { return live_ptr_ ? *live_ptr_ : default_val_; }
    T*       operator->() noexcept { return live_ptr_ ? live_ptr_ : &default_val_; }
    const T* operator->() const noexcept { return live_ptr_ ? live_ptr_ : &default_val_; }

    T*       storagePtr() noexcept { return live_ptr_ ? live_ptr_ : &default_val_; }
    const T* storagePtr() const noexcept { return live_ptr_ ? live_ptr_ : &default_val_; }

    T&       defaultVal() noexcept { return default_val_; }
    const T& defaultVal() const noexcept { return default_val_; }

private:
    T default_val_{};
    T* live_ptr_ = nullptr;
};

/// Специализированный канал для счётчика GPU: дефолт — атомар на шине, live — указатель автора в DLL.
class AtomicFloatLiveChannel final : public BusHandleBase {
public:
    AtomicFloatLiveChannel() = default;

    void clearLive() noexcept override { live_ = nullptr; }
    void setLiveRaw(void* ptr) noexcept override { live_ = static_cast<std::atomic<float>*>(ptr); }

    float load_relaxed() const noexcept
    {
        auto* l = live_;
        return l ? l->load(std::memory_order_relaxed) : default_.load(std::memory_order_relaxed);
    }

private:
    mutable std::atomic<float> default_{0.f};
    std::atomic<float>* live_ = nullptr;
};