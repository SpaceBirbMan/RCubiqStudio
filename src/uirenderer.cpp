#define STB_IMAGE_IMPLEMENTATION
#include "uirenderer.h"
#include <stb/stb_image.h>
#include <iostream>
#include <QPointer>
#include <QListWidget>
#include <QDialog>
#include <QStringListModel>

///////////////////////////////////////////////////////////////
//  helpers
///////////////////////////////////////////////////////////////

static QBoxLayout* makeLayout(CompositionType type) {
    switch (type) {
    case VBOX: return new QVBoxLayout;
    case HBOX: return new QHBoxLayout;
    case FREE: return new QVBoxLayout; // FREE needs custom positioning, fallback to VBOX
    default: return new QVBoxLayout;
    }
}

static Qt::Orientation toQtOrientation(ProgressBarOrientation orient) {
    return (orient == VERTICAL) ? Qt::Vertical : Qt::Horizontal;
}

///////////////////////////////////////////////////////////////
//  Render dispatcher
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderElement(UiElement* elem) {
    if (!elem) return nullptr;

    QWidget* result = nullptr;

    if (auto* pg = dynamic_cast<UiPage*>(elem)) result = renderPage(pg);
    else if (auto* ctx = dynamic_cast<UiContextMenu*>(elem)) result = renderContextMenu(ctx);
    else if (auto* win = dynamic_cast<UiWindow*>(elem)) result = renderWindow(win);
    else if (auto* fd = dynamic_cast<UiFileDialog*>(elem)) result = renderFileDialog(fd);
    else if (auto* tree = dynamic_cast<UiTreeView*>(elem)) result = renderTreeView(tree);
    else if (auto* list = dynamic_cast<UiListView*>(elem)) result = renderListView(list);
    else if (auto* grid = dynamic_cast<UiGridView*>(elem)) result = renderGridView(grid);
    else if (auto* c = dynamic_cast<UiContainer*>(elem)) result = renderContainer(c);
    else if (auto* t = dynamic_cast<UiTitle*>(elem)) {
        auto* l = new QLabel(QString::fromStdString(t->getText()));
        QFont f = l->font();
        switch (t->getFormat()) {
        case ITALIC: f.setItalic(true); break;
        case BOLD: f.setBold(true); break;
        case UNDERLINE: f.setUnderline(true); break;
        default: break;
        }
        l->setFont(f);
        
        QPointer<QLabel> lPtr = l;
        t->onChange = [lPtr, t]() {
            if (lPtr) {
                lPtr->setText(QString::fromStdString(t->getText()));
                QFont f = lPtr->font();
                f.setItalic(t->getFormat() == ITALIC);
                f.setBold(t->getFormat() == BOLD);
                f.setUnderline(t->getFormat() == UNDERLINE);
                lPtr->setFont(f);
            }
        };
        result = l;
    }
    else if (auto* b = dynamic_cast<UiButton*>(elem)) {
        auto* btn = new QPushButton(QString::fromStdString(b->text));
        if (b->onClick) {
            QObject::connect(btn, &QPushButton::clicked, [cb = b->onClick]() { cb(); });
        }
        QPointer<QPushButton> bPtr = btn;
        b->onChange = [bPtr, b]() { if(bPtr) bPtr->setText(QString::fromStdString(b->text)); };
        result = btn;
    }
    else if (auto* tb = dynamic_cast<UiToggleableButton*>(elem)) {
        auto* btn = new QPushButton(QString::fromStdString(tb->text));
        btn->setCheckable(true);
        btn->setChecked(tb->active);
        if (tb->onToggle) {
            QObject::connect(btn, &QPushButton::toggled, [cb = tb->onToggle](bool v) { cb(v); });
        }
        QPointer<QPushButton> bPtr = btn;
        tb->onChange = [bPtr, tb]() { if(bPtr) { bPtr->setText(QString::fromStdString(tb->text)); bPtr->setChecked(tb->active); } };
        result = btn;
    }
    else if (auto* cb = dynamic_cast<UiCheckBox*>(elem)) {
        auto* ch = new QCheckBox(QString::fromStdString(cb->text));
        ch->setChecked(cb->active);
        if (cb->onToggle)
            QObject::connect(ch, &QCheckBox::toggled, [cbfn = cb->onToggle](bool v){ cbfn(v); });
        QPointer<QCheckBox> cPtr = ch;
        cb->onChange = [cPtr, cb]() { if(cPtr) { cPtr->setText(QString::fromStdString(cb->text)); cPtr->setChecked(cb->active); } };
        result = ch;
    }
    else if (auto* in = dynamic_cast<UiInputField*>(elem)) {
        auto* le = new QLineEdit;
        le->setPlaceholderText(QString::fromStdString(in->hint));
        le->setText(QString::fromStdString(in->value));
        if (in->inputType == HIDDENTEXT) le->setEchoMode(QLineEdit::Password);
        if (in->onTextChanged) {
            QObject::connect(le, &QLineEdit::textChanged, [cb = in->onTextChanged](const QString& v){
                cb(v.toStdString());
            });
        }
        QPointer<QLineEdit> lPtr = le;
        in->onChange = [lPtr, in]() { if(lPtr) { lPtr->setPlaceholderText(QString::fromStdString(in->hint)); lPtr->setText(QString::fromStdString(in->value)); } };
        result = le;
    }
    else if (auto* sp = dynamic_cast<UiSpinField*>(elem)) {
        if (!sp->isFloat) {
            auto* sb = new QSpinBox;
            sb->setRange(sp->minValue, sp->maxValue);
            sb->setValue(sp->intValue);
            if (sp->onValueChanged)
                QObject::connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), [cb = sp->onValueChanged](int v){ cb(v); });
            QPointer<QSpinBox> sPtr = sb;
            sp->onChange = [sPtr, sp]() { if(sPtr) { sPtr->setRange(sp->minValue, sp->maxValue); sPtr->setValue(sp->intValue); } };
            result = sb;
        } else {
            auto* ds = new QDoubleSpinBox;
            ds->setRange(static_cast<double>(sp->minValue), static_cast<double>(sp->maxValue));
            ds->setValue(static_cast<double>(sp->intValue));
            if (sp->onValueChanged)
                QObject::connect(ds, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [cb = sp->onValueChanged](double v){ cb(static_cast<int>(v)); });
            QPointer<QDoubleSpinBox> sPtr = ds;
            sp->onChange = [sPtr, sp]() { if(sPtr) { sPtr->setRange(static_cast<double>(sp->minValue), static_cast<double>(sp->maxValue)); sPtr->setValue(static_cast<double>(sp->intValue)); } };
            result = ds;
        }
    }
    else if (auto* sl = dynamic_cast<UiSlider*>(elem)) {
        auto* s = new QSlider(Qt::Horizontal);
        s->setRange(sl->getMinValue(), sl->getMaxValue());
        s->setValue(sl->getValue());
        if (sl->onSlide)
            QObject::connect(s, &QSlider::valueChanged, [cb = sl->onSlide](int v){ cb(v); });
        QPointer<QSlider> sPtr = s;
        sl->onChange = [sPtr, sl]() { if(sPtr) { sPtr->setRange(sl->getMinValue(), sl->getMaxValue()); sPtr->setValue(sl->getValue()); } };
        result = s;
    }
    else if (auto* dl = dynamic_cast<UiDial*>(elem)) {
        auto* d = new QDial;
        d->setRange(dl->getMinValue(), dl->getMaxValue());
        d->setValue(dl->getValue());
        if (dl->onSlide) {
            QObject::connect(d, &QDial::valueChanged, [cb = dl->onSlide](int v){ cb(v); });
        }
        QPointer<QDial> dPtr = d;
        dl->onChange = [dPtr, dl]() { if(dPtr) { dPtr->setRange(dl->getMinValue(), dl->getMaxValue()); dPtr->setValue(dl->getValue()); } };
        result = d;
    }
    else if (auto* img = dynamic_cast<UiImageBox*>(elem)) {
        result = renderImageBox(img);
    }
    else if (auto* pb = dynamic_cast<UiProgressBar*>(elem)) {
        auto* p = new QProgressBar;
        p->setRange(pb->getMinValue(), pb->getMaxValue());
        p->setValue(pb->getValue());
        p->setOrientation(toQtOrientation(pb->getOrientation()));
        QPointer<QProgressBar> pPtr = p;
        pb->onChange = [pPtr, pb]() { if(pPtr) { pPtr->setRange(pb->getMinValue(), pb->getMaxValue()); pPtr->setValue(pb->getValue()); } };
        result = p;
    }
    else if (auto* cbx = dynamic_cast<UiComboBox*>(elem)) {
        auto* box = new QComboBox;
        for (const auto& it : cbx->items)
            box->addItem(QString::fromStdString(it));
        box->setCurrentIndex(cbx->currentIndex);
        if (cbx->onSelect) {
            QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), [cb = cbx->onSelect](int v){ cb(v); });
        }
        QPointer<QComboBox> cPtr = box;
        cbx->onChange = [cPtr, cbx]() { 
            if(cPtr) { 
                cPtr->clear();
                for(const auto& it : cbx->items) cPtr->addItem(QString::fromStdString(it));
                cPtr->setCurrentIndex(cbx->currentIndex);
            } 
        };
        result = box;
    }
    else {
        result = new QWidget;
    }

    if (result) {
        result->setObjectName(QString::fromStdString(elem->getName()));
    }
    return result;
}

