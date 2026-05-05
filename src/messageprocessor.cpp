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
                    std::atomic<bool>& ref;
                    explicit DispatchGuard(std::atomic<bool>& r) : ref(r) {
                        ref.store(true, std::memory_order_release);
                    }
                    ~DispatchGuard() {
                        ref.store(false, std::memory_order_release);
                    }
                } guard{dispatching};

                for (const subStruct sstr : subsVector) {
                    if (msg.getMessage() == sstr.name) {
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
            }

            lock.lock();
        }
    }
}


//TODO: Защита от зацикливания вызовов
// TODO: Заменить на бинарный поиск, после инициализации выполнять сортировку по алфавиту
