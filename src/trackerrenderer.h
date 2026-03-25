#ifndef TRACKERRENDERER_H
#define TRACKERRENDERER_H

#include <QWidget>
#include <vector>
#include <QPointF>

class TrackerRenderer : public QWidget
{
    Q_OBJECT
public:
    explicit TrackerRenderer(QWidget *parent = nullptr);
    void setPoints(const std::vector<QPointF>& points);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<QPointF> _points;
};

#endif // TRACKERRENDERER_H
