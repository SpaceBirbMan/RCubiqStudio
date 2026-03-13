#define STB_IMAGE_IMPLEMENTATION
#include "uirenderer.h"
#include <stb/stb_image.h>
#include <iostream>

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

    if (auto* pg = dynamic_cast<UiPage*>(elem))
        return renderPage(pg);

    if (auto* c = dynamic_cast<UiContainer*>(elem))
        return renderContainer(c);

    if (auto* t = dynamic_cast<UiTitle*>(elem)) {
        auto* l = new QLabel(QString::fromStdString(t->getText()));
        QFont f = l->font();
        switch (t->getFormat()) {
        case ITALIC: f.setItalic(true); break;
        case BOLD: f.setBold(true); break;
        case UNDERLINE: f.setUnderline(true); break;
        default: break;
        }
        l->setFont(f);
        return l;
    }

    if (auto* b = dynamic_cast<UiButton*>(elem)) {
        auto* btn = new QPushButton(QString::fromStdString(b->text));
        if (b->onClick) {
            QObject::connect(btn, &QPushButton::clicked, [cb = b->onClick]() { cb(); });
        }
        return btn;
    }

    if (auto* tb = dynamic_cast<UiToggleableButton*>(elem)) {
        auto* btn = new QPushButton(QString::fromStdString(tb->text));
        btn->setCheckable(true);
        btn->setChecked(tb->active);
        if (tb->onToggle) {
            QObject::connect(btn, &QPushButton::toggled, [cb = tb->onToggle](bool v) { cb(v); });
        }
        return btn;
    }

    if (auto* cb = dynamic_cast<UiCheckBox*>(elem)) {
        auto* ch = new QCheckBox(QString::fromStdString(cb->text));
        ch->setChecked(cb->active);
        if (cb->onToggle)
            QObject::connect(ch, &QCheckBox::toggled, [cbfn = cb->onToggle](bool v){ cbfn(v); });
        return ch;
    }

    if (auto* in = dynamic_cast<UiInputField*>(elem)) {
        auto* le = new QLineEdit;
        le->setPlaceholderText(QString::fromStdString(in->hint));
        le->setText(QString::fromStdString(in->value));
        if (in->inputType == HIDDENTEXT) le->setEchoMode(QLineEdit::Password);
        if (in->onTextChanged) {
            QObject::connect(le, &QLineEdit::textChanged, [cb = in->onTextChanged](const QString& v){
                cb(v.toStdString());
            });
        }
        return le;
    }

    if (auto* sp = dynamic_cast<UiSpinField*>(elem)) {
        if (!sp->isFloat) {
            auto* sb = new QSpinBox;
            sb->setRange(sp->minValue, sp->maxValue);
            sb->setValue(sp->intValue);
            if (sp->onValueChanged)
                QObject::connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), [cb = sp->onValueChanged](int v){ cb(v); });
            return sb;
        } else {
            auto* ds = new QDoubleSpinBox;
            ds->setRange(static_cast<double>(sp->minValue), static_cast<double>(sp->maxValue));
            ds->setValue(static_cast<double>(sp->intValue));
            if (sp->onValueChanged)
                QObject::connect(ds, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [cb = sp->onValueChanged](double v){ cb(static_cast<int>(v)); });
            return ds;
        }
    }

    if (auto* sl = dynamic_cast<UiSlider*>(elem)) {
        auto* s = new QSlider(Qt::Horizontal);
        s->setRange(sl->getMinValue(), sl->getMaxValue());
        s->setValue(sl->getValue());
        if (sl->onSlide)
            QObject::connect(s, &QSlider::valueChanged, [cb = sl->onSlide](int v){ cb(v); });
        return s;
    }

    if (auto* dl = dynamic_cast<UiDial*>(elem)) {
        auto* d = new QDial;
        d->setRange(dl->getMinValue(), dl->getMaxValue());
        d->setValue(dl->getValue());
        if (dl->onSlide) {
            QObject::connect(d, &QDial::valueChanged, [cb = dl->onSlide](int v){ cb(v); });
        }
        return d;
    }

    if (auto* img = dynamic_cast<UiImageBox*>(elem)) {
        return renderImageBox(img);
    }

    if (auto* pb = dynamic_cast<UiProgressBar*>(elem)) {
        auto* p = new QProgressBar;
        p->setRange(pb->getMinValue(), pb->getMaxValue());
        p->setValue(pb->getValue());
        p->setOrientation(toQtOrientation(pb->getOrientation()));
        return p;
    }

    if (auto* cbx = dynamic_cast<UiComboBox*>(elem)) {
        auto* box = new QComboBox;
        for (const auto& it : cbx->items)
            box->addItem(QString::fromStdString(it));
        box->setCurrentIndex(cbx->currentIndex);
        if (cbx->onSelect) {
            QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), [cb = cbx->onSelect](int v){ cb(v); });
        }
        return box;
    }

    if (auto* m = dynamic_cast<UiMenu*>(elem)) {
        return renderMenu(m);
    }

    // if (auto* tv = dynamic_cast<UiTreeView*>(elem)) {
    //     return renderTree(tv);
    // }

    if (auto* lv = dynamic_cast<UiListView*>(elem)) {
        return renderList(lv);
    }

    return new QWidget;
}

