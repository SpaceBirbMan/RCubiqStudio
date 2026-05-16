#ifndef IEVENTMANAGER_H
#define IEVENTMANAGER_H

#include <string>
#include <functional>
#include <any>
#include <typeinfo>
#include "appmessage.h"

// Forward declaration of IDataBus if needed via misc.h later.
class IDataBus;

class IEventManager {
public:
    virtual ~IEventManager() = default;

    virtual void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) = 0;
    virtual void subscribe(const std::string& rec, const std::string& msg, std::function<void(const std::any&)> fn) = 0;

    /// Remove all subscriptions registered with this receiver id (matches subStruct.receiver).
    /// Plugins must call this from shutdown() before the instance is destroyed.
    virtual void unsubscribeReceiver(const std::string& receiver) = 0;

    template <typename C>
    void subscribe(const std::string& msg, void (C::*method)(), C* instance) {
        auto callback = [instance, method](const std::any&) {
            (instance->*method)();
        };
        this->subscribe("N/A", msg, std::move(callback));
    }

    template <typename C>
    void subscribe(const std::string& rec, const std::string& msg, void (C::*method)(), C* instance) {
        auto callback = [instance, method](const std::any&) {
            (instance->*method)();
        };
        this->subscribe(rec, msg, std::move(callback));
    }

    template <typename T, typename C>
    void subscribe(const std::string& rec, const std::string& msg, void (C::*method)(T), C* instance) {
        auto callback = [instance, method](const std::any& data) {
            if (data.type() == typeid(T)) {
                (instance->*method)(std::any_cast<T>(data));
            }
        };
        this->subscribe(rec, msg, std::move(callback));
    }

    template <typename T, typename C>
    void subscribe(const std::string& msg, void (C::*method)(T), C* instance) {
        auto callback = [instance, method](const std::any& data) {
            if (data.type() == typeid(T)) {
                (instance->*method)(std::any_cast<T>(data));
            }
        };
        this->subscribe("N/A", msg, std::move(callback));
    }

    virtual void sendMessage(AppMessage message) = 0;

    /// Вызывает подписчиков по теме сообщения сразу, на стеке текущего потока (без постановки в очередь).
    virtual void dispatchImmediately(const AppMessage& message) = 0;
};

#endif // IEVENTMANAGER_H
