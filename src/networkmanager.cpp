#include "networkmanager.h"

#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <vector>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
#endif

#ifdef _WIN32
#include <winhttp.h>
#include <shellapi.h>
#include <winreg.h>
#endif

struct WsShared {
    std::mutex mu;
    std::atomic<bool> stop{false};
#ifdef _WIN32
    HINTERNET socket = nullptr;
#endif
};

NetworkManager::NetworkManager(AppCore* appCore)
    : appCore(appCore)
    , log_(appCore->getEventManager().getLogger().module("NetMan"))
{

    this->appCore->getEventManager().subscribe(name, "http_get", &NetworkManager::httpGet, this);
    this->appCore->getEventManager().subscribe(name, "http_post", &NetworkManager::httpPost, this);
    this->appCore->getEventManager().subscribe(name, "http_delete", &NetworkManager::httpDelete, this);
    this->appCore->getEventManager().subscribe(name, "http_head", &NetworkManager::httpHead, this);
    this->appCore->getEventManager().subscribe(name, "http_patch", &NetworkManager::httpPatch, this);
    this->appCore->getEventManager().subscribe(name, "http_connect", &NetworkManager::httpConnect, this);
    this->appCore->getEventManager().subscribe(name, "http_trace", &NetworkManager::httpTrace, this);
    this->appCore->getEventManager().subscribe(name, "browser_auth", &NetworkManager::browserAuth, this);
    this->appCore->getEventManager().subscribe(name, "websocket_open", &NetworkManager::openWebSocket, this);
}

void NetworkManager::stopAuthListen(const AuthListen& listen)
{
    if (!listen.server || !listen.stopped)
        return;
    if (listen.stopped->exchange(true))
        return;
    listen.server->stop();
}

NetworkManager::~NetworkManager()
{
    std::thread previous;
    {
        std::lock_guard<std::mutex> lock(authMutex_);
        for (const auto& listen : authServers_)
            stopAuthListen(listen);
        if (authThread_.joinable())
            previous = std::move(authThread_);
    }
    if (previous.joinable())
        previous.join();

    std::thread socketPrevious;
    std::shared_ptr<WsShared> socket;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        socket = socketRun_;
        if (socket)
            socket->stop.store(true);
        if (socketThread_.joinable())
            socketPrevious = std::move(socketThread_);
    }
    if (socketPrevious.joinable())
        socketPrevious.join();
}

