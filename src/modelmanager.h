#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include "fileloader.h"
#include "filesaver.h"

class ModelManager
{
public:
    ModelManager();

    FileLoader getLoader() {
        return loader;
    }

    FileSaver getSaver() {
        return saver;
    }

private:

    FileLoader loader = FileLoader();
    FileSaver saver = FileSaver();

};

#endif // MODELMANAGER_H
