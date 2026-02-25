#ifndef FILELOADER_H
#define FILELOADER_H

#include "misc.h"
#include <string>
#include <nlohmann/json.hpp>

class FileLoader
{
public:
    FileLoader();

    payload loadBin(std::string path);
    nlohmann::json loadJson(std::string path);
};

#endif // FILELOADER_H