namespace {

#ifdef _WIN32
std::wstring widen(const std::string& text)
{
    if (text.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(size > 0 ? size : 0, L'\0');
    if (size > 0)
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

struct HttpHandle {
    HINTERNET handle = nullptr;
    ~HttpHandle()
    {
        if (handle)
            WinHttpCloseHandle(handle);
    }
};

HttpRespond winHttps(const std::string& method, const HttpRequest& request)
{
    HttpRespond respond;
    respond.reason = request.reason;
    const std::wstring url = widen(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        respond.code = "0";
        respond.raw = "bad url";
        return respond;
    }

    HttpHandle session;
    session.handle = WinHttpOpen(L"Studio", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) {
        respond.code = "0";
        respond.raw = "winhttp open";
        return respond;
    }
    WinHttpSetTimeouts(session.handle, 10000, 10000, 30000, 30000);

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    HttpHandle connection;
    connection.handle = WinHttpConnect(session.handle, host.c_str(), parts.nPort, 0);
    if (!connection.handle) {
        respond.code = "0";
        respond.raw = "winhttp connect";
        return respond;
    }

    std::wstring target(parts.lpszUrlPath, parts.dwUrlPathLength);
    target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (target.empty())
        target = L"/";
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HttpHandle req;
    req.handle = WinHttpOpenRequest(connection.handle, widen(method).c_str(), target.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req.handle) {
        respond.code = "0";
        respond.raw = "winhttp request";
        return respond;
    }

    const bool hasBody = method != "GET" && method != "HEAD";
    const std::string body = !hasBody ? std::string()
        : (request.rawBody.empty() ? request.object.dump() : request.rawBody);
    std::wstring headers;
    for (const auto& entry : request.headers)
        headers += widen(entry.first) + L": " + widen(entry.second) + L"\r\n";
    if (hasBody)
        headers += L"Content-Type: " + widen(request.contentType.empty() ? "application/json" : request.contentType) + L"\r\n";

    const wchar_t* headerPtr = headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str();
    void* bodyPtr = (hasBody && !body.empty())
        ? static_cast<void*>(const_cast<char*>(body.data()))
        : static_cast<void*>(WINHTTP_NO_REQUEST_DATA);
    const BOOL sent = WinHttpSendRequest(
        req.handle,
        headerPtr,
        headers.empty() ? 0 : static_cast<DWORD>(-1),
        bodyPtr,
        hasBody ? static_cast<DWORD>(body.size()) : 0,
        hasBody ? static_cast<DWORD>(body.size()) : 0,
        0);
    if (!sent || !WinHttpReceiveResponse(req.handle, nullptr)) {
        respond.code = "0";
        respond.raw = "winhttp " + std::to_string(GetLastError());
        return respond;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(req.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    respond.code = std::to_string(status);

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.handle, &available) || available == 0)
            break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(req.handle, chunk.data(), available, &read))
            break;
        chunk.resize(read);
        respond.raw += chunk;
    }
    try {
        respond.object = json::parse(respond.raw);
    } catch (...) {
        respond.object = json::object();
    }
    return respond;
}

std::string narrow(const std::wstring& text)
{
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size : 0, '\0');
    if (size > 0)
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring registryString(HKEY root, const std::wstring& path, const wchar_t* value)
{
    DWORD bytes = 0;
    if (RegGetValueW(root, path.c_str(), value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes < sizeof(wchar_t))
        return {};
    std::wstring text(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, path.c_str(), value, RRF_RT_REG_SZ, nullptr, text.data(), &bytes) != ERROR_SUCCESS)
        return {};
    while (!text.empty() && text.back() == L'\0')
        text.pop_back();
    return text;
}

std::wstring defaultBrowserCommand()
{
    const std::wstring progId = registryString(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice",
        L"ProgId");
    if (progId.empty())
        return {};
    const std::wstring key = progId + L"\\shell\\open\\command";
    std::wstring command = registryString(HKEY_CURRENT_USER, L"Software\\Classes\\" + key, nullptr);
    if (command.empty())
        command = registryString(HKEY_CLASSES_ROOT, key, nullptr);
    return command;
}

std::string browserFileName(const std::wstring& command)
{
    std::wstring path = command;
    if (!path.empty() && path.front() == L'"') {
        const size_t end = path.find(L'"', 1);
        path = path.substr(1, end == std::wstring::npos ? std::wstring::npos : end - 1);
    } else {
        const size_t space = path.find(L' ');
        if (space != std::wstring::npos)
            path.resize(space);
    }
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        path = path.substr(slash + 1);
    return narrow(path);
}

bool runCommand(std::wstring command)
{
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

struct BrowserOpen {
    bool ok = false;
    std::string text;
};

BrowserOpen openBrowser(const std::string& url)
{
    BrowserOpen result;
    const std::wstring stored = defaultBrowserCommand();
    const std::string file = browserFileName(stored);
    if (!stored.empty()) {
        std::wstring command = stored;
        const size_t slot = command.find(L"%1");
        const std::wstring raw = widen(url);
        const std::wstring quoted = L"\"" + raw + L"\"";
        if (slot != std::wstring::npos) {
            const bool insideQuotes = slot > 0 && command[slot - 1] == L'"';
            command.replace(slot, 2, insideQuotes ? raw : quoted);
        } else {
            command += L" " + quoted;
        }
        if (runCommand(command)) {
            result.ok = true;
            result.text = file.empty() ? "Открыт браузер по умолчанию" : "Открыт браузер по умолчанию: " + file;
            return result;
        }
        result.text = "Не запустилась команда браузера по умолчанию";
        if (!file.empty())
            result.text += ": " + file;
    }

    const INT_PTR opened = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (opened > 32) {
        result.ok = true;
        result.text = "Открыт обработчик https по умолчанию";
        return result;
    }

    std::wstring command = L"cmd.exe /c start \"\" \"" + widen(url) + L"\"";
    if (runCommand(std::move(command))) {
        result.ok = true;
        result.text = "Открыт браузер по умолчанию через start";
        return result;
    }
    if (result.text.empty())
        result.text = "Браузер по умолчанию не открылся, код " + std::to_string(static_cast<long long>(opened));
    return result;
}
#else
struct BrowserOpen {
    bool ok = false;
    std::string text;
};

BrowserOpen openBrowser(const std::string& url)
{
    BrowserOpen result;
    const std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1";
    result.ok = std::system(command.c_str()) == 0;
    result.text = result.ok ? "Открыт браузер по умолчанию" : "Браузер по умолчанию не открылся";
    return result;
}
#endif

const char* kRedirectPage = R"HTML(<!doctype html><meta charset="utf-8"><title>Twitch</title>
<p id="m">Возвращаемся в студию…</p>
<script>
const hash = location.hash.charAt(0) === '#' ? location.hash.slice(1) : '';
const query = location.search.charAt(0) === '?' ? location.search.slice(1) : '';
fetch('/callback', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: hash || query})
  .then(function(){ document.getElementById('m').textContent = 'Можно закрыть вкладку'; });
</script>)HTML";

} // namespace

