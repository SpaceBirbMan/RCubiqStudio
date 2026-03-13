#ifndef UIRENDERER_H
#define UIRENDERER_H

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QProgressBar>
#include <QComboBox>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTreeView>
#include <QListView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QPixmap>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>

#include "abstractuinodes.h"

using namespace RUI;

class UiRenderer {
public:
    static QWidget* renderElement(UiElement* elem);
    static QWidget* renderContainer(UiContainer* container);
    static QWidget* renderPage(UiPage* page);
    static QWidget* renderImageBox(UiImageBox* imgBox);
    static QWidget* renderList(UiListView* list);
    static QWidget* renderMenu(UiMenu* menu);

    static void renderToTabWidget(std::shared_ptr<UiPage> root, QTabWidget* tabTarget);
    static void renderToTabWidget(const std::vector<std::shared_ptr<UiPage>>& pages, QTabWidget* tabTarget);
};

#endif // UIRENDERER_H
