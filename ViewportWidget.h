#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPainter>

class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr) : QWidget(parent) {}

    void setImage(const QImage& img) {
        image = img;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        if (!image.isNull()) {
            painter.drawImage(rect(), image, image.rect());
        } else {
            painter.fillRect(rect(), Qt::black);
        }
    }

private:
    QImage image;
};

#endif // VIEWPORTWIDGET_H
