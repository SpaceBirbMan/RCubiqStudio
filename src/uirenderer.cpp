#define STB_IMAGE_IMPLEMENTATION
#include "uirenderer.h"
#include <stb/stb_image.h>
#include <iostream>
#include <QPointer>
#include <QListWidget>
#include <QDialog>
#include <QStringListModel>
#include <QPainter>
#include <QTimer>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolBox>
#include <QEvent>
#include <algorithm>
#include <memory>
#include <vector>

namespace {

/// Qt wraps each QToolBox page in a QScrollArea (qtbase qtoolbox.cpp). That inner vertical
/// scrollbar only shrinks the page; we turn it off and size the QScrollArea to the page's
/// full layout height so the section opens to its natural length. Outer RUI UiScrollBox stays.
static void configureQtToolBoxPageScrollAreas(QToolBox* tb) {
    if (!tb) return;
    const QList<QScrollArea*> areas =
        tb->findChildren<QScrollArea*>(QString(), Qt::FindDirectChildrenOnly);
    for (QScrollArea* sa : areas) {
        sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        sa->setWidgetResizable(true);
        QWidget* w = sa->widget();
        if (!w) continue;
        if (QLayout* lay = w->layout())
            lay->activate();
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        const int ih = std::max(w->minimumSizeHint().height(), 1);
        w->setMinimumHeight(ih);
        // Without this, Qt keeps a short viewport and clips; min height grows the page row.
        sa->setMinimumHeight(ih);
        sa->setMaximumHeight(QWIDGETSIZE_MAX);
        sa->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }
}

/// Keeps per-page scroll policies and minimum heights in sync after resize / tab change / rebuild.
class ToolBoxPageStretcher final : public QObject {
public:
    explicit ToolBoxPageStretcher(QToolBox* tb) : QObject(tb), m_tb(tb) {
        connect(tb, &QToolBox::currentChanged, this, [this](int) { apply(); });
        tb->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_tb && (event->type() == QEvent::Resize || event->type() == QEvent::Show
                                || event->type() == QEvent::LayoutRequest))
            apply();
        return QObject::eventFilter(watched, event);
    }

private:
    void apply() {
        if (!m_tb) return;
        configureQtToolBoxPageScrollAreas(m_tb);
    }

    QToolBox* m_tb = nullptr;
};

} // namespace

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

namespace {

// Double-click on editable UiTitle label → inline QLineEdit (cannot use `new class` in new-expr on all compilers)
class TitleEditFilter : public QObject {
public:
    QPointer<QLabel>      lbl;
    QPointer<QHBoxLayout> hlay;
    RUI::UiTitle*         node = nullptr;

    TitleEditFilter(QLabel* l, QHBoxLayout* h, RUI::UiTitle* n, QObject* parent)
        : QObject(parent), lbl(l), hlay(h), node(n) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != lbl.data() || event->type() != QEvent::MouseButtonDblClick)
            return false;
        if (!lbl || !hlay || !node)
            return false;

        auto* le = new QLineEdit(lbl->text());
        le->selectAll();
        const int idx = hlay->indexOf(lbl);
        hlay->removeWidget(lbl);
        lbl->hide();
        hlay->insertWidget(idx, le);
        le->setFocus();

