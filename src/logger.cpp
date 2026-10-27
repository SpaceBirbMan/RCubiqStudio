#include "logger.h"

#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <iostream>
#include <iomanip>
#include <sstream>

// todo: Убрать отсюда Qt

namespace {

std::string logFilePath() {
    const QString dir = AppSettings::writablePluginsCacheDirectory();
    QDir d(dir);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));
    return (d.filePath(QStringLiteral("studio.log"))).toStdString();
}

} // namespace

ModuleLogProvider::ModuleLogProvider(Logger* logger, std::string module)
    : logger_(logger), module_(std::move(module)) {}

void ModuleLogProvider::debug(std::string message) const {
    if (!logger_) return;
    LogRecord r;
    r.module = module_;
    r.level = LogLevel::Debug;
    r.message = std::move(message);
    logger_->submit(std::move(r));
}

void ModuleLogProvider::info(std::string message) const {
    if (!logger_) return;
    LogRecord r;
    r.module = module_;
    r.level = LogLevel::Info;
    r.message = std::move(message);
    logger_->submit(std::move(r));
}

void ModuleLogProvider::warn(std::string message) const {
    if (!logger_) return;
    LogRecord r;
    r.module = module_;
    r.level = LogLevel::Warn;
    r.message = std::move(message);
    logger_->submit(std::move(r));
}

void ModuleLogProvider::error(std::string message) const {
    if (!logger_) return;
    LogRecord r;
    r.module = module_;
    r.level = LogLevel::Error;
    r.message = std::move(message);
    logger_->submit(std::move(r));
}

Logger::Logger() {
    file_logging_.store(AppSettings::logToFile(), std::memory_order_release);
    if (file_logging_.load(std::memory_order_acquire))
        openLogFile();
    worker_ = std::thread(&Logger::workerLoop, this);
}

void Logger::openLogFile() {
    std::lock_guard<std::mutex> lk(file_mu_);
    if (file_.is_open())
        return;
    file_.open(logFilePath(), std::ios::out | std::ios::app);
}

void Logger::closeLogFile() {
    std::lock_guard<std::mutex> lk(file_mu_);
    if (file_.is_open())
        file_.close();
}

void Logger::setFileLoggingEnabled(bool enabled) {
    file_logging_.store(enabled, std::memory_order_release);
    if (enabled)
        openLogFile();
    else
        closeLogFile();
}

Logger::~Logger() {
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        stop_.store(true, std::memory_order_release);
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
    std::lock_guard<std::mutex> lk(file_mu_);
    if (file_.is_open())
        file_.close();
}

ModuleLogProvider Logger::registerModule(std::string moduleName) {
    if (!moduleName.empty()) {
        std::lock_guard<std::mutex> lk(queue_mu_);
        registered_.insert(moduleName);
    }
    return ModuleLogProvider(this, std::move(moduleName));
}

ModuleLogProvider Logger::module(const std::string& moduleName) {
    return registerModule(moduleName);
}

void Logger::submit(LogRecord record) {
    if (!enabled_.load(std::memory_order_acquire))
        return;
    if (record.module.empty())
        record.module = "unknown";
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        queue_.push(std::move(record));
    }
    cv_.notify_one();
}

const char* Logger::levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

void Logger::writeRecord(const LogRecord& record) {
    const auto tt = std::chrono::system_clock::to_time_t(record.time);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    std::ostringstream line;
    line << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
         << " [" << levelName(record.level) << "] "
         << "[" << record.module << "] "
         << record.message;

    const std::string text = line.str();
    if (record.level >= LogLevel::Warn)
        std::cerr << text << '\n';
    else
        std::cout << text << '\n';

    if (!file_logging_.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::mutex> lk(file_mu_);
    if (file_.is_open()) {
        file_ << text << '\n';
        file_.flush();
    }
}

void Logger::workerLoop() {
    for (;;) {
        LogRecord rec;
        {
            std::unique_lock<std::mutex> lk(queue_mu_);
            cv_.wait(lk, [this] {
                return stop_.load(std::memory_order_acquire) || !queue_.empty();
            });
            if (stop_.load(std::memory_order_acquire) && queue_.empty())
                return;
            rec = std::move(queue_.front());
            queue_.pop();
        }
        writeRecord(rec);
    }
}
