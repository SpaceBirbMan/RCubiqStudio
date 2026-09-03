#include "eventmanager.h"
#include <iostream>
#include <thread>

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
    messages.sendMessage(message); //?
}

void EventManager::dispatchImmediately(const AppMessage& message)
{
    processor.enterDispatchContext();
    struct DepthExit {
        MessageProcessor& proc;
        explicit DepthExit(MessageProcessor& p) : proc(p) {}
        ~DepthExit() { proc.leaveDispatchContext(); }
    } exitGuard{processor};

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
