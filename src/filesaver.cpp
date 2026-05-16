#include "filesaver.h"
#include <fstream>
#include <iostream>
#include <filesystem>

FileSaver::FileSaver() {}

void FileSaver::saveBin(std::string path, const payload &data) {

    try {
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) { // Проверка, открыт ли файл
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            // file.close(); // Необязательно, деструктор закроет
        } else {
            std::cerr << "Error opening file for writing: " << path << std::endl;
        }
    } catch (...) {
        std::cerr << "Exception occurred in saveBin for file: " << path << std::endl;
    }
}

void FileSaver::saveJson(std::string path, const nlohmann::json &data) { // Предполагаем, что тип параметра data - nlohmann::json

    try {
        namespace fs = std::filesystem;
        const fs::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
            (void)ec;
        }
        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "Error opening JSON file for writing: " << path << std::endl;
            return;
        }
        file << data.dump(0);
        file.flush();
        if (!file.good())
            std::cerr << "Error writing JSON file: " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in saveJson for file: " << path << " — " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Exception occurred in saveJson for file: " << path << std::endl;
    }
}
