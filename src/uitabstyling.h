#ifndef UITABSTYLING_H
#define UITABSTYLING_H

#include <QAbstractButton>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBox>
#include <QVariant>
#include <algorithm>

#include "rcqapi.h"
/// По-моему это стили для Widget-окна
enum class UiTabCategory {
    App,
    Generic,
    Tracker,
    Engine,
};

inline int uiBlendChannel(int base, int tint, float mix) {
    return qBound(0, int(base + (tint - base) * mix), 255);
}

inline QColor uiBlendColors(const QColor& base, const QColor& tint, float mix) {
    return QColor(
        uiBlendChannel(base.red(), tint.red(), mix),
        uiBlendChannel(base.green(), tint.green(), mix),
        uiBlendChannel(base.blue(), tint.blue(), mix));
}

inline bool uiPaletteIsDark(const QPalette& pal) {
    return pal.color(QPalette::Window).lightness() < 128;
}

/// Акцент категории — смешивается с QPalette::Button основной темы.
inline QColor uiTabCategoryAccent(UiTabCategory cat, bool darkTheme) {
    if (darkTheme) {
        switch (cat) {
        case UiTabCategory::App:     return QColor(62, 88, 118);  // бледно-синий
        case UiTabCategory::Generic: return QColor(108, 98, 58);  // бледно-жёлтый
        case UiTabCategory::Tracker: return QColor(118, 68, 68);  // бледно-красный
        case UiTabCategory::Engine:  return QColor(68, 108, 68);  // бледно-зелёный
        }
    } else {
        switch (cat) {
        case UiTabCategory::App:     return QColor(198, 218, 245);
        case UiTabCategory::Generic: return QColor(248, 238, 188);
        case UiTabCategory::Tracker: return QColor(248, 208, 208);
        case UiTabCategory::Engine:  return QColor(198, 232, 198);
        }
    }
    return {};
}

inline QColor uiTabCategoryColor(UiTabCategory cat, const QPalette& pal, bool selected = false) {
    const bool dark = uiPaletteIsDark(pal);
    const QColor themeBtn = pal.color(QPalette::Button);
    const float mix = dark ? 0.38f : 0.55f;
    QColor base = uiBlendColors(themeBtn, uiTabCategoryAccent(cat, dark), mix);
    if (selected) {
        const QColor hi = pal.color(QPalette::Highlight);
        base = uiBlendColors(base, hi, dark ? 0.22f : 0.18f);
    } else if (dark) {
        base = base.darker(104);
    }
    return base;
}

inline UiTabCategory pluginUiTypeToTabCategory(PluginUIType type) {
    switch (type) {
    case PluginUIType::Engine:  return UiTabCategory::Engine;
    case PluginUIType::Tracker: return UiTabCategory::Tracker;
    case PluginUIType::Generic: return UiTabCategory::Generic;
    }
    return UiTabCategory::App;
}

inline void setTabCategory(QTabWidget* tabs, int index, UiTabCategory cat) {
    if (!tabs || index < 0 || index >= tabs->count())
        return;
    tabs->tabBar()->setTabData(index, static_cast<int>(cat));
    tabs->tabBar()->update();
}

class CategoryTabBar : public QTabBar {
public:
    explicit CategoryTabBar(QWidget* parent = nullptr) : QTabBar(parent) {
        connect(this, &QTabBar::currentChanged, this, qOverload<>(&QWidget::update));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStylePainter painter(this);
        const QPalette pal = palette();
        const QColor text = pal.color(QPalette::ButtonText);
        for (int i = 0; i < count(); ++i) {
            QStyleOptionTab opt;
            initStyleOption(&opt, i);
            const QVariant data = tabData(i);
            if (data.isValid()) {
                const auto cat = static_cast<UiTabCategory>(data.toInt());
                const QColor bg = uiTabCategoryColor(cat, pal, i == currentIndex());
                opt.palette.setBrush(QPalette::Button, bg);
                opt.palette.setBrush(QPalette::Window, bg);
                opt.palette.setBrush(QPalette::Midlight, bg);
                opt.palette.setColor(QPalette::ButtonText, text);
                opt.palette.setColor(QPalette::WindowText, text);
            }
            painter.drawControl(QStyle::CE_TabBarTab, opt);
        }
    }
};

/// QTabWidget with CategoryTabBar; setTabBar() is protected in Qt, so install from subclass.
class CategoryTabWidget : public QTabWidget {
public:
    explicit CategoryTabWidget(QWidget* parent = nullptr) : QTabWidget(parent) {
        setTabBar(new CategoryTabBar(this));
    }
};

inline QList<QAbstractButton*> toolBoxTabButtons(QToolBox* box) {
    QList<QAbstractButton*> buttons;
    if (!box)
        return buttons;
    for (auto* child : box->findChildren<QAbstractButton*>()) {
        if (child->parentWidget() == box)
            buttons.append(child);
    }
    std::sort(buttons.begin(), buttons.end(), [](QAbstractButton* a, QAbstractButton* b) {
        return a->geometry().top() < b->geometry().top();
    });
    return buttons;
}

inline void styleToolBoxTabButton(QToolBox* box, int index, UiTabCategory cat) {
    const auto buttons = toolBoxTabButtons(box);
    if (index < 0 || index >= buttons.size())
        return;
    auto* btn = buttons[index];
    const QPalette& pal = box->palette();
    const QColor idle = uiTabCategoryColor(cat, pal, false);
    const QColor active = uiTabCategoryColor(cat, pal, true);
    const QColor text = pal.color(QPalette::ButtonText);
    const QColor border = pal.color(QPalette::Mid);
    btn->setStyleSheet(QString(
        "QAbstractButton { background-color: %1; color: %2; border: 1px solid %3; "
        "border-radius: 2px; padding: 4px 6px; text-align: left; }"
        "QAbstractButton:checked { background-color: %4; font-weight: bold; }"
    ).arg(idle.name(), text.name(), border.name(), active.name()));
}

#endif // UITABSTYLING_H
