#ifndef LOGGER_H
#define LOGGER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
// Лолгер
enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

struct LogRecord {
    std::string module;
    LogLevel level = LogLevel::Info;
    std::string message;
    std::chrono::system_clock::time_point time =
        std::chrono::system_clock::now();
};

class Logger;

/// Per-module facade: register once, then log without repeating the module name.
class ModuleLogProvider {
public:
    ModuleLogProvider() = default;
    ModuleLogProvider(Logger* logger, std::string module);

    void debug(std::string message) const;
    void info(std::string message) const;
    void warn(std::string message) const;
    void error(std::string message) const;

    const std::string& moduleName() const { return module_; }

private:
    Logger* logger_ = nullptr;
    std::string module_;
};

/// Host-facing API exposed on the data bus as `app_logger` for plugins.
class IAppLogger {
public:
    virtual ~IAppLogger() = default;
    virtual ModuleLogProvider module(const std::string& moduleName) = 0;
};

/// Central async logger: dedicated worker thread, non-blocking enqueue.
class Logger final : public IAppLogger {
public:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    ModuleLogProvider registerModule(std::string moduleName);
    ModuleLogProvider module(const std::string& moduleName) override;

    void submit(LogRecord record);

    void setEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_release); }
    bool isEnabled() const { return enabled_.load(std::memory_order_acquire); }

    void setFileLoggingEnabled(bool enabled);
    bool isFileLoggingEnabled() const { return file_logging_.load(std::memory_order_acquire); }

private:
    void openLogFile();
    void closeLogFile();
    void workerLoop();
    void writeRecord(const LogRecord& record);
    static const char* levelName(LogLevel level);

    std::atomic<bool> enabled_{true};
    std::atomic<bool> file_logging_{true};
    std::atomic<bool> stop_{false};
    std::mutex queue_mu_;
    std::condition_variable cv_;
    std::queue<LogRecord> queue_;
    std::thread worker_;
    std::mutex file_mu_;
    std::ofstream file_;
    std::unordered_set<std::string> registered_;
};

#endif // LOGGER_H
