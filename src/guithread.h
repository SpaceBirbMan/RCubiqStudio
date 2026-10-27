#ifndef GUITHREAD_H
#define GUITHREAD_H

#include <functional>

// Костыль
class QObject;

void installGuiDispatcher(QObject* owner);
void invokeOnGui(std::function<void()> fn);

#endif
