#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <memory>
#include <string>

class EventManager;

/// Диагностика падений через [cpptrace](https://github.com/jeremy-rifkin/cpptrace).
/// Каталог логов: подкаталог crash_logs рядом с исполняемым файлом (без Qt в путях).
/// Обработчики OS/terminate не трогают EventManager. Уведомление модулей — publishPendingIfAny().
class CrashHandler {
public:
    /// Сообщение для subscribe(receiver, ...): отправитель "Core", данные — std::string путь к .log.
    static constexpr const char* kCrashPendingMessage = "crash_report_pending";

    explicit CrashHandler(EventManager& em);
    ~CrashHandler();

    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    void install();
    /// Вызвать после конструкторов модулей, до или в начале exec(): прошлый краш → AppMessage.
    void publishPendingIfAny();

    void forceCreateDump(const std::string& dump_dir = {});
    void forceCloseApp();
    void forceRebootApp();
    void forceRestartModule();
    void forceBlockModule();

private:
    void startErrorListener();
    EventManager* em_ = nullptr;
    bool installed_ = false;
    std::string crash_logs_dir_;
};

#endif // CRASHHANDLER_H