void NetworkManager::executeRequest(const std::string& method, HttpRequest r) {
    log_.info(method + " " + r.url);
    std::string scheme = "http";
    std::string rest = r.url;
    const auto schemePos = rest.find("://");
    if (schemePos != std::string::npos) {
        scheme = rest.substr(0, schemePos);
        rest = rest.substr(schemePos + 3);
    }
    for (char& ch : scheme)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    std::string host;
    std::string path = "/";
    const auto pathStart = rest.find('/');
    if (pathStart != std::string::npos) {
        host = rest.substr(0, pathStart);
        path = rest.substr(pathStart);
    } else {
        host = rest;
    }

    auto reply = [this, &r, &method](HttpRespond respond) {
        this->appCore->getEventManager().sendMessage(
            AppMessage(name, r.sender + "_http_" + toLowerStr(method) + "_respond", std::move(respond)));
    };

#ifdef _WIN32
    if (scheme == "https") {
        reply(winHttps(method, r));
        return;
    }
#endif

    if (host.empty()) {
        HttpRespond respond;
        respond.code = "400";
        respond.reason = r.reason;
        reply(std::move(respond));
        return;
    }

    httplib::Headers headers;
    for (const auto& entry : r.headers)
        headers.emplace(entry.first, entry.second);
    const std::string body = r.rawBody.empty() ? r.object.dump() : r.rawBody;
    const std::string contentType = r.contentType.empty() ? std::string("application/json") : r.contentType;

    try {
        httplib::Client client(scheme + "://" + host);
        if (!client.is_valid()) {
            HttpRespond respond;
            respond.code = "501";
            respond.reason = r.reason;
            respond.raw = "failed to open http client";
            reply(std::move(respond));
            return;
        }
        client.set_connection_timeout(10, 0);
        client.set_read_timeout(30, 0);

        httplib::Result res;
        if (method == "GET")
            res = client.Get(path, headers);
        else if (method == "POST")
            res = client.Post(path, headers, body, contentType);
        else if (method == "PUT")
            res = client.Put(path, headers, body, contentType);
        else if (method == "PATCH")
            res = client.Patch(path, headers, body, contentType);
        else if (method == "DELETE")
            res = client.Delete(path, headers);
        else if (method == "HEAD")
            res = client.Head(path, headers);
        else {
            HttpRespond respond;
            respond.code = "501";
            respond.reason = r.reason;
            reply(std::move(respond));
            return;
        }

        HttpRespond respond;
        respond.reason = r.reason;
        if (res) {
            respond.code = std::to_string(res->status);
            respond.raw = res->body;
            try {
                respond.object = json::parse(res->body);
            } catch (...) {
                respond.object = json::object();
            }
        } else {
            respond.code = "0";
            respond.object = json::object();
        }
        reply(std::move(respond));
    } catch (const std::exception& e) {
        HttpRespond respond;
        respond.code = "501";
        respond.reason = r.reason;
        respond.raw = e.what();
        reply(std::move(respond));
    }
}

void NetworkManager::browserAuth(BrowserAuth request)
{
    log_.info("browser_auth " + request.url);
    std::thread previous;
    {
        std::lock_guard<std::mutex> lock(authMutex_);
        for (const auto& listen : authServers_)
            stopAuthListen(listen);
        if (authThread_.joinable())
            previous = std::move(authThread_);
    }
    if (previous.joinable())
        std::thread([gone = std::move(previous)]() mutable { gone.join(); }).detach();
    std::lock_guard<std::mutex> lock(authMutex_);
    authThread_ = std::thread([this, request] { runBrowserAuth(std::move(request)); });
}