///////////////////////////////////////////////////////////////
//  ListView rendering
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderList(UiListView* list) {
    if (!list) return nullptr;

    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* listView = new QListView;
    auto* model = new QStandardItemModel(listView);

    for (const auto& item : list->getItems()) {
        model->appendRow(new QStandardItem(QString::fromStdString(item)));
    }

    listView->setModel(model);
    listView->setSelectionMode(QAbstractItemView::SingleSelection);

    if (list->getSelectedIndex() >= 0) {
        auto index = model->index(list->getSelectedIndex(), 0);
        listView->setCurrentIndex(index);
    }

    // if (auto cb = list->getSelectedIndex()) {
    //     QObject::connect(listView->selectionModel(), &QItemSelectionModel::currentRowChanged,
    //                      [cb, model](const QModelIndex& current, const QModelIndex&) {
    //                          if (current.isValid()) {
    //                              cb(current.row());
    //                          }
    //                      });
    // }

    layout->addWidget(listView);
    return widget;
}

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
        if (imgBox->hasImage() && !imgBox->imagePath.empty()) {
            QPixmap pixmap(QString::fromStdString(imgBox->imagePath));
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

    for (const auto& ch : container->getChildren()) {
        QWidget* child = renderElement(ch.get());
        if (child) lay->addWidget(child);
    }

    if (container->getComposition() != FREE) {
        lay->addStretch();
    }

    return w;
}

QWidget* UiRenderer::renderPage(UiPage* page) {
    if (!page) return nullptr;
    return renderContainer(page);
}

///////////////////////////////////////////////////////////////
//  Menu rendering
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderMenu(UiMenu* menu) {
    if (!menu) return nullptr;

    // Создаем само Qt-меню
    auto* qMenu = new QMenu();

    // // Заполняем действиями
    // for (const auto& btnPtr : menu->getButtons()) {
    //     if (btnPtr) {
    //         // Предполагаем, что у UiMenuButton есть метод getTitle() и onClick() или подобный
    //         // Если UiMenuButton наследуется от UiButton, используем его текст и клик
    //         std::string title = btnPtr->getTitle(); // Или btnPtr->text, если публичное поле

    //         auto* action = qMenu->addAction(QString::fromStdString(title));

    //         // FIX: Подключение сигнала
    //         // Предположим, у UiMenuButton есть метод trigger() или callback onClick
    //         // Если UiMenuButton имеет структуру как UiButton:
    //         if (btnPtr->onClick) {
    //             QObject::connect(action, &QAction::triggered, [cb = btnPtr->onClick]() {
    //                 cb();
    //             });
    //         }
    //     }
    // }

    auto* triggerBtn = new QPushButton("Menu");
    QObject::connect(triggerBtn, &QPushButton::clicked, [triggerBtn, qMenu]() {
        qMenu->popup(triggerBtn->mapToGlobal(QPoint(0, triggerBtn->height())));
    });

    // FIXME: не вешается на виджет
    return triggerBtn;
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
