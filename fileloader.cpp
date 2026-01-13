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
        // todo: catch
    }

}

nlohmann::json FileLoader::loadJson(std::string path) {

    nlohmann::json j;

    try {
        std::ifstream file(path);
        if (file.is_open()) {
            file >> j;
            file.close();
        } else {
            std::cerr << "Error opening file for reading!" << std::endl;
        }

        return j;
    } catch (...) {
        return nullptr;
    }
}
