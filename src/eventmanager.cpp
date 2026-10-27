#include "eventmanager.h"
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace {

void logSend(const ModuleLogProvider& log, const std::string& bus, const AppMessage& message)
{
    if (message.getMessage() == LoggerEvents::kLogWrite
        || message.getMessage() == "wht_cmd_refresh_monitor")
        return;
    static std::mutex gate;
    static std::unordered_map<std::string, std::chrono::steady_clock::time_point> seen;
    const auto now = std::chrono::steady_clock::now();
    const std::string key = bus + "|" + message.getSender() + "|" + message.getMessage();
    {
        std::lock_guard<std::mutex> lock(gate);
        const auto it = seen.find(key);
        if (it != seen.end() && now - it->second < std::chrono::seconds(1))
            return;
        seen[key] = now;
    }
    log.info(message.getSender() + " -> " + message.getMessage());
}

} // namespace

// todo: Возможно стоит добавлять функции списком а не только по одному

void EventManager::drainPendingMessages() {
    processor.drainPendingMessages();
}

void EventManager::waitUntilIdle() {
    std::unique_lock<std::mutex> lock(processor.idleMutex());
    processor.idleCv().wait(lock, [this]() {
        return messages.is_empty() && !processor.isDispatching();
    });
}

void EventManager::test() {
    // todo: потом
}

void EventManager::sendMessage(AppMessage message) {
    logSend(busLog_, busName_, message);
    messages.sendMessage(std::move(message));
}

void EventManager::dispatchImmediately(const AppMessage& message)
{
    processor.enterDispatchContext();
    struct DepthExit {
        MessageProcessor& proc;
        explicit DepthExit(MessageProcessor& p) : proc(p) {}
        ~DepthExit() { proc.leaveDispatchContext(); }
    }     exitGuard{processor};

    logSend(busLog_, busName_, message);

    std::vector<subStruct> subsCopy;
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        const auto it = subscribers_by_message_.find(message.getMessage());
        if (it != subscribers_by_message_.end())
            subsCopy = it->second;
    }
    for (const subStruct& sstr : subsCopy) {
        try {
            sstr.callback(message.getData());
        } catch (const std::exception& e) {
            std::cerr << "Exception in immediate callback '" << sstr.name << "' " << e.what() << std::endl;
        }
    }
    processor.notifyIfIdle();
}

EventQueue& EventManager::getQueue() { return messages; }
