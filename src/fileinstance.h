#ifndef FILEINSTANCE_H
#define FILEINSTANCE_H

#include "filetypes.h"
#include <any>
#include <vector>

class FileInstance
{
public:
    FileInstance() = default;

    ~FileInstance();

    void setData(std::any data) { this->data = data; }
    void setFileType(int ftype) { this->fileType = ftype; }
    void setFlags(std::vector<bool> flags) { this->flags = flags; }

    int getFileType() { return this->fileType; }
    std::any getData() { return this->data; }
    std::vector<bool> getFlags() { return this->flags; }

private:

    int fileType;

    std::any data;

    std::vector<bool> flags {};

};

#endif // FILEINSTANCE_H