        auto committed = std::make_shared<bool>(false);
        auto commitEdit = [this, le, committed]() {
            if (*committed)
                return;
            const int i = hlay ? hlay->indexOf(le) : -1;
            if (i < 0)
                return;
            *committed = true;
            const QString newText = le->text();
            node->setText(newText.toStdString());
            if (lbl)
                lbl->setText(newText);
            hlay->removeWidget(le);
            le->deleteLater();
            if (lbl) {
                hlay->insertWidget(i, lbl);
                lbl->show();
            }
            if (node->onTextEdited)
                node->onTextEdited(newText.toStdString());
        };
        QObject::connect(le, &QLineEdit::returnPressed, commitEdit);
        QObject::connect(le, &QLineEdit::editingFinished, commitEdit);
        return true;
    }
};

} // namespace

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
    else if (auto* cpi = dynamic_cast<UiColorPicker*>(elem)) result = renderColorPicker(cpi);
    else if (auto* tree = dynamic_cast<UiTreeView*>(elem)) result = renderTreeView(tree);
    else if (auto* list = dynamic_cast<UiListView*>(elem)) result = renderListView(list);
    else if (auto* grid = dynamic_cast<UiGridView*>(elem)) result = renderGridView(grid);
    else if (auto* canvas = dynamic_cast<UiCanvas*>(elem)) result = renderCanvas(canvas);
    else if (auto* tb = dynamic_cast<UiToolBox*>(elem)) result = renderToolBox(tb);
    else if (auto* scroll = dynamic_cast<UiScrollBox*>(elem)) result = renderScrollBox(scroll);
    else if (auto* c = dynamic_cast<UiContainer*>(elem)) result = renderContainer(c);
    else if (auto* t = dynamic_cast<UiTitle*>(elem)) {
        auto applyFont = [](QLabel* lbl, TextFormat fmt) {
            QFont f = lbl->font();
            f.setItalic(fmt == ITALIC);
            f.setBold(fmt == BOLD);
            f.setUnderline(fmt == UNDERLINE);
            lbl->setFont(f);
        };

        if (t->editable) {
            // Wrap label + inline edit in a stacked widget-like container
            auto* container = new QWidget;
            auto* hlay = new QHBoxLayout(container);
            hlay->setContentsMargins(0, 0, 0, 0);

            auto* lbl = new QLabel(QString::fromStdString(t->getText()));
            applyFont(lbl, t->getFormat());
            hlay->addWidget(lbl);
            hlay->addStretch();

            lbl->installEventFilter(new TitleEditFilter(lbl, hlay, t, lbl));

            QPointer<QLabel> lPtr = lbl;
            t->onChange = [lPtr, t, applyFont]() {
                if (lPtr) {
                    lPtr->setText(QString::fromStdString(t->getText()));
                    applyFont(lPtr, t->getFormat());
                }
            };
            result = container;
        } else {
            auto* l = new QLabel(QString::fromStdString(t->getText()));
            applyFont(l, t->getFormat());
            QPointer<QLabel> lPtr = l;
            t->onChange = [lPtr, t, applyFont]() {
                if (lPtr) {
                    lPtr->setText(QString::fromStdString(t->getText()));
                    applyFont(lPtr, t->getFormat());
                }
            };
            result = l;
        }
    }
    // UiCheckBox before UiButton / UiToggleableButton (inheritance)
    else if (auto* cb = dynamic_cast<UiCheckBox*>(elem)) {
        auto* ch = new QCheckBox(QString::fromStdString(cb->text));
        ch->setChecked(cb->active);
        if (cb->onToggle)
            QObject::connect(ch, &QCheckBox::toggled, [cbfn = cb->onToggle](bool v){ cbfn(v); });
        QPointer<QCheckBox> cPtr = ch;
        cb->onChange = [cPtr, cb]() { if(cPtr) { cPtr->setText(QString::fromStdString(cb->text)); cPtr->setChecked(cb->active); } };
        result = ch;
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
    else if (auto* b = dynamic_cast<UiButton*>(elem)) {
        auto* btn = new QPushButton(QString::fromStdString(b->text));
        if (b->onClick) {
            QObject::connect(btn, &QPushButton::clicked, [cb = b->onClick]() { cb(); });
        }
        QPointer<QPushButton> bPtr = btn;
        b->onChange = [bPtr, b]() { if(bPtr) bPtr->setText(QString::fromStdString(b->text)); };
        result = btn;
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

    {
        bool fillsVerticalSpace = false;
        for (const auto& ch : container->getChildrens()) {
            QWidget* child = renderElement(ch.get());
            if (!child) continue;
            // Only outer UiScrollBox should grab free height; UiToolBox uses its natural size
            // inside that scroll (or elsewhere).
            const int sf = dynamic_cast<RUI::UiScrollBox*>(ch.get()) ? 1 : 0;
            if (sf) fillsVerticalSpace = true;
            lay->addWidget(child, sf);
        }
        if (!fillsVerticalSpace && container->getComposition() != FREE) {
            lay->addStretch();
        }
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
            bool fillsVerticalSpace = false;
            for (const auto& ch : container->getChildrens()) {
                QWidget* child = renderElement(ch.get());
                if (!child) continue;
                const int sf = dynamic_cast<RUI::UiScrollBox*>(ch.get()) ? 1 : 0;
                if (sf) fillsVerticalSpace = true;
                layPtr->addWidget(child, sf);
            }
            if (!fillsVerticalSpace && container->getComposition() != FREE) {
                layPtr->addStretch();
            }
        }
    };

    return w;
}

