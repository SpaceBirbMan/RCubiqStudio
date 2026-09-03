#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>
#include <string>
#include "rcqapi.h"
#include "appmessage.h"

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

    /// Process every message currently in the queue. Safe to call from inside a callback
    /// (the worker thread unlocks its mutex before invoking handlers).
    void drainPendingMessages();

    std::mutex& idleMutex();
    std::condition_variable& idleCv();

    void notifyIfIdle();

private:
    void dispatchOne(const AppMessage& msg);
    std::thread proc_thread;
    std::mutex mut;
    std::condition_variable cv;
    std::atomic<bool> stopFlag;
    std::atomic<int> dispatchDepth{0};
    EventQueue& qPtr;
    std::function<std::vector<subStruct>(const std::string&)> getSubscribersSnapshot;

    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;

    void process();

    void handleMessage() {
        // обработка сообщения
    }
};

#endif // MESSAGEPROCESSOR_H
