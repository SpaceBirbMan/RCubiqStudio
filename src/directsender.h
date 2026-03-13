#ifndef DIRECTSENDER_H
#define DIRECTSENDER_H

#include <any>
#include <functional>
class DirectSender
{

private:

    std::unordered_map<void*, void*> subs {};
    std::unordered_map<void*, std::function<void(const std::any&)>> subs_callbacks_pairs {};

public:

    DirectSender();

    void send(void* getter, std::any data); // отвал при вызове

    template <typename C, typename T>
    void subscribe(C* getter, void* sender, void (C::*method)(T)) {
        auto callback = [getter, method](const std::any& data) {
            if (data.type() == typeid(T)) {
                (getter->*method)(std::any_cast<T>(data));
            }
        };
        this->subs_callbacks_pairs.insert_or_assign(getter, callback);
        this->subs.insert_or_assign(getter, sender);
    }

    std::unordered_map<void*, void*> getSubs() {
        return subs;
    }

};

#endif // DIRECTSENDER_H
