#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <queue> // мб стоит юзать журнал, аки в apache kafka
#include <map>
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
        std::string str = "";
        for (AppMessage am : eventQueue) {
            str.append(am.toString()+'\n');
        }
        return str;
    } // выводит всю очередь, полезно для определния, что там есть

    void sendMessage(const AppMessage message) {

        eventQueue.push_back(message);
        mpPtr->notify();

    } // отправить сообщение в очередь

    AppMessage pollMessage(/*AppMessage message*/) {
        if (!eventQueue.empty()) {

        AppMessage res = eventQueue.front(); // зачем
        eventQueue.pop_front();
        return res;
        } else {

        }
    } // достать сообщение из очереди

    void clearQueue() {
        eventQueue.clear();
    }

    void setProcessor(MessageProcessor* mpPtr) {
        this->mpPtr = mpPtr;
    }

    bool is_empty() {
        return eventQueue.empty();
    }


private:

    mutable std::mutex mut; // надо будет потом засунуть в методы
    MessageProcessor* mpPtr;

    deque<AppMessage> eventQueue = deque<AppMessage>{}; // сюда попадают сообщения из блоков
    //todo: Есть подозрения, что у сообщений будет приоретизация, для этого нужно будет юзать дерево, как минимум

};

#endif // EVENTQUEUE_H