void NetworkManager::runBrowserAuth(BrowserAuth request)
{
    auto finish = [this, &request](HttpRespond respond) {
        respond.reason = request.reason;
        if (!respond.object.is_object())
            respond.object = json::object();
        respond.object["state"] = request.state;
        this->appCore->getEventManager().sendMessage(
            AppMessage(name, request.sender + "_browser_auth_respond", std::move(respond)));
    };

    std::mutex waitMutex;
    std::condition_variable waitCv;
    bool done = false;
    std::string captured;
    auto makeServer = [&]() {
        auto server = std::make_shared<httplib::Server>();
        server->Get("/", [&](const httplib::Request&, httplib::Response& response) {
            response.set_content(kRedirectPage, "text/html; charset=utf-8");
        });
        server->Post("/callback", [&](const httplib::Request& incoming, httplib::Response& response) {
            {
                std::lock_guard<std::mutex> lock(waitMutex);
                captured = incoming.body;
                done = true;
            }
            waitCv.notify_all();
            response.set_content("ok", "text/plain");
        });
        return server;
    };

    auto v4 = makeServer();
    auto v6 = makeServer();
    v6->set_ipv6_v6only(true);
    const bool v4ok = v4->bind_to_port("127.0.0.1", request.port);
    const bool v6ok = v6->bind_to_port("::1", request.port);
    std::vector<AuthListen> bound;
    if (v4ok)
        bound.push_back(AuthListen{v4});
    if (v6ok)
        bound.push_back(AuthListen{v6});
    {
        std::lock_guard<std::mutex> lock(authMutex_);
        authServers_ = bound;
    }

    auto stopBound = [&]() {
        for (const auto& listen : bound)
            stopAuthListen(listen);
    };

    const int timeout = request.timeoutSec < 5 ? 5 : request.timeoutSec;
    if (bound.empty()) {
        finish(HttpRespond{"501", request.reason, json::object({{"message", "Порт localhost занят"}})});
    } else {
        const BrowserOpen opened = openBrowser(request.url);
        log_.info(opened.text);
        std::cout << "[NetMan] " << opened.text << std::endl;
        if (!opened.ok) {
            stopBound();
            finish(HttpRespond{"501", request.reason, json::object({{"message", opened.text}})});
        } else {
            HttpRespond note;
            note.code = "102";
            note.object = json::object({{"message", opened.text}});
            finish(std::move(note));
            std::thread stopper([&] {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCv.wait_for(lock, std::chrono::seconds(timeout), [&] { return done; });
            stopBound();
        });
        std::thread extra;
        if (bound.size() == 2)
            extra = std::thread([v6] { v6->listen_after_bind(); });
        bound.front().server->listen_after_bind();
        {
            std::lock_guard<std::mutex> lock(waitMutex);
            done = true;
        }
        waitCv.notify_all();
        stopBound();
        if (extra.joinable())
            extra.join();
        stopper.join();

        HttpRespond respond;
        respond.raw = captured;
        respond.code = captured.empty() ? "408" : "200";
        respond.object = json::object();
            finish(std::move(respond));
        }
    }

    std::lock_guard<std::mutex> lock(authMutex_);
    if (!authServers_.empty() && !bound.empty() && authServers_.front().server == bound.front().server)
        authServers_.clear();
}

void NetworkManager::httpGet(HttpRequest r) {
    executeRequest("GET", r);
}

void NetworkManager::httpPost(HttpRequest r) {
    executeRequest("POST", r);
}

void NetworkManager::httpPut(HttpRequest r) {
    executeRequest("PUT", r);
}

void NetworkManager::httpPatch(HttpRequest r) {
    executeRequest("PATCH", r);
}

void NetworkManager::httpDelete(HttpRequest r) {
    executeRequest("DELETE", r);
}

void NetworkManager::httpHead(HttpRequest r) {
    executeRequest("HEAD", r);
}

void NetworkManager::httpConnect(HttpRequest r) {
    HttpRespond respond;
    respond.code = "501";
    this->appCore->getEventManager().sendMessage(
        AppMessage(name, r.sender + "_http_connect_respond", respond)
        );
}

void NetworkManager::httpTrace(HttpRequest r) {
    HttpRespond respond;
    respond.code = "501";
    this->appCore->getEventManager().sendMessage(
        AppMessage(name, r.sender + "_http_trace_respond", respond)
        );
}