///////////////////////////////////////////////////////////////
//  ListView rendering
///////////////////////////////////////////////////////////////

// QWidget* UiRenderer::renderList(UiListView* list) {
//     if (!list) return nullptr;

//     auto* widget = new QWidget;
//     auto* layout = new QVBoxLayout(widget);
//     layout->setContentsMargins(0, 0, 0, 0);

//     auto* listView = new QListView;
//     auto* model = new QStandardItemModel(listView);

//     for (const auto& item : list->getItems()) {
//         model->appendRow(new QStandardItem(QString::fromStdString(item)));
//     }

//     listView->setModel(model);
//     listView->setSelectionMode(QAbstractItemView::SingleSelection);

//     if (list->getSelectedIndex() >= 0) {
//         auto index = model->index(list->getSelectedIndex(), 0);
//         listView->setCurrentIndex(index);
//     }

//     // if (auto cb = list->getSelectedIndex()) {
//     //     QObject::connect(listView->selectionModel(), &QItemSelectionModel::currentRowChanged,
//     //                      [cb, model](const QModelIndex& current, const QModelIndex&) {
//     //                          if (current.isValid()) {
//     //                              cb(current.row());
//     //                          }
//     //                      });
//     // }

//     layout->addWidget(listView);
//     return widget;
// }