QWidget* UiRenderer::renderScrollBox(UiScrollBox* scroll) {
    if (!scroll) return nullptr;

    auto* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);

    auto mapPolicy = [](SliderPolicy p) {
        if (p == ALWAYS) return Qt::ScrollBarAlwaysOn;
        if (p == NEVER) return Qt::ScrollBarAlwaysOff;
        return Qt::ScrollBarAsNeeded;
    };

    sa->setHorizontalScrollBarPolicy(mapPolicy(scroll->getSliderHPolicy()));
    sa->setVerticalScrollBarPolicy(mapPolicy(scroll->getSliderVPolicy()));

    auto* content = new QWidget;
    // Height from children so the outer QScrollArea scrolls the whole block (e.g. toolbox).
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* lay = makeLayout(scroll->getComposition());
    content->setLayout(lay);

    auto populateScrollInner = [scroll](QBoxLayout* targetLay) {
        for (const auto& ch : scroll->getChildrens()) {
            QWidget* child = renderElement(ch.get());
            if (child) {
                child->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                targetLay->addWidget(child, 0);
            }
        }
        // No trailing stretch: it would pin content to viewport height and hide the outer scrollbar.
    };
    populateScrollInner(lay);

    sa->setWidget(content);

    QPointer<QScrollArea> saPtr = sa;
    QPointer<QWidget> cPtr = content;
    QPointer<QBoxLayout> layPtr = lay;

    scroll->onChange = [saPtr, cPtr, layPtr, scroll, populateScrollInner]() {
        if (saPtr && cPtr && layPtr) {
            QLayoutItem* item;
            while ((item = layPtr->takeAt(0)) != nullptr) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
            populateScrollInner(layPtr);
            
            // Re-apply policies in case they changed
            auto mapP = [](SliderPolicy p) {
                if (p == ALWAYS) return Qt::ScrollBarAlwaysOn;
                if (p == NEVER) return Qt::ScrollBarAlwaysOff;
                return Qt::ScrollBarAsNeeded;
            };
            saPtr->setHorizontalScrollBarPolicy(mapP(scroll->getSliderHPolicy()));
            saPtr->setVerticalScrollBarPolicy(mapP(scroll->getSliderVPolicy()));
        }
    };

    return sa;
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

namespace {

// TesterV1 / bgfx: 0xRRGGBBAA
static QColor u32RgbaToQColor(uint32_t v) {
    return QColor(int((v >> 24) & 0xFF), int((v >> 16) & 0xFF), int((v >> 8) & 0xFF), int(v & 0xFF));
}

static uint32_t qColorToU32Rgba(const QColor& c) {
    return (uint32_t(c.red()) << 24) | (uint32_t(c.green()) << 16) | (uint32_t(c.blue()) << 8) | uint32_t(c.alpha());
}

} // namespace

QWidget* UiRenderer::renderColorPicker(UiColorPicker* cp) {
    if (!cp) return nullptr;

    auto*    wrap = new QWidget;
    auto*    hlay = new QHBoxLayout(wrap);
    hlay->setContentsMargins(0, 0, 0, 0);
    auto*    swatch = new QFrame(wrap);
    auto*    btn    = new QPushButton(QString::fromStdString(cp->buttonText), wrap);
    const auto kSwatchName = QStringLiteral("M3UiColorSwatch");
    swatch->setObjectName(kSwatchName);
    swatch->setFrameShape(QFrame::StyledPanel);
    swatch->setFixedSize(44, 24);

    hlay->addWidget(swatch);
    hlay->addWidget(btn, 1);

    auto setSwatchFromU32 = [swatch, kSwatchName](uint32_t v) {
        const int  r   = int((v >> 24) & 0xFF);
        const int  g   = int((v >> 16) & 0xFF);
        const int  b   = int((v >> 8) & 0xFF);
        const int  a   = int(v & 0xFF);
        const double af = a / 255.0;
        swatch->setStyleSheet(
            QStringLiteral("QFrame#%1 { background-color: rgba(%2,%3,%4,%5); border: 1px solid #666; }")
                .arg(kSwatchName, QString::number(r), QString::number(g), QString::number(b), QString::number(af, 'f', 3)));
    };

    setSwatchFromU32(cp->color);

    QObject::connect(btn, &QPushButton::clicked, [setSwatchFromU32, btn, cp]() {
        const QColor start = u32RgbaToQColor(cp->color);
        const QColor pick  = QColorDialog::getColor(
            start, btn, QString(),
            QColorDialog::ColorDialogOptions(QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog));
        if (!pick.isValid())
            return;
        cp->color = qColorToU32Rgba(pick);
        setSwatchFromU32(cp->color);
        if (cp->onColorChanged)
            cp->onColorChanged(cp->color);
    });

    QPointer<QPushButton> bPtr = btn;
    cp->onChange = [bPtr, cp, setSwatchFromU32]() {
        if (bPtr)
            bPtr->setText(QString::fromStdString(cp->buttonText));
        setSwatchFromU32(cp->color);
    };

    return wrap;
}

///////////////////////////////////////////////////////////////
//  Canvas rendering
///////////////////////////////////////////////////////////////

