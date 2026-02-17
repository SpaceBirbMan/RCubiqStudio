#ifndef UIRENDERER_H
#define UIRENDERER_H

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
#include <QPixmap>
#include <QFileDialog>

#include "AbstractUiNodes.h"

using namespace RUI;

class UiRenderer {
public:
    static QWidget* renderElement(UiElement* elem);
    static QWidget* renderContainer(UiContainer* container);
    static QWidget* renderGroup(UiGroup* group);
    static QWidget* renderPage(UiPage* page);
    static QWidget* renderTree(UiTreeView* tree);
    static QWidget* renderImageBox(UiImageBox* imgBox);

    static void renderToTabWidget(std::shared_ptr<UiPage> root, QTabWidget* tabTarget);
};

#endif // UIRENDERER_H
