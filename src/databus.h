#ifndef DATABUS_H
#define DATABUS_H

#include <any>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "misc.h"

class DataBus : public IDataBus
{
private:

    // нужна потокобезопасность

    std::unordered_map<std::string, std::any> storage_;

public:

    DataBus();

    void registerData(const std::string& key, std::any data) override {
        std::cout << "ADDED " << key << std::endl; // в модулях не отрабатывается добавление
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

};

#endif // DATABUS_H
