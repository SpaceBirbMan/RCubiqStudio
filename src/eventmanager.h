#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "databus.h"
#include "directsender.h"
#include "eventqueue.h"
#include <vector>
#include <functional>
#include <any>
#include "ieventmanager.h"
#include "misc.h"

using namespace std;

// TODO: Неблокирующее логгирование и система доверия (кеширование проверок)

// template <typename... Args>
// using Callback = function<void(Args... args)>;

class EventManager : public IEventManager // занимается рассылкой и обработкой сообщений, создаёт очередь сообщений, даёт на неё ссылку
        // сюда же можно свалить задачу по загрузке данных в окна, программа запустилась - постепенно закидываются данные в окна
{
public:
    EventManager(): messages{}, processor(messages, subscribers) {
        messages.setProcessor(&(this->processor));
    }

    // void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) {
    //     subscribers[msg] = std::move(fn);
    // }

    void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) override {
        subscribers.emplace_back(subStruct("N/A", msg, fn));
    }

    void subscribe(const std::string& rec, const std::string& msg, std::function<void(const std::any&)> fn) override {
        subscribers.emplace_back(subStruct(rec, msg, fn));
    }

    using IEventManager::subscribe;

    //void unsubscribe(const ); доделать, я хз по какому признаку их удалять (да и зачем)

    EventQueue& getQueue(); // todo: Возможно требуется запретить запрос самой очереди
    DirectSender& getDirectSender() { return this->directSender; }

    IDataBus* getBusPtr() { return bus; }

    void sendMessage(AppMessage message) override;

private:

    //std::unordered_map<std::string, std::function<void(const std::any&)>> subscribers = std::unordered_map<std::string, std::function<void(const std::any&)>>();

    std::vector<subStruct> subscribers {};

    EventQueue messages = EventQueue{}; // хранит сообщения

    MessageProcessor processor = MessageProcessor(messages, subscribers);
    DirectSender directSender = DirectSender(); // TODO: Убрать
    DataBus* bus = new DataBus();

    void notifyModules();

    // тестирует всю систему на предмет ошибок, тесты поверхностные, глубже в модуле, выдающем ошибку
    void test();

};

#endif // EVENTMANAGER_H
