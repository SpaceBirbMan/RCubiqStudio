#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "appcore.h"
#include "rcqapi.h"
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// В MinGW нет GetAddrInfoExCancel, а порт httplib включает этот путь.
#ifdef CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO
#undef CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO
#endif
#include <httplib.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class NetworkManager
{
private:
    AppCore* appCore;
    std::string name = "NetMan";
    ModuleLogProvider log_;

    void httpGet(HttpRequest r);
    void httpPost(HttpRequest r);
    void httpPut(HttpRequest r);
    void httpDelete(HttpRequest r);
    void httpHead(HttpRequest r);
    void httpPatch(HttpRequest r);
    void httpConnect(HttpRequest r);
    void httpTrace(HttpRequest r);
    void browserAuth(BrowserAuth request);
    void openWebSocket(WebSocketOpen request);
    void executeRequest(const std::string& method, HttpRequest r);
    void runBrowserAuth(BrowserAuth request);
    void runWebSocket(WebSocketOpen request, std::shared_ptr<struct WsShared> shared);

    struct AuthListen {
        std::shared_ptr<httplib::Server> server;
        std::shared_ptr<std::atomic<bool>> stopped = std::make_shared<std::atomic<bool>>(false);
    };

    static void stopAuthListen(const AuthListen& listen);

    std::mutex authMutex_;
    std::thread authThread_;
    std::vector<AuthListen> authServers_;

    std::mutex socketMutex_;
    std::thread socketThread_;
    std::shared_ptr<struct WsShared> socketRun_;

    static std::string toLowerStr(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

public:
    NetworkManager(AppCore* appCore);
    ~NetworkManager();
};

#endif // NETWORKMANAGER_H
