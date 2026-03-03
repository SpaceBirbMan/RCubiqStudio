#ifndef CONTROLLAYER_H
#define CONTROLLAYER_H

#include <string>
#include <vector>

class ViewportWidget;

class ControlLayer
{
private:

    std::vector<std::string> hid_queue {}; // очередь сообщений c HID

    std::vector<std::string> commands {};

    void* ui_md = nullptr;

    ViewportWidget* parrent = nullptr;

public:

    ControlLayer(ViewportWidget* parrent);

    void setHidQueue(std::vector<std::string> hid_queue);
    const std::vector<std::string> getHidQueue();
    void setCommandQ(std::vector<std::string> command_q);
    const std::vector<std::string> getCommandQ();
    void setParrent(ViewportWidget* parrent);
    const ViewportWidget* getParrent();

    void handle();

};

#endif // CONTROLLAYER_H