///////////////////////////////////////////////////////////////
//  TreeView rendering (stub - requires UiTreeView implementation)
///////////////////////////////////////////////////////////////

// QWidget* UiRenderer::renderTree(UiTreeView* tree) {
//     if (!tree) return nullptr;

//     auto* widget = new QWidget;
//     auto* layout = new QVBoxLayout(widget);
//     auto* treeView = new QTreeView;

//     // TODO: Implement tree population when UiTreeView has proper structure
//     // For now, return empty tree view
//     auto* model = new QStandardItemModel(treeView);
//     treeView->setModel(model);

//     layout->addWidget(treeView);
//     return widget;
// }

///////////////////////////////////////////////////////////////
//  ImageBox rendering
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderImageBox(UiImageBox* imgBox) {
    if (!imgBox) return nullptr;

    auto* btn = new QPushButton;
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btn->setMinimumSize(64, 64);
    btn->setStyleSheet("QPushButton { border: 1px dashed gray; }");

    auto updateButton = [btn, imgBox]() {
        if (imgBox->hasImage() && !imgBox->getPath().empty()) {
            QPixmap pixmap(QString::fromStdString(imgBox->getPath()));
            if (!pixmap.isNull()) {
                btn->setIcon(QIcon(pixmap));
                btn->setIconSize(pixmap.size().scaled(btn->size() - QSize(10, 10), Qt::KeepAspectRatio));
                btn->setText("");
                btn->setStyleSheet("");
                return;
            }
        }
        btn->setIcon(QIcon());
        btn->setText("Select image");
        btn->setStyleSheet("QPushButton { border: 1px dashed gray; }");
    };

    updateButton();

    QObject::connect(btn, &QPushButton::clicked, [btn, imgBox, updateButton]() {
        QString filePath = QFileDialog::getOpenFileName(
            btn->window(), "Select Image", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");

        if (!filePath.isEmpty()) {
            std::string path = filePath.toStdString();
            imgBox->setImage(path);

            int width, height, channels;
            unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
            if (!data) {
                std::cerr << "[UI] stbi_load failed: " << stbi_failure_reason() << std::endl;
                updateButton();
                return;
            }

            // Convert RGBA8 bytes to uint32_t RGBA pixels
            auto* pixels = new uint32_t[static_cast<size_t>(width) * height];
            for (int i = 0; i < width * height; ++i) {
                pixels[i] = (static_cast<uint32_t>(data[i * 4 + 3]) << 24) | // A
                            (static_cast<uint32_t>(data[i * 4 + 0]) << 16) | // R
                            (static_cast<uint32_t>(data[i * 4 + 1]) << 8)  | // G
                            (static_cast<uint32_t>(data[i * 4 + 2]));        // B
            }
            stbi_image_free(data);

            auto imgData = std::make_shared<ImageData>(pixels, static_cast<uint16_t>(width), static_cast<uint16_t>(height), 4);
            imgBox->setImageData(imgData);
            updateButton();
        }
    });

    return btn;
}

///////////////////////////////////////////////////////////////
//  Container rendering with composition support
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderContainer(UiContainer* container) {
    if (!container) return nullptr;

    auto* w = new QWidget;
    QBoxLayout* lay = makeLayout(container->getComposition());

    // FREE layout could use QGridLayout or absolute positioning - fallback to VBOX for now
    if (container->getComposition() == FREE) {
        lay->setSpacing(0);
    }

    w->setLayout(lay);

    for (const auto& ch : container->getChildrens()) {
        QWidget* child = renderElement(ch.get());
        if (child) lay->addWidget(child);
    }

    if (container->getComposition() != FREE) {
        lay->addStretch();
    }

    QPointer<QWidget> wPtr = w;
    QPointer<QBoxLayout> layPtr = lay;
    container->onChange = [wPtr, layPtr, container]() {
        if (wPtr && layPtr) {
            QLayoutItem* item;
            while ((item = layPtr->takeAt(0)) != nullptr) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
            for (const auto& ch : container->getChildrens()) {
                QWidget* child = renderElement(ch.get());
                if (child) layPtr->addWidget(child);
            }
            if (container->getComposition() != FREE) {
                layPtr->addStretch();
            }
        }
    };

    return w;
}

QWidget* UiRenderer::renderPage(UiPage* page) {
    if (!page) return nullptr;
    return renderContainer(page);
}

///////////////////////////////////////////////////////////////
//  Menu rendering
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderContextMenu(UiContextMenu* ctx) {
    if (!ctx || !ctx->target) return nullptr;
    
    QWidget* targetWidget = renderElement(ctx->target.get());
    if (targetWidget) {
        targetWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(targetWidget, &QWidget::customContextMenuRequested, [targetWidget, ctx](const QPoint& pos) {
            QMenu menu(targetWidget);
            for (const auto& ch : ctx->getChildrens()) {
                if (auto mb = std::dynamic_pointer_cast<UiMenuButton>(ch)) {
                    QAction* a = menu.addAction(QString::fromStdString(mb->text));
                    QObject::connect(a, &QAction::triggered, [mb](){ if(mb->onClick) mb->onClick(); });
                }
            }
            menu.exec(targetWidget->mapToGlobal(pos));
        });
        
        QPointer<QWidget> wPtr = targetWidget;
        ctx->onChange = [wPtr, ctx]() {
            if (wPtr) {
                // Changing the target element requires its own onChange to handle its update.
                // The context menu items are rebuilt on-the-fly dynamically.
            }
        };
    }
    return targetWidget;
}

QWidget* UiRenderer::renderTreeView(UiTreeView* tree) {
    auto tv = new QTreeView;
    auto model = new QStandardItemModel(tv);
    
    std::function<void(const std::vector<std::shared_ptr<UiTreeNode>>&, QStandardItem*)> fill;
    fill = [&fill](const std::vector<std::shared_ptr<UiTreeNode>>& nodes, QStandardItem* parent) {
        for(const auto& node : nodes) {
            auto item = new QStandardItem(QString::fromStdString(node->text));
            item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(node.get())), Qt::UserRole + 1);
            if (parent) parent->appendRow(item);
            fill(node->children, item);
        }
    };
    
    for (const auto& rootNode : tree->rootNodes) {
        auto item = new QStandardItem(QString::fromStdString(rootNode->text));
        item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(rootNode.get())), Qt::UserRole + 1);
        model->appendRow(item);
        fill(rootNode->children, item);
    }
    
    tv->setModel(model);
    tv->setHeaderHidden(true);
    
    QObject::connect(tv, &QTreeView::clicked, [tree, model](const QModelIndex& index) {
        auto item = model->itemFromIndex(index);
        if (item) {
            auto ptr = item->data(Qt::UserRole + 1).value<quintptr>();
            auto node = reinterpret_cast<UiTreeNode*>(ptr);
            if (node && node->onSelect) node->onSelect();
            if (tree->onNodeSelected && node) tree->onNodeSelected(node->text);
        }
    });

    QPointer<QTreeView> tvPtr = tv;
    tree->onChange = [tvPtr, tree]() {
        if (tvPtr) {
            auto m = new QStandardItemModel(tvPtr);
            std::function<void(const std::vector<std::shared_ptr<UiTreeNode>>&, QStandardItem*)> fill;
            fill = [&fill](const std::vector<std::shared_ptr<UiTreeNode>>& nodes, QStandardItem* parent) {
                for(const auto& node : nodes) {
                    auto item = new QStandardItem(QString::fromStdString(node->text));
                    item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(node.get())), Qt::UserRole + 1);
                    if (parent) parent->appendRow(item);
                    fill(node->children, item);
                }
            };
            for (const auto& rootNode : tree->rootNodes) {
                auto item = new QStandardItem(QString::fromStdString(rootNode->text));
                item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(rootNode.get())), Qt::UserRole + 1);
                m->appendRow(item);
                fill(rootNode->children, item);
            }
            tvPtr->setModel(m);
        }
    };
    return tv;
}

