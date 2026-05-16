#include "eventmanager.h"
#include <iostream>
#include <thread>

// todo: Возможно стоит добавлять функции списком а не только по одному

void EventManager::waitUntilQuiet(std::chrono::milliseconds timeout) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + timeout;
    while (clock::now() < deadline) {
        if (messages.is_empty() && !processor.isDispatching()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if (messages.is_empty() && !processor.isDispatching())
                return;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
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
}

EventQueue& EventManager::getQueue() { return messages; }
