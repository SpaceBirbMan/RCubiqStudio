#include "crashhandler.h"
#include "appmessage.h"
#include "eventmanager.h"

#include <cpptrace/cpptrace.hpp>

#include <QCoreApplication>
#include <QMetaObject>
#include <QProcess>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <csignal>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <cstdint>
#include <mach-o/dyld.h>
#include <climits>
#endif
#endif

namespace {

std::string g_logs_dir;
std::atomic<bool> g_listeners_started{false};
std::terminate_handler g_prev_terminate = nullptr;

#ifdef _WIN32
LPTOP_LEVEL_EXCEPTION_FILTER g_prev_unhandled = nullptr;
#endif

std::filesystem::path executable_path()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (!n || n >= MAX_PATH)
        return {};
    return std::filesystem::path(buf);
#elif defined(__APPLE__)
    char buf[PATH_MAX]{};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return {};
    std::error_code ec;
    return std::filesystem::weakly_canonical(std::filesystem::path(buf), ec);
#else
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::weakly_canonical("/proc/self/exe", ec);
    if (ec)
        return {};
    return p;
#endif
}

std::string normalize_logs_dir(const std::filesystem::path& dir)
{
    return dir.lexically_normal().generic_string();
}

void commit_file_buffer(FILE* f)
{
    if (!f)
        return;
    std::fflush(f);
#ifdef _WIN32
    if (_fileno(f) >= 0)
        _commit(_fileno(f));
#else
    const int fd = fileno(f);
    if (fd >= 0)
        fsync(fd);
#endif
}

std::string utc_timestamp_for_filename()
{
    const auto t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%dT%H%M%S");
    return oss.str();
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& u8)
{
    if (u8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8.data(), (int)u8.size(), nullptr, 0);
    if (n <= 0)
        return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8.data(), (int)u8.size(), w.data(), n);
    return w;
}

void show_native_crash_dialog(const std::string& path_utf8, const char* kind)
{
    std::wstring pathw = utf8_to_wide(path_utf8);
    std::wstring kindw = utf8_to_wide(std::string(kind ? kind : ""));
    std::wstring msg = L"Произошёл сбой (" + kindw + L").\n\nОтчёт (рядом с .exe, папка crash_logs):\n" + pathw;
    MessageBoxW(nullptr, msg.c_str(), L"Сбой программы",
        MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND);
}

void show_native_pending_dialog(const std::string& path_utf8)
{
    std::wstring pathw = utf8_to_wide(path_utf8);
    std::wstring msg =
        L"Прошлый запуск завершился сбоем.\n\nОтчёт:\n" + pathw;
    MessageBoxW(nullptr, msg.c_str(), L"Сбой при прошлом запуске",
        MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND);
}
#else
void show_native_crash_dialog(const std::string& /*path_utf8*/, const char* /*kind*/) {}
void show_native_pending_dialog(const std::string& /*path_utf8*/) {}
#endif

void write_pending_marker(const std::string& log_path)
{
    if (g_logs_dir.empty())
        return;
    const std::string marker = g_logs_dir + "/last_crash_pending.txt";
    if (FILE* m = std::fopen(marker.c_str(), "wb")) {
        std::fwrite(log_path.data(), 1, log_path.size(), m);
        std::fputc('\n', m);
        commit_file_buffer(m);
        std::fclose(m);
    }
}

std::string write_crash_log_impl(const char* kind, const std::string& extra, bool mark_pending)
{
    if (g_logs_dir.empty())
        return {};
    const std::string fname = "crash_" + utc_timestamp_for_filename() + "_" + kind + ".log";
    const std::string path = g_logs_dir + '/' + fname;

    const std::string header = std::string("crash log\nkind=") + kind + '\n' + extra + "\n\nStack trace:\n";

    if (FILE* primary = std::fopen(path.c_str(), "wb")) {
        std::fwrite(header.data(), 1, header.size(), primary);
        commit_file_buffer(primary);
        if (mark_pending)
            write_pending_marker(path);

        std::string trace_str;
        try {
            std::ostringstream trace_ss;
            cpptrace::generate_trace().print(trace_ss, false);
            trace_str = trace_ss.str();
        } catch (...) {
            trace_str = "(cpptrace::generate_trace failed)\n";
        }
        std::fwrite(trace_str.data(), 1, trace_str.size(), primary);
        commit_file_buffer(primary);
        std::fclose(primary);
    }
    return path;
}

