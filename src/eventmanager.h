#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "eventqueue.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <any>
#include "misc.h"

using namespace std;

// template <typename... Args>
// using Callback = function<void(Args... args)>;

class EventManager // занимается рассылкой и обработкой сообщений, создаёт очередь сообщений, даёт на неё ссылку
        // сюда же можно свалить задачу по загрузке данных в окна, программа запустилась - постепенно закидываются данные в окна
{
public:
    EventManager(): messages{}, processor(messages, subscribers) {
        messages.setProcessor(&(this->processor));
    }

    // void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) {
    //     subscribers[msg] = std::move(fn);
    // }

    void subscribe(const std::string& msg, std::function<void(const std::any&)> fn) {
        subscribers.emplace_back(subStruct("N/A", msg, fn));
    }

    // template <typename C>
    // void subscribe(const std::string& msg, void (C::*method)(), C* instance) {
    //     subscribers[msg] = [=](const std::any&) {
    //         (instance->*method)();
    //     };
    // }

    template <typename C>
    void subscribe(const std::string& msg, void (C::*method)(), C* instance) {
        auto callback = [instance, method](const std::any&) {
            (instance->*method)();
        };
        subscribers.emplace_back("N/A", msg, std::move(callback));
    }

    template <typename C>
    void subscribe(const std::string& rec, const std::string& msg, void (C::*method)(), C* instance) {
        auto callback = [instance, method](const std::any&) {
            (instance->*method)();
        };
        subscribers.emplace_back(rec, msg, std::move(callback));
    }


    /**
     * @brief subscribe Подписывает модуль на сообщения
     * @param msg Сообщение, на которое отреагирует метод модуля
     * @param instance Метод и модуль, из которого вызывается метод (&класс::метод, *модуль)
     */

    template <typename T, typename C>
    void subscribe(const std::string& rec, const std::string& msg, void (C::*method)(T), C* instance) {
        auto callback = [instance, method](const std::any& data) {
            if (data.type() == typeid(T)) {
                (instance->*method)(std::any_cast<T>(data));
            }
        };
        subscribers.emplace_back(rec, msg, std::move(callback));
    }

    template <typename T, typename C>
    void subscribe(const std::string& msg, void (C::*method)(T), C* instance) {
        auto callback = [instance, method](const std::any& data) {
            if (data.type() == typeid(T)) {
                (instance->*method)(std::any_cast<T>(data));
            }
        };
        subscribers.emplace_back("N/A", msg, std::move(callback));
    }


    //void unsubscribe(const ); доделать, я хз по какому признаку их удалять

    EventQueue& getQueue(); // todo: Возможно требуется запретить запрос самой очереди

    void sendMessage(AppMessage message);

private:

    // регистрируем модуль в формате <Сообщение, на которое отреагирую><Коллбек>
    // Сообщения уже отправляются с данными
    // Нужна трассировка сохранённых сообщений

    //std::unordered_map<std::string, std::function<void(const std::any&)>> subscribers = std::unordered_map<std::string, std::function<void(const std::any&)>>();

    std::vector<subStruct> subscribers {};

    EventQueue messages = EventQueue{}; // хранит сообщения

    MessageProcessor processor = MessageProcessor(messages, subscribers);

    void notifyModules();

    // тестирует всю систему на предмет ошибок, тесты поверхностные, глубже в модуле, выдающем ошибку
    void test();

};

#endif // EVENTMANAGER_H
