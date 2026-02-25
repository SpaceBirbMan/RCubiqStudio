#include "filesaver.h"
#include <fstream>
#include <iostream>

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
        std::ofstream file(path);
        if (file.is_open()) {
            file << data.dump(0); // Используем параметр data, а не несуществующую j
            // file.close(); // Необязательно, деструктор закроет
        } else {
            std::cerr << "Error opening file for writing: " << path << std::endl;
        }
    } catch (...) {
        std::cerr << "Exception occurred in saveJson for file: " << path << std::endl;
    }
}
