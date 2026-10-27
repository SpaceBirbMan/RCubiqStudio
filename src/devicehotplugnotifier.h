#ifndef DEVICEHOTPLUGNOTIFIER_H
#define DEVICEHOTPLUGNOTIFIER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QCoreApplication>

class AppCore;

/// Listens for OS device connect/disconnect and asks DeviceManager to refresh enumeration.
class DeviceHotplugNotifier final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit DeviceHotplugNotifier(AppCore* core, QObject* parent = nullptr);

    void installOn(QCoreApplication* app);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onDebouncedRefresh();

private:
    void scheduleRefresh();

    AppCore* core_ = nullptr;
    QTimer debounce_;
};

#endif // DEVICEHOTPLUGNOTIFIER_H
