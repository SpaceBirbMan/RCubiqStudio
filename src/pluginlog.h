#ifndef PLUGINLOG_H
#define PLUGINLOG_H

#include "appmessage.h"
#include "ieventmanager.h"
#include "logger.h"

// Логи с плагинов, непонятно зачем они так работают, исправить, перепридумать!

namespace PluginLogTopics {
inline constexpr const char* kReceiver = "Logger";
inline constexpr const char* kLogWrite = "log_write";
inline constexpr const char* kStatusMessage = "status_message";
}

/// Log from a plugin DLL into the host Logger (async worker thread).
inline void pluginLog(IEventManager* em, const std::string& module, const std::string& message,
                      LogLevel level = LogLevel::Info)
{
    if (!em)
        return;
    LogRecord rec;
    rec.module = module;
    rec.level = level;
    rec.message = message;
    em->sendMessage(AppMessage(PluginLogTopics::kReceiver, PluginLogTopics::kLogWrite, std::move(rec)));
}

inline void pluginStatus(IEventManager* em, const std::string& text)
{
    if (!em)
        return;
    em->sendMessage(AppMessage("Core", PluginLogTopics::kStatusMessage, text));
}

#endif // PLUGINLOG_H
