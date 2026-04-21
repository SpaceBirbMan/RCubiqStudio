#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include "eventmanager.h"
#include <string>

class CrashHandler {

private:

    void _startErrorListener();
    void _stopErrorListener();

    void _blockModule();
    void _restartModule();
    void _rebootApp();
    void _closeApp();
    void _createDump();

    EventManager* _em = nullptr;

public:

    CrashHandler() = default;
    CrashHandler(EventManager &em);

    ~CrashHandler();

    void forceRebootApp();
    void forceRestartModule();
    void forceBlockModule();
    void forceCloseApp();
    void forceCreateDump(std::string dump_path = "/crash_dumps/");

};

#endif // CRASHHANDLER_H