void on_terminate_hook()
{
    const std::string path = write_crash_log_impl("terminate", {}, true);
#ifdef _WIN32
    if (!path.empty())
        show_native_crash_dialog(path, "terminate");
#endif
    if (g_prev_terminate)
        g_prev_terminate();
    std::abort();
}

#ifdef _WIN32
LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* info)
{
    std::ostringstream oss;
    if (info && info->ExceptionRecord) {
        oss << "ExceptionCode=0x" << std::hex << info->ExceptionRecord->ExceptionCode << std::dec << '\n';
        oss << "ExceptionAddress=" << info->ExceptionRecord->ExceptionAddress << '\n';
    }
    const std::string path = write_crash_log_impl("seh", oss.str(), true);
    if (!path.empty())
        show_native_crash_dialog(path, "seh");
    if (g_prev_unhandled) {
        const LONG r = g_prev_unhandled(info);
        if (r != EXCEPTION_CONTINUE_SEARCH)
            return r;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void posix_signal_handler(int sig)
{
    std::string extra = "signal=" + std::to_string(sig);
#if defined(SIGBUS) && (SIGBUS != SIGSEGV)
    if (sig == SIGBUS)
        extra += " (SIGBUS)";
#endif
    if (sig == SIGSEGV)
        extra += " (SIGSEGV)";
    const std::string path = write_crash_log_impl("signal", extra, true);
#ifdef _WIN32
    if (!path.empty())
        show_native_crash_dialog(path, "signal");
#endif
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

} // namespace

void CrashHandler::startErrorListener()
{
    if (g_listeners_started.exchange(true))
        return;

    g_prev_terminate = std::get_terminate();
    std::set_terminate(on_terminate_hook);

#ifdef _WIN32
    g_prev_unhandled = SetUnhandledExceptionFilter(unhandled_exception_filter);
#endif

    struct SigReg {
        int sig;
    };
    const SigReg regs[] = {
        {SIGABRT},
        {SIGSEGV},
        {SIGILL},
        {SIGFPE},
#if defined(SIGBUS)
        {SIGBUS},
#endif
    };
    for (SigReg r : regs) {
        void (*prev)(int) = std::signal(r.sig, posix_signal_handler);
        static_cast<void>(prev);
    }
}

CrashHandler::CrashHandler(EventManager& em)
    : em_(&em)
{
    std::filesystem::path exe = executable_path();
    if (exe.empty())
        exe = std::filesystem::current_path();
    crash_logs_dir_ = normalize_logs_dir(exe.parent_path() / "crash_logs");
}

CrashHandler::~CrashHandler() = default;

void CrashHandler::install()
{
    if (installed_)
        return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(crash_logs_dir_), ec);
    g_logs_dir = crash_logs_dir_;
    startErrorListener();
    installed_ = true;
}

void CrashHandler::publishPendingIfAny()
{
    if (!em_)
        return;
    const std::string marker_path = crash_logs_dir_ + "/last_crash_pending.txt";
    std::ifstream m(marker_path);
    if (!m)
        return;
    std::string path;
    std::getline(m, path);
    m.close();
    if (path.empty())
        return;
    std::filesystem::remove(std::filesystem::path(marker_path));
    em_->sendMessage(AppMessage("Core", kCrashPendingMessage, path));
#ifdef _WIN32
    show_native_pending_dialog(path);
#endif
}

void CrashHandler::forceCreateDump(const std::string& dump_dir)
{
    const std::string dir = dump_dir.empty() ? crash_logs_dir_ : dump_dir;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(dir), ec);
    const std::string prev = g_logs_dir;
    g_logs_dir = dir;
    write_crash_log_impl("manual_dump", {}, false);
    g_logs_dir = prev;
}

void CrashHandler::forceCloseApp()
{
    if (QCoreApplication::instance()) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            "quit",
            Qt::QueuedConnection
        );
    } else {
        std::exit(0);
    }
}

void CrashHandler::forceRebootApp()
{
    const QString exe = QCoreApplication::applicationFilePath();
    if (!exe.isEmpty())
        QProcess::startDetached(exe, QStringList());
    if (QCoreApplication::instance())
        QCoreApplication::exit(0);
    else
        std::exit(0);
}

void CrashHandler::forceRestartModule()
{
    if (em_)
        em_->sendMessage(AppMessage("Core", "crash_handler_restart_module", 0));
}

void CrashHandler::forceBlockModule()
{
    if (em_)
        em_->sendMessage(AppMessage("Core", "crash_handler_block_module", 0));
}
