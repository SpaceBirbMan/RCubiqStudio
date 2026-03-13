#include "directsender.h"

DirectSender::DirectSender() {
    // создать поток (?)
}

void DirectSender::send(void* getter, std::any data) {
    std::function<void(std::any)> callback = this->subs_callbacks_pairs.find(getter)->second;
    callback(std::move(data));
}
