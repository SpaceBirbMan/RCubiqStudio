#ifndef FILESAVER_H
#define FILESAVER_H

#include <string>
#include <any>
#include "misc.h"
#include <nlohmann/json.hpp>

class FileSaver
{
public:
    FileSaver();

    void saveBin(std::string path, const payload &data);

    void saveJson(std::string path, const nlohmann::json &data);

    //void saveIniFile(std::string name, std::)

    // json
};

#endif // FILESAVER_H

