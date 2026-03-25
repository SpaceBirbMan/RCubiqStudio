#include "trackerrenderer.h"
#include <QPainter>

TrackerRenderer::TrackerRenderer(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void TrackerRenderer::setPoints(const std::vector<QPointF>& points) {
    _points = points;
    update();
}

void TrackerRenderer::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dark background for better contrast
    painter.fillRect(rect(), QColor(25, 25, 25));

    if (_points.empty()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No Tracking Data");
        return;
    }

    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
    
    std::vector<QPointF> processedPoints;
    processedPoints.reserve(_points.size());

    for (const auto& p : _points) {

        float rx = p.x();
        float ry = p.y();
        processedPoints.push_back(QPointF(rx, ry));

        if (rx < minX) minX = rx;
        if (rx > maxX) maxX = rx;
        if (ry < minY) minY = ry;
        if (ry > maxY) maxY = ry;
    }

    if (minX >= maxX || minY >= maxY) return;

    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    
    float widgetW = width() - 20.0f;
    float widgetH = height() - 20.0f;
    
    float scaleX = widgetW / rangeX;
    float scaleY = widgetH / rangeY;
    float scale = std::min(scaleX, scaleY);
    
    float offsetX = (width() - rangeX * scale) / 2.0f;
    float offsetY = (height() - rangeY * scale) / 2.0f;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 255, 150)); // Nice bright green for dots

    for (const auto& p : processedPoints) {
        float px = offsetX + (p.x() - minX) * scale;
        float py = offsetY + (p.y() - minY) * scale;
        painter.drawEllipse(QPointF(px, py), 2.5, 2.5);
    }
}
