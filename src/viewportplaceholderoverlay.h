#ifndef VIEWPORTPLACEHOLDEROVERLAY_H
#define VIEWPORTPLACEHOLDEROVERLAY_H

#include <QWidget>
#include <QTimer>
#include <QFont>

/// Плейсхолдер поверх вьюпорта
///
class ViewportPlaceholder final : public QWidget {
public:
    explicit ViewportPlaceholder(QWidget* parent = nullptr);

    void setHint(const QString& hint);
    void setAnimating(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void rebuildCachedFont();
    void refreshPaletteColors();

    QTimer  m_timer;
    float   m_angle = 0.f;
    QString m_hint;
    QFont   m_textFont;
    QColor  m_bgColor;
    QColor  m_fgColor;
    QColor  m_dimColor;
    bool    m_animating = false;
};

#endif // VIEWPORTPLACEHOLDEROVERLAY_H