QWidget* UiRenderer::renderListView(UiListView* list) {
    auto lv = new QListView;
    auto model = new QStringListModel(lv);
    QStringList sl;
    for(const auto& str : list->items) sl << QString::fromStdString(str);
    model->setStringList(sl);
    lv->setModel(model);
    
    if (list->selectedIndex >= 0 && list->selectedIndex < sl.size()) {
        lv->setCurrentIndex(model->index(list->selectedIndex, 0));
    }
    
    QObject::connect(lv, &QListView::clicked, [list](const QModelIndex& index) {
        if(list->onSelect) list->onSelect(index.row());
    });
    
    QPointer<QListView> lvPtr = lv;
    list->onChange = [lvPtr, list]() {
        if(lvPtr && lvPtr->model()) {
            auto m = static_cast<QStringListModel*>(lvPtr->model());
            QStringList strings;
            for(const auto& str : list->items) strings << QString::fromStdString(str);
            m->setStringList(strings);
        }
    };
    return lv;
}

QWidget* UiRenderer::renderGridView(UiGridView* grid) {
    auto lw = new QListWidget;
    lw->setViewMode(QListView::IconMode);
    lw->setResizeMode(QListView::Adjust);
    lw->setSpacing(5);
    
    auto fill = [lw, grid]() {
        lw->clear();
        for (const auto& item : grid->items) {
            auto li = new QListWidgetItem(QString::fromStdString(item.text), lw);
            if (!item.imagePath.empty()) {
                li->setIcon(QIcon(QString::fromStdString(item.imagePath)));
            }
        }
    };
    fill();
    
    QObject::connect(lw, &QListWidget::itemClicked, [lw, grid](QListWidgetItem* item) {
        if(grid->onSelect) grid->onSelect(lw->row(item));
    });
    
    QPointer<QListWidget> lwPtr = lw;
    grid->onChange = [lwPtr, fill]() {
        if(lwPtr) fill();
    };
    return lw;
}

