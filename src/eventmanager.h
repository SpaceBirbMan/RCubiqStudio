#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "databus.h"
#include "directsender.h"
#include "eventqueue.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>
#include <chrono>
#include <mutex>
#include <algorithm>
#include "ieventmanager.h"
#include "misc.h"

using namespace std;

// TODO: Неблокирующее логгирование и система доверия (кеширование проверок)

// template <typename... Args>
// using Callback = function<void(Args... args)>;

class EventManager : public IEventManager // занимается рассылкой и обработкой сообщений, создаёт очередь сообщений, даёт на неё ссылку
        // сюда же можно свалить задачу по загрузке данных в окна, программа запустилась - постепенно закидываются данные в окна
{
public:
    EventManager()
        : messages{}
        , processor(messages, [this](const std::string& topic) {
              std::lock_guard<std::mutex> lock(subscribers_mutex_);
              const auto it = subscribers_by_message_.find(topic);
              return it == subscribers_by_message_.end() ? std::vector<subStruct>{} : it->second;
          })
    {
        messages.setProcessor(&(this->processor));
    }

    // void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) {
    //     subscribers[msg] = std::move(fn);
    // }

    void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) override {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_by_message_[msg].emplace_back(subStruct("N/A", msg, std::move(fn)));
    }

    void subscribe(const std::string& rec, const std::string& msg, std::function<void(const std::any&)> fn) override {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_by_message_[msg].emplace_back(subStruct(rec, msg, std::move(fn)));
    }

    using IEventManager::subscribe;

    void unsubscribeReceiver(const std::string& receiver) override
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (auto it = subscribers_by_message_.begin(); it != subscribers_by_message_.end(); ) {
            auto& bucket = it->second;
            bucket.erase(
                std::remove_if(bucket.begin(), bucket.end(),
                    [&](const subStruct& s) { return s.receiver == receiver; }),
                bucket.end());
            if (bucket.empty())
                it = subscribers_by_message_.erase(it);
            else
                ++it;
        }
    }

    EventQueue& getQueue(); // todo: Возможно требуется запретить запрос самой очереди
    DirectSender& getDirectSender() { return this->directSender; }

    IDataBus* getBusPtr() { return bus; }

    void sendMessage(AppMessage message) override;

    void dispatchImmediately(const AppMessage& message) override;

    /// Block until the message queue is empty and no callback is running (or timeout). For shutdown ordering.
    void waitUntilQuiet(std::chrono::milliseconds timeout);

private:

    mutable std::mutex subscribers_mutex_;
    std::unordered_map<std::string, std::vector<subStruct>> subscribers_by_message_;

    EventQueue messages = EventQueue{}; // хранит сообщения

    MessageProcessor processor;
    DirectSender directSender = DirectSender(); // TODO: Убрать
    DataBus* bus = new DataBus();

    void notifyModules();

    // тестирует всю систему на предмет ошибок, тесты поверхностные, глубже в модуле, выдающем ошибку
    void test(); // а точнее должен был по идее

};

#endif // EVENTMANAGER_H
