#ifndef DATABUS_H
#define DATABUS_H

#include <any>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "misc.h"
#include "bushandle.h"

/// Шина данных. Потокобезопасность ключей — задача вызывающих; «живые» каналы см. registerBusHandle.
class DataBus : public IDataBus {
private:
    std::unordered_map<std::string, std::any> storage_;
    std::unordered_map<std::string, std::unique_ptr<BusHandleBase>> bus_handles_;

public:
    DataBus();

    void registerData(const std::string& key, std::any data) override {
        std::cout << "ADDED " << key << std::endl;
        storage_[key] = std::move(data);
    }

    std::any& getData(const std::string& key) override {
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