QWidget* UiRenderer::renderWindow(UiWindow* win) {
    auto dummy = new QWidget;
    dummy->hide();
    
    auto dialog = new QDialog(dummy);
    dialog->setWindowTitle(QString::fromStdString(win->windowTitle));
    
    auto lay = makeLayout(win->getComposition());
    dialog->setLayout(lay);
    for (const auto& ch : win->getChildrens()) {
        if (QWidget* w = renderElement(ch.get())) lay->addWidget(w);
    }
    
    QObject::connect(dialog, &QDialog::finished, [win](int) {
        win->isVisible = false;
        if(win->onClose) win->onClose();
    });
    
    if (win->isVisible) dialog->show();
    
    QPointer<QDialog> diagPtr = dialog;
    win->onChange = [diagPtr, win]() {
        if (diagPtr) {
            if (win->isVisible && !diagPtr->isVisible()) diagPtr->show();
            else if (!win->isVisible && diagPtr->isVisible()) diagPtr->hide();
            diagPtr->setWindowTitle(QString::fromStdString(win->windowTitle));
            
            auto layPtr = diagPtr->layout();
            if (layPtr) {
                QLayoutItem* item;
                while ((item = layPtr->takeAt(0)) != nullptr) {
                    if (item->widget()) item->widget()->deleteLater();
                    delete item;
                }
                for (const auto& ch : win->getChildrens()) {
                    QWidget* cw = renderElement(ch.get());
                    if (cw) layPtr->addWidget(cw);
                }
                if (win->getComposition() != FREE) {
                     qobject_cast<QBoxLayout*>(layPtr)->addStretch();
                }
            }
        }
    };
    
    return dummy;
}

