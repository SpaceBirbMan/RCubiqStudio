#include "uirenderer.h"
#include <iostream>

///////////////////////////////////////////////////////////////
//  helpers
///////////////////////////////////////////////////////////////

static QBoxLayout* makeLayout(GroupSortType sort) {
    return (sort == VBox)
    ? static_cast<QBoxLayout*>(new QVBoxLayout)
    : static_cast<QBoxLayout*>(new QHBoxLayout);
}

///////////////////////////////////////////////////////////////
//  Render dispatcher
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderElement(UiElement* elem) {

    std::cout << "renderElement: " << typeid(*elem).name() << std::endl;

    if (auto g = dynamic_cast<UiGroup*>(elem))
        return renderGroup(g);

    if (auto pg = dynamic_cast<UiPage*>(elem))
        return renderPage(pg);

    if (auto c = dynamic_cast<UiContainer*>(elem))
        return renderContainer(c);

    if (auto t = dynamic_cast<UiTitle*>(elem)) {
        QLabel* l = new QLabel(QString::fromStdString(t->text));
        QFont f = l->font();
        if (t->format == Italic) f.setItalic(true);
        if (t->format == Bold) f.setBold(true);
        if (t->format == Underline) f.setUnderline(true);
        l->setFont(f);
        return l;
    }

    if (auto b = dynamic_cast<UiButton*>(elem)) {
        QPushButton* btn = new QPushButton(QString::fromStdString(b->text));
        if (b->onClick) {
            QObject::connect(btn, &QPushButton::clicked,
                             [cb = b->onClick]() { cb(); });
        }
        return btn;
    }

    if (auto tb = dynamic_cast<UiToggleableButton*>(elem)) {
        QPushButton* btn = new QPushButton(QString::fromStdString(tb->text));
        btn->setCheckable(true);
        btn->setChecked(tb->active);

        if (tb->onToggle) {
            QObject::connect(btn, &QPushButton::toggled,
                             [cb = tb->onToggle](bool v) { cb(v); });
        }
        return btn;
    }

    if (auto cb = dynamic_cast<UiCheckBox*>(elem)) {
        QCheckBox* ch = new QCheckBox(QString::fromStdString(cb->text));
        ch->setChecked(cb->active);
        if (cb->onToggle)
            QObject::connect(ch, &QCheckBox::toggled,
                             [cbfn = cb->onToggle](bool v){ cbfn(v); });
        return ch;
    }

    if (auto in = dynamic_cast<UiInputField*>(elem)) {
        QLineEdit* le = new QLineEdit;
        le->setPlaceholderText(QString::fromStdString(in->hint));
        le->setText(QString::fromStdString(in->value));
        if (in->inputType == HIDDENTEXT) le->setEchoMode(QLineEdit::Password);

        if (in->onTextChanged) {
            QObject::connect(le, &QLineEdit::textChanged,
                             [cb = in->onTextChanged](const QString& v){
                                 cb(v.toStdString());
                             });
        }
        return le;
    }

    if (auto sp = dynamic_cast<UiSpinField*>(elem)) {

        if (!sp->isFloat) {
            QSpinBox* sb = new QSpinBox;
            sb->setRange(sp->minValue, sp->maxValue);
            sb->setValue(sp->intValue);

            if (sp->onValueChanged)
                QObject::connect(sb, &QSpinBox::valueChanged,
                                 [cb = sp->onValueChanged](int v){ cb(v); });

            return sb;
        } else {
            QDoubleSpinBox* ds = new QDoubleSpinBox;
            ds->setRange(sp->minValue, sp->maxValue);
            ds->setValue(sp->intValue);

            if (sp->onValueChanged)
                QObject::connect(ds, &QDoubleSpinBox::valueChanged,
                                 [cb = sp->onValueChanged](double v){ cb((int)v); });

            return ds;
        }
    }

    if (auto sl = dynamic_cast<UiSlider*>(elem)) {
        QSlider* s = new QSlider(Qt::Horizontal);
        s->setRange(sl->minValue, sl->maxValue);
        s->setValue(sl->value);

        if (sl->onSlide)
            QObject::connect(s, &QSlider::valueChanged,
                             [cb = sl->onSlide](int v){ cb(v); });

        return s;
    }

    if (auto dl = dynamic_cast<UiDial*>(elem)) {
        QDial* d = new QDial;
        d->setRange(dl->minValue, dl->maxValue);
        d->setValue(dl->value);

        if (dl->onSlide) {
            QObject::connect(d, &QDial::valueChanged,
                             [cb = dl->onSlide](int v){ cb(v); });
        }

        return d;
    }

    if (auto img = dynamic_cast<UiImageBox*>(elem)) {
        return renderImageBox(img);
    }

    if (auto pb = dynamic_cast<UiProgressBar*>(elem)) {
        QProgressBar* p = new QProgressBar;
        p->setRange(pb->minValue, pb->maxValue);
        p->setValue(pb->value);
        // ориентации можно добавить позже
        return p;
    }

    if (auto cbx = dynamic_cast<UiComboBox*>(elem)) {
        QComboBox* box = new QComboBox;
        for (const auto& it : cbx->items)
            box->addItem(QString::fromStdString(it));

        box->setCurrentIndex(cbx->currentIndex);

        if (cbx->onSelect) {
            QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged),
                             [cb = cbx->onSelect](int v){ cb(v); });
        }
        return box;
    }

    if (auto tv = dynamic_cast<UiTreeView*>(elem)) {

    }

    return new QWidget;  // fallback
}

