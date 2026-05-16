#include "messageprocessor.h"
#include "eventqueue.h"
#include <iostream>

void MessageProcessor::process() {
    std::unique_lock<std::mutex> lock(mut);

    while (!stopFlag) {
        cv.wait(lock, [&]() {
            return stopFlag || !qPtr.is_empty();
        });

        if (stopFlag) break;

        while (!qPtr.is_empty()) {
            auto msg = qPtr.pollMessage();
            std::cout << "[SENDER] " + msg.getSender() + " [MESSAGE] " + msg.getMessage() << std::endl;
            lock.unlock();
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

                const std::vector<subStruct> subsCopy = getSubscribersSnapshot(msg.getMessage());
                for (const subStruct& sstr : subsCopy) {
                    try {
                        // TODO: Походу придётся всё приводить к типу bool callback(std::any) и сваливать приведение на коллбеки
                        sstr.callback(msg.getData());
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Exception in callback '"
                                  << sstr.name << "' " << e.what() << std::endl;
                    }
                    std::cout << "[RECEIVER] " << sstr.receiver << std::endl;
                }
            }

            lock.lock();
        }
    }
}


//TODO: Защита от зацикливания вызовов