QWidget* UiRenderer::renderFileDialog(UiFileDialog* fd) {
    auto btn = new QPushButton(QString::fromStdString(fd->title));
    QObject::connect(btn, &QPushButton::clicked, [btn, fd]() {
        QString file = QFileDialog::getOpenFileName(btn, QString::fromStdString(fd->title), "", QString::fromStdString(fd->filter));
        if (!file.isEmpty() && fd->onFileSelected) {
            fd->onFileSelected(file.toStdString());
        }
    });
    QPointer<QPushButton> bPtr = btn;
    fd->onChange = [bPtr, fd]() { if(bPtr) bPtr->setText(QString::fromStdString(fd->title)); };
    return btn;
}
///////////////////////////////////////////////////////////////
//  Root renderer into QTabWidget
///////////////////////////////////////////////////////////////

void UiRenderer::renderToTabWidget(std::shared_ptr<UiPage> root, QTabWidget* tabTarget) {
    if (!root || !tabTarget) return;

    QWidget* content = renderPage(root.get());
    if (content) {
        tabTarget->insertTab(root->getIndex(), content, QString::fromStdString(root->getTitle()));
    }
}

void UiRenderer::renderToTabWidget(const std::vector<std::shared_ptr<UiPage>>& pages, QTabWidget* tabTarget) {
    if (!tabTarget) return;

    for (const auto& page : pages) {
        if (page) {
            renderToTabWidget(page, tabTarget);
        }
    }
}
