#include "messageprocessor.h"
#include "eventqueue.h"
#include <iostream>

void MessageProcessor::process() {
    std::unique_lock<std::mutex> lock(mut);

    while (!stopFlag) {
        // ждём сигнал либо новые данные
        cv.wait(lock, [&]() {
            return stopFlag || !qPtr.is_empty();
        });

        if (stopFlag) break;

        // обрабатываем все сообщения, пока они есть
        while (!qPtr.is_empty()) {
            auto msg = qPtr.pollMessage();
            std::cout << "[SENDER] " + msg.getSender() + " [MESSAGE] " + msg.getMessage() << std::endl;
            lock.unlock();

            for (const subStruct sstr : subsVector) {
                if (msg.getMessage() == sstr.name) {
                    try {
                        sstr.callback(msg.getData());
                    }
                    catch (const std::bad_any_cast& e) {
                        std::cerr << "bad_any_cast in callback '"
                                  << sstr.name << "': "
                                  << e.what() << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "std::exception in callback '"
                                  << sstr.name << "': "
                                  << e.what() << std::endl;
                    }
                    catch (...) {
                        std::cerr << "unknown exception in callback '"
                                  << sstr.name << "'" << std::endl;
                    }
                    std::cout << "[RECEIVER] " << sstr.receiver << std::endl;
                }
            }

            lock.lock(); // todo: Заменить на бинарный поиск, после инициализации выполнять сортировку по алфавиту
        }
    }
}
