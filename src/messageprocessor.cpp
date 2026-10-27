#include "messageprocessor.h"
#include "cpumeter.h"
#include "eventqueue.h"
#include <chrono>
#include <iostream>

std::mutex& MessageProcessor::idleMutex() { return sync_mutex_; }
std::condition_variable& MessageProcessor::idleCv() { return sync_cv_; }

void MessageProcessor::dispatchOne(const AppMessage& msg)
{
    struct DispatchGuard {
        std::atomic<int>& depth;
        explicit DispatchGuard(std::atomic<int>& d) : depth(d) {
            depth.fetch_add(1, std::memory_order_acq_rel);
        }
        ~DispatchGuard() {
            depth.fetch_sub(1, std::memory_order_acq_rel);
        }
    } guard{dispatchDepth};

    if (!getSubscribersSnapshot)
        return;

    const std::vector<subStruct> subs = getSubscribersSnapshot(msg.getMessage());
    for (const subStruct& sstr : subs) {
        try {
            if (!cpuMeter_ || sstr.name == "log_write") {
                sstr.callback(msg.getData());
            } else {
                const auto started = std::chrono::steady_clock::now();
                sstr.callback(msg.getData());
                const double ms = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - started)
                                      .count();
                if (ms >= 0.4)
                    cpuMeter_->add(sstr.receiver == "N/A" ? sstr.name : sstr.receiver, sstr.name, ms);
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in callback '" << sstr.name << "' " << e.what() << std::endl;
        }
    }
}

void MessageProcessor::notifyIfIdle()
{
    if (!qPtr.is_empty() || dispatchDepth.load(std::memory_order_acquire) > 0)
        return;
    std::lock_guard<std::mutex> lock(sync_mutex_);
    sync_cv_.notify_all();
}

void MessageProcessor::drainPendingMessages()
{
    std::unique_lock<std::mutex> lock(mut);
    while (!stopFlag && !qPtr.is_empty()) {
        AppMessage msg = qPtr.pollMessage();
        //std::cout << "[SENDER] " + msg.getSender() + " [MESSAGE] " + msg.getMessage() << std::endl;
        lock.unlock();
        dispatchOne(msg);
        lock.lock();
    }
    notifyIfIdle();
}

void MessageProcessor::process() {
    std::unique_lock<std::mutex> lock(mut);

    while (!stopFlag) {
        cv.wait(lock, [&]() {
            return stopFlag || !qPtr.is_empty();
        });

        if (stopFlag) break;

        while (!qPtr.is_empty()) {
            AppMessage msg = qPtr.pollMessage();
            //std::cout << "[SENDER] " + msg.getSender() + " [MESSAGE] " + msg.getMessage() << std::endl;
            lock.unlock();
            dispatchOne(msg);
            lock.lock();
        }
        notifyIfIdle();
    }
}

//TODO: Защита от зацикливания вызовов