void NetworkManager::openWebSocket(WebSocketOpen request)
{
    std::thread previous;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (socketRun_)
            socketRun_->stop.store(true);
        if (socketThread_.joinable())
            previous = std::move(socketThread_);
        socketRun_.reset();
    }
    if (previous.joinable())
        std::thread([gone = std::move(previous)]() mutable { gone.join(); }).detach();
    if (request.url.empty() || request.sender.empty())
        return;

    log_.info("websocket " + request.url);
    auto shared = std::make_shared<WsShared>();
    std::lock_guard<std::mutex> lock(socketMutex_);
    socketRun_ = shared;
    socketThread_ = std::thread([this, request, shared] {
        runWebSocket(request, shared);
    });
}

void NetworkManager::runWebSocket(WebSocketOpen request, std::shared_ptr<WsShared> shared)
{
    auto post = [this, request](std::string text, bool closed) {
        WebSocketFrame frame;
        frame.reason = request.reason;
        frame.text = std::move(text);
        frame.closed = closed;
        appCore->getEventManager().sendMessage(
            AppMessage(name, request.sender + "_websocket_frame", std::move(frame)));
    };

#ifndef _WIN32
    post("websocket только в Windows", true);
    return;
#else
    auto fail = [&](const std::string& text) {
        if (!shared->stop.load())
            post(text, true);
    };

    const std::string& url = request.url;
    bool tls = false;
    INTERNET_PORT port = 0;
    std::string rest;
    if (url.rfind("wss://", 0) == 0) {
        tls = true;
        port = 443;
        rest = url.substr(6);
    } else if (url.rfind("ws://", 0) == 0) {
        port = 80;
        rest = url.substr(5);
    } else {
        fail("плохой адрес сокета");
        return;
    }
    const auto slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    const std::string pathUtf = slash == std::string::npos ? std::string("/") : rest.substr(slash);
    const auto colon = hostport.rfind(':');
    if (colon != std::string::npos) {
        try {
            port = static_cast<INTERNET_PORT>(std::stoi(hostport.substr(colon + 1)));
        } catch (...) {
            fail("плохой порт сокета");
            return;
        }
        hostport.resize(colon);
    }
    const std::wstring host = widen(hostport);
    const std::wstring path = widen(pathUtf.empty() ? "/" : pathUtf);
    if (host.empty()) {
        fail("пустой адрес сокета");
        return;
    }

    HttpHandle session;
    session.handle = WinHttpOpen(L"Studio", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HttpHandle connection;
    HttpHandle req;
    if (session.handle) {
        WinHttpSetTimeouts(session.handle, 5000, 5000, 5000, 1000);
        connection.handle = WinHttpConnect(session.handle, host.c_str(), port, 0);
    }
    if (connection.handle) {
        req.handle = WinHttpOpenRequest(connection.handle, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, tls ? WINHTTP_FLAG_SECURE : 0);
    }
    if (!req.handle || !WinHttpSetOption(req.handle, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
        fail("сокет не открылся");
        return;
    }
    if (!WinHttpSendRequest(req.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(req.handle, nullptr)) {
        fail("сокет не ответил " + std::to_string(GetLastError()));
        return;
    }
    HINTERNET upgraded = WinHttpWebSocketCompleteUpgrade(req.handle, 0);
    WinHttpCloseHandle(req.handle);
    req.handle = nullptr;
    if (!upgraded) {
        fail("сокет не переключился " + std::to_string(GetLastError()));
        return;
    }
    if (shared->stop.load()) {
        WinHttpCloseHandle(upgraded);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(shared->mu);
        shared->socket = upgraded;
    }

    std::string message;
    std::vector<char> buffer(65536);
    while (!shared->stop.load()) {
        DWORD read = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
        const DWORD err = WinHttpWebSocketReceive(upgraded, buffer.data(), static_cast<DWORD>(buffer.size()), &read, &type);
        if (err == ERROR_WINHTTP_TIMEOUT)
            continue;
        if (err != ERROR_SUCCESS || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
            break;
        message.append(buffer.data(), read);
        const bool done = type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE
            || type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        if (!done)
            continue;
        post(std::move(message), false);
        message.clear();
    }

    bool own = false;
    {
        std::lock_guard<std::mutex> lock(shared->mu);
        if (shared->socket == upgraded) {
            shared->socket = nullptr;
            own = true;
        }
    }
    if (own)
        WinHttpCloseHandle(upgraded);
    if (!shared->stop.load())
        post({}, true);
#endif
}
