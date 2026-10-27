#include "viewportplaceholderoverlay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>

ViewportPlaceholder::ViewportPlaceholder(QWidget* parent)
    : QWidget(parent)
    , m_hint(QObject::tr("placeholder_hint"))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!m_animating || !isVisible())
            return;
        m_angle += 2.5f;
        if (m_angle >= 360.f)
            m_angle -= 360.f;
        update();
    });

    refreshPaletteColors();
    rebuildCachedFont();
}

void ViewportPlaceholder::setAnimating(bool enabled)
{
    m_animating = enabled;
    if (enabled && isVisible())
        m_timer.start();
    else
        m_timer.stop();
}

void ViewportPlaceholder::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshPaletteColors();
    rebuildCachedFont();
    if (m_animating)
        m_timer.start();
}

void ViewportPlaceholder::hideEvent(QHideEvent* event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}

void ViewportPlaceholder::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildCachedFont();
}

void ViewportPlaceholder::refreshPaletteColors()
{
    m_bgColor = palette().color(QPalette::Window);
    m_fgColor = palette().color(QPalette::WindowText);
    m_dimColor = QColor(m_fgColor.red(), m_fgColor.green(), m_fgColor.blue(), 60);
}

void ViewportPlaceholder::rebuildCachedFont()
{
    const int w = width();
    const int h = height();
    const int side = (w > 4 && h > 4) ? qMax(16, qMin(w, h) / 10) : 16;
    m_textFont = font();
    m_textFont.setPixelSize(qMax(11, side * 4 / 10));
}

void ViewportPlaceholder::paintEvent(QPaintEvent*)
{
    if (!isVisible())
        return;

    const int w = width();
    const int h = height();
    if (w < 4 || h < 4)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), m_bgColor);

    const int cx   = w / 2;
    const int cy   = h / 2;
    const int side = qMax(16, qMin(w, h) / 10);
    const int gap  = side * 3 / 4;
    const int squareCy = cy - gap;

    p.save();
    p.translate(cx, squareCy);
    p.rotate(static_cast<double>(m_angle));
    QPen pen(m_dimColor, 2.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(-side / 2, -side / 2, side, side);
    p.restore();

    if (m_hint.isEmpty())
        return;

    p.setFont(m_textFont);
    p.setPen(QColor(m_fgColor.red(), m_fgColor.green(), m_fgColor.blue(), 160));

    const int textTop = squareCy + side / 2 + gap / 2;
    const QRect textRect(8, textTop, w - 16, h - textTop - 8);
    if (textRect.height() > 0)
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_hint);
}

void ViewportPlaceholder::setHint(const QString& hint)
{
    if (m_hint == hint)
        return;
    m_hint = hint;
    if (isVisible())
        update();
}
