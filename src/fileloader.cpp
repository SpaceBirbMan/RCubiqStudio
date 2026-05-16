#include "fileloader.h"
#include <fstream>
#include <iostream>

FileLoader::FileLoader() {}

payload FileLoader::loadBin(std::string path) {

    try {
        std::ifstream file(path, std::ios::binary);
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        payload data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    } catch (...) {
        // TODO: catch
    }
    return {};
}

nlohmann::json FileLoader::loadJson(std::string path) {

    nlohmann::json j = nlohmann::json::object();

    try {
        std::ifstream file(path);
        if (file.is_open()) {
            file >> j;
            file.close();
        } else {
            std::cerr << "Cannot open JSON for reading: " << path << std::endl;
        }

        return j;
    } catch (...) {
        return nlohmann::json::object();
    }
}
