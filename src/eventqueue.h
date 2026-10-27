#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <queue> // мб стоит юзать журнал, аки в apache kafka
#include <map>
#include <mutex>
#include <QString>
#include "appmessage.h"
#include <condition_variable>
#include "messageprocessor.h"

using namespace std;

class EventQueue
{
public:

    EventQueue() = default;

    std::string logQueue() {
        std::lock_guard<std::mutex> lock(mut);
        std::string str = "";
        for (AppMessage am : eventQueue) {
            str.append(am.toString()+'\n');
        }
        return str;
    } // выводит всю очередь, полезно для определния, что там есть

    void sendMessage(const AppMessage message) {
        {
            std::lock_guard<std::mutex> lock(mut);
            eventQueue.push_back(message);
        }
        mpPtr->notify();
    } // отправить сообщение в очередь

    AppMessage pollMessage(/*AppMessage message*/) {
        std::lock_guard<std::mutex> lock(mut);
        if (!eventQueue.empty()) {
            AppMessage res = eventQueue.front();
            eventQueue.pop_front();
            return res;
        }
        return AppMessage();
    } // достать сообщение из очереди

    void clearQueue() {
        std::lock_guard<std::mutex> lock(mut);
        eventQueue.clear();
    }

    void setProcessor(MessageProcessor* mpPtr) {
        this->mpPtr = mpPtr;
    }

    bool is_empty() {
        std::lock_guard<std::mutex> lock(mut);
        return eventQueue.empty();
    }


private:

    mutable std::mutex mut; // надо будет потом засунуть в методы
    MessageProcessor* mpPtr;

    deque<AppMessage> eventQueue = deque<AppMessage>{}; // сюда попадают сообщения из блоков
    //todo: Есть подозрения, что у сообщений будет приоретизация, для этого нужно будет юзать дерево, как минимум

};

#endif // EVENTQUEUE_H
