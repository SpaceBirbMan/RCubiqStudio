#ifndef APPSHUTDOWN_H
#define APPSHUTDOWN_H

class AppCore;

/// True after `beginApplicationShutdown` has started (used to skip UI refresh during unload).
bool isApplicationShuttingDown();

/// Stop render tick, persist caches, unload engines. Safe to call multiple times.
void beginApplicationShutdown(AppCore* core);

#endif // APPSHUTDOWN_H
