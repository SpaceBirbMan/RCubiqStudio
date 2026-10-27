#include "devicehotplugnotifier.h"
#include "appcore.h"
#include "eventmanager.h"
#include "logger.h"

#include <QCoreApplication>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbt.h>
#endif

DeviceHotplugNotifier::DeviceHotplugNotifier(AppCore* core, QObject* parent)
    : QObject(parent)
    , core_(core)
{
    debounce_.setSingleShot(true);
    debounce_.setInterval(400);
    connect(&debounce_, &QTimer::timeout, this, &DeviceHotplugNotifier::onDebouncedRefresh);
}

void DeviceHotplugNotifier::installOn(QCoreApplication* app)
{
    if (app)
        app->installNativeEventFilter(this);
}

bool DeviceHotplugNotifier::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/)
{
#ifdef _WIN32
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;

    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_DEVICECHANGE)
        return false;

    switch (msg->wParam) {
    case DBT_DEVICEARRIVAL:
    case DBT_DEVICEREMOVECOMPLETE:
    case DBT_DEVNODES_CHANGED:
        scheduleRefresh();
        break;
    default:
        break;
    }
#else
    (void)eventType;
    (void)message;
#endif
    return false;
}

void DeviceHotplugNotifier::scheduleRefresh()
{
    if (!debounce_.isActive())
        debounce_.start();
}

void DeviceHotplugNotifier::onDebouncedRefresh()
{
    if (!core_)
        return;
    core_->getEventManager().getLogger().module("DeviceHotplug").info("hotplug: refreshing device list");
    core_->getEventManager().dispatchImmediately(
        AppMessage("DeviceManager", "refresh_devices", 0));
}
