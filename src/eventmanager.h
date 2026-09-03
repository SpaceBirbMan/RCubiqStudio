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
#include "consts.h"
#include "rcqapi.h"
#include "logger.h"

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
        subscribe(LoggerEvents::kReceiver, LoggerEvents::kLogWrite,
                  [this](const std::any& data) {
                      if (data.type() == typeid(LogRecord))
                          logger.submit(std::any_cast<LogRecord>(data));
                  });
    }

    Logger& getLogger() { return logger; }
    const Logger& getLogger() const { return logger; }

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
        size_t removed = 0;
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (auto it = subscribers_by_message_.begin(); it != subscribers_by_message_.end(); ) {
            auto& bucket = it->second;
            const size_t before = bucket.size();
            bucket.erase(
                std::remove_if(bucket.begin(), bucket.end(),
                    [&](const subStruct& s) { return s.receiver == receiver; }),
                bucket.end());
            removed += before - bucket.size();
            if (bucket.empty())
                it = subscribers_by_message_.erase(it);
            else
                ++it;
        }
        if (removed > 0) {
            logger.module("EventManager").info(
                "unsubscribeReceiver: '" + receiver + "' removed "
                + std::to_string(removed) + " callback(s)");
        }
    }

    /// Диагностика: receiver id подписчиков на топик (для отладки use-after-unload DLL).
    std::string subscriberReceiversForTopic(const std::string& msg) const
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        const auto it = subscribers_by_message_.find(msg);
        if (it == subscribers_by_message_.end())
            return "{}";
        std::string out;
        for (const auto& s : it->second) {
            if (!out.empty())
                out += ", ";
            out += s.receiver;
        }
        return "{" + out + "}";
    }

    EventQueue& getQueue(); // todo: Возможно требуется запретить запрос самой очереди
    DirectSender& getDirectSender() { return this->directSender; }

    IDataBus* getBusPtr() { return bus; }

    void sendMessage(AppMessage message) override;

    void dispatchImmediately(const AppMessage& message) override;

    /// Process every message already enqueued. Call from inside a handler on the worker thread
    /// when later steps must wait for side effects of sendMessage() issued earlier in the same flow.
    void drainPendingMessages();

    /// Block until the queue is empty and no callback is running. For shutdown from non-handler threads.
    void waitUntilIdle();

private:

    mutable std::mutex subscribers_mutex_;
    std::unordered_map<std::string, std::vector<subStruct>> subscribers_by_message_;

    EventQueue messages = EventQueue{}; // хранит сообщения

    MessageProcessor processor;
    DirectSender directSender = DirectSender(); // TODO: Убрать
    Logger logger;
    DataBus* bus = new DataBus();

    void notifyModules();

    // тестирует всю систему на предмет ошибок, тесты поверхностные, глубже в модуле, выдающем ошибку
    void test(); // а точнее должен был по идее

};

#endif // EVENTMANAGER_H