///////////////////////////////////////////////////////////////
//  Containers
///////////////////////////////////////////////////////////////

QWidget* UiRenderer::renderTree(UiTreeView* tree) {
    QWidget* tr = new QWidget;

}

QWidget* UiRenderer::renderImageBox(UiImageBox* imgBox) {
    QPushButton* btn = new QPushButton;
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    btn->setMinimumSize(64, 64);
    btn->setStyleSheet("QPushButton { border: 1px dashed gray; }");

    auto updateButton = [btn, imgBox]() {
        if (imgBox->hasImage && !imgBox->imagePath.empty()) {
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
            btn->window(),
            "Select Image",
            "",
            "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"
            );

        if (!filePath.isEmpty()) {
            imgBox->setImage(filePath.toStdString());
            updateButton(); // обновляем вид
        }
    });

    return btn;
}

QWidget* UiRenderer::renderContainer(UiContainer* container) { // тут именно вертикальный, TODO: расширить поддержку ориентаций расположения
    QWidget* w = new QWidget;
    auto lay = new QVBoxLayout;

    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    w->setLayout(lay);

    for (auto& ch : container->getChildren()) {
        QWidget* child = renderElement(ch.get());
        lay->addWidget(child);
    }
    lay->addStretch();

    return w;
}

QWidget* UiRenderer::renderGroup(UiGroup* group) {
    QWidget* w = new QWidget;
    auto lay = makeLayout(group->sort);
    w->setLayout(lay);

    for (auto& ch : group->getChildren())
        lay->addWidget(renderElement(ch.get()));

    return w;
}

QWidget* UiRenderer::renderPage(UiPage* page) {

    std::cout << "renderPage CALLED" << std::endl;
    return renderContainer(page);
}

///////////////////////////////////////////////////////////////
//  Root renderer into QTabWidget
///////////////////////////////////////////////////////////////

void UiRenderer::renderToTabWidget(std::shared_ptr<UiPage> root, QTabWidget* tabTarget) {
    //tabTarget->clear();

    // for (auto& ch : root->children) { todo: смысл этого - передавать страницы в контейнере, чтобы не делать это много раз по отдельности
    //     if (auto page = dynamic_cast<UiPage*>(ch.get())) {
    //         QWidget* content = renderPage(page);
    //         tabTarget->insertTab(page->index, content,
    //                              QString::fromStdString(page->title));
    //     }
    // }

    // ВКЛАДКИ МОЖНО ЗАПУСТИТЬ ОТДЕЛЬНЫМ ОКНОМ!!! ЛОЛ

     QWidget* content = renderPage(root.get());
     tabTarget->insertTab(root->index, content, QString::fromStdString(root->title));
}