class QtCanvasWidget : public QWidget {
    UiCanvas* canvasNode;
    QTimer* timer;
public:
    QtCanvasWidget(UiCanvas* node, QWidget* parent = nullptr) : QWidget(parent), canvasNode(node) {
        setMinimumSize(96, 96);
        setMaximumSize(480, 480);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMouseTracking(true); // Enable mouse move events without clicking
        
        // Setup a timer to constantly repaint the canvas
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            this->update();
        });
        timer->start(33); // ~30 fps
    }

    QSize sizeHint() const override { return QSize(280, 280); }

protected:
    QPoint m_mousePos = QPoint(-1, -1);

    void mouseMoveEvent(QMouseEvent* event) override {
        m_mousePos = event->pos();
        update();
    }

    void leaveEvent(QEvent* event) override {
        m_mousePos = QPoint(-1, -1);
        update();
    }

    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), Qt::black);

        if (!canvasNode) return;
        
        std::lock_guard<std::mutex> lock(*(canvasNode->pointsMutex));

        if (canvasNode->points.empty()) return;

        // Auto-scale to fit
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();

        for (const auto& pt : canvasNode->points) {
            if (pt.x < minX) minX = pt.x;
            if (pt.x > maxX) maxX = pt.x;
            if (pt.y < minY) minY = pt.y;
            if (pt.y > maxY) maxY = pt.y;
        }

        float rangeX = std::max(1.0f, maxX - minX);
        float rangeY = std::max(1.0f, maxY - minY);

        float margin = 20.0f;
        float drawW = width() - margin * 2;
        float drawH = height() - margin * 2;

        for (const auto& pt : canvasNode->points) {
            float nx = (pt.x - minX) / rangeX;
            float ny = (pt.y - minY) / rangeY;

            int px = static_cast<int>(margin + nx * drawW);
            int py = static_cast<int>(margin + ny * drawH);

            QColor color(
                (pt.color >> 24) & 0xFF,
                (pt.color >> 16) & 0xFF,
                (pt.color >> 8) & 0xFF,
                pt.color & 0xFF
            );
            
            bool isHovered = (std::abs(px - m_mousePos.x()) <= 6 && std::abs(py - m_mousePos.y()) <= 6);
            
            if (isHovered) {
                painter.setBrush(Qt::yellow);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPoint(px, py), static_cast<int>(pt.size) + 2, static_cast<int>(pt.size) + 2);
                
                if (!pt.label.empty()) {
                    painter.setPen(Qt::white);
                    painter.drawText(px + pt.size + 4, py + pt.size + 4, QString::fromStdString(pt.label));
                }
            } else {
                painter.setBrush(color);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPoint(px, py), static_cast<int>(pt.size), static_cast<int>(pt.size));
            }
        }
    }
};

QWidget* UiRenderer::renderCanvas(UiCanvas* canvas) {
    if (!canvas) return nullptr;
    // QVBoxLayout gives children the full tab width; without a row + side stretches the canvas
    // would stretch horizontally and widen the whole right dock. Center a bounded square instead.
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->addStretch(1);
    auto* cv = new QtCanvasWidget(canvas);
    h->addWidget(cv, 0, Qt::AlignCenter);
    h->addStretch(1);
    return row;
}

///////////////////////////////////////////////////////////////
//  ToolBox rendering
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderToolBox(UiToolBox* toolbox) {
    if (!toolbox) return nullptr;

    auto* tb = new QToolBox;
    tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    for (const auto& page : toolbox->pages) {
        QWidget* content = UiRenderer::renderContainer(page.content.get());
        if (content) {
            content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            tb->addItem(content, QString::fromStdString(page.title));
        }
    }
    new ToolBoxPageStretcher(tb);
    configureQtToolBoxPageScrollAreas(tb);

    QPointer<QToolBox> tbPtr = tb;
    toolbox->onChange = [tbPtr, toolbox]() {
        if (!tbPtr) return;
        while (tbPtr->count() > 0) {
            QWidget* w = tbPtr->widget(0);
            tbPtr->removeItem(0);
            if (w) w->deleteLater();
        }
        for (const auto& page : toolbox->pages) {
            QWidget* content = UiRenderer::renderContainer(page.content.get());
            if (content) {
                content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                tbPtr->addItem(content, QString::fromStdString(page.title));
            }
        }
        configureQtToolBoxPageScrollAreas(tbPtr);
        tbPtr->updateGeometry();
    };

    return tb;
}

///////////////////////////////////////////////////////////////
//  Root renderer into QTabWidget
///////////////////////////////////////////////////////////////

void UiRenderer::renderToTabWidget(std::shared_ptr<UiPage> root, QTabWidget* tabTarget) {
    if (!root || !tabTarget) return;

    QWidget* content = renderPage(root.get());
    if (content) {
        // Append in order; UiPage::getIndex() is often unset (0 for all) and must not be used as tab slot.
        const int idx = tabTarget->count();
        tabTarget->insertTab(idx, content, QString::fromStdString(root->getTitle()));
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
