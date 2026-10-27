#ifndef DATABUS_H
#define DATABUS_H

#include <any>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "rcqapi.h"
#include "bushandle.h"

/// Шина данных. Потокобезопасность ключей — задача вызывающих; «живые» каналы см. registerBusHandle.
class DataBus : public IDataBus {
private:
    std::unordered_map<std::string, std::any> storage_;
    std::unordered_map<std::string, std::unique_ptr<BusHandleBase>> bus_handles_;
    IDataBus* upstream_ = nullptr;
    std::unordered_set<std::string> followData_;
    std::unordered_set<std::string> followHandles_;
    std::mutex followMu_;

public:
    DataBus();

    void follow(IDataBus* upstream, const std::string& key, bool asHandle) override {
        if (!upstream || upstream == this)
            return;
        std::lock_guard<std::mutex> lock(followMu_);
        upstream_ = upstream;
        if (asHandle)
            followHandles_.insert(key);
        else
            followData_.insert(key);
    }

    void registerData(const std::string& key, std::any data) override {
        IDataBus* upstream = nullptr;
        {
            std::lock_guard<std::mutex> lock(followMu_);
            if (upstream_ && followData_.count(key))
                upstream = upstream_;
        }
        if (upstream) {
            upstream->registerData(key, std::move(data));
            return;
        }
        std::cout << "ADDED " << key << std::endl;
        storage_[key] = std::move(data);
    }

    std::any& getData(const std::string& key) override {
        IDataBus* upstream = nullptr;
        {
            std::lock_guard<std::mutex> lock(followMu_);
            if (upstream_ && followData_.count(key))
                upstream = upstream_;
        }
        if (upstream)
            return upstream->getData(key);
        auto it = storage_.find(key);
        if (it == storage_.end()) {
            throw std::out_of_range("Key not found: " + key);
        }
        return it->second;
    }

    void remove(const std::string& key) override {
        storage_.erase(key);
    }

    void registerBusHandle(const std::string& key, std::unique_ptr<BusHandleBase> handle) override {
        bus_handles_[key] = std::move(handle);
    }

    BusHandleBase* tryBusHandle(const std::string& key) noexcept override {
        IDataBus* upstream = nullptr;
        {
            std::lock_guard<std::mutex> lock(followMu_);
            if (upstream_ && followHandles_.count(key))
                upstream = upstream_;
        }
        if (upstream)
            return upstream->tryBusHandle(key);
        auto it = bus_handles_.find(key);
        return it != bus_handles_.end() ? it->second.get() : nullptr;
    }

    void clearBusHandleLive(const std::string& key) noexcept override {
        auto it = bus_handles_.find(key);
        if (it != bus_handles_.end() && it->second)
            it->second->clearLive();
    }
};

template<typename T>
inline BusHandle<T>* bus_handle_cast(IDataBus* db, const std::string& key)
{
    if (!db)
        return nullptr;
    BusHandleBase* raw = db->tryBusHandle(key);
    return raw ? dynamic_cast<BusHandle<T>*>(raw) : nullptr;
}

template<typename Dest>
inline Dest* bus_handle_cast_derived(IDataBus* db, const std::string& key)
{
    if (!db)
        return nullptr;
    BusHandleBase* raw = db->tryBusHandle(key);
    return raw ? dynamic_cast<Dest*>(raw) : nullptr;
}

#endif // DATABUS_H
