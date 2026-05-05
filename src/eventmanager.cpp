#include "eventmanager.h"
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

EventQueue& EventManager::getQueue() { return messages; }
