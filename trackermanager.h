#ifndef TRACKERMANAGER_H
#define TRACKERMANAGER_H

#include <memory>

struct TrackingData {
    // может хватить jsonа, так как неизвестно, что в данных будет + они всё равно в таблицу будут передваться
};

class ITracker {
public:
    virtual ~ITracker() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual TrackingData getData() const = 0;
};

class TrackerManager
{
private:
    std::unique_ptr<ITracker> currentTracker;

public:
    TrackerManager();
    ~TrackerManager();

    // Устанавливает активный трекер (например, OpenSeeFace, MediaPipe и т.д.)
    void setTracker(std::unique_ptr<ITracker> tracker);

    // Запускает/останавливает отслеживание
    bool startTracking();
    void stopTracking();

    // // Получает текущие данные позы/головы/лица (формат — обобщённый)
    // struct TrackingData {
    //     // Например: векторы поворота, трансляции, landmarks и т.п.
    //     // Конкретная структура зависит от требований системы
    // };
    // TrackingData getCurrentData() const;

    // Проверяет, запущен ли трекер
    bool isRunning() const;
};

#endif
