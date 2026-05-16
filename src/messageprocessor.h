#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>
#include <string>
#include "misc.h"

class EventQueue; // решение циклической зависимости

class MessageProcessor
{
public:
    explicit MessageProcessor(EventQueue& q, std::function<std::vector<subStruct>(const std::string&)> subscriberLookupByTopic)
        : qPtr(q)
        , stopFlag(false)
        , getSubscribersSnapshot(std::move(subscriberLookupByTopic))
    {
        proc_thread = std::thread(&MessageProcessor::process, this);
    }

    ~MessageProcessor() {
        {
            std::lock_guard<std::mutex> lock(mut);
            stopFlag = true;
        }
        cv.notify_one();
        if (proc_thread.joinable())
            proc_thread.join();
    }

    // вызывать при добавлении сообщения
    void notify() {
        cv.notify_one();
    }

    /// Учитывать синхронные вызовы коллбеков вне очереди (см. EventManager::dispatchImmediately).
    void enterDispatchContext() {
        dispatchDepth.fetch_add(1, std::memory_order_acq_rel);
    }

    void leaveDispatchContext() {
        dispatchDepth.fetch_sub(1, std::memory_order_acq_rel);
    }

    /// True while dispatching callbacks (including nested synchronous dispatches).
    bool isDispatching() const {
        return dispatchDepth.load(std::memory_order_acquire) > 0;
    }

private:
    std::thread proc_thread;
    std::mutex mut;
    std::condition_variable cv;
    std::atomic<bool> stopFlag;
    std::atomic<int> dispatchDepth{0};
    EventQueue& qPtr;
    std::function<std::vector<subStruct>(const std::string&)> getSubscribersSnapshot;

    void process();

    void handleMessage() {
        // обработка сообщения
    }
};

#endif // MESSAGEPROCESSOR_H
