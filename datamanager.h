
#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "appcore.h"
#include "modelmanager.h"
#include "cachemanager.h"
#include "crashreportmanager.h"
#include "dynamiclibrary.h"
#include <any>
#include <vector>
#include <set>
#include "misc.h"
#include <unordered_map>

class DataManager
{
public:
    DataManager(AppCore* acptr);

    void initialize();

    void dummy(int a) { std::cout << "a1" << std::endl;}
    void dummy2() { std::cout << "a2" << std::endl;}

    const std::string name = "DataManager"; // имя модуля, используется в методах с описанием отправителя/получаетля

private:



    AppCore* appCorePtr = nullptr; // ядро

    /// Внутренние модули модуля
    ModelManager modelManager = ModelManager(); // были указатели, вспомнить зачем
    CacheManager cacheManager;
    CrashReportManager crashReportManager;

    std::unordered_map<std::string, std::shared_ptr<DynamicLibrary>> libsPool {};

    void tryToLoadCache();

    void loadModel(std::vector<std::string> exts);

    void resolveFuncTable(LibMeta meta);

    void saveFiles(std::any data);
    void resolveApi(LibMeta meta);

};

#endif // DATAMANAGER_H
