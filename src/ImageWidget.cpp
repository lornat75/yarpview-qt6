#include "ImageWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

ImageWidget::ImageWidget(QWidget *parent) : QWidget(parent) {
    dispTimer.start();
    setMouseTracking(true);
}

void ImageWidget::setAutoResize(bool on) { autoResize = on; }

void ImageWidget::setSourceImage(const QImage &img) {
    if (img.isNull()) return;
    source = img;
    if (mode==DisplayMode::OriginalSize && autoResize) {
        resize(source.size());
    }
    update();
}

void ImageWidget::setMode(DisplayMode m) {
    mode = m;
    update();
}

void ImageWidget::resetZoom() { update(); }

void ImageWidget::paintEvent(QPaintEvent *) {
    if (source.isNull()) return;
    double dispMs = dispTimer.restart();
    if (dispMs>0) {
        dispIntervals.push_back(dispMs);
        if ((int)dispIntervals.size()>FPS_WINDOW) dispIntervals.pop_front();
    }
    QSize avail = size();
    QSize drawSize;
    QPoint topLeft(0,0);
    if (mode==DisplayMode::OriginalSize) {
        drawSize = source.size();
        // center if window bigger
        if (avail.width() > drawSize.width()) topLeft.setX((avail.width()-drawSize.width())/2);
        if (avail.height() > drawSize.height()) topLeft.setY((avail.height()-drawSize.height())/2);
    } else if (mode==DisplayMode::AspectRatio) {
        if (source.width()>0 && source.height()>0) {
            double aspect = double(source.height())/double(source.width());
            int w = avail.width();
            int h = int(std::round(w * aspect));
            if (h > avail.height()) {
                h = avail.height();
                w = int(std::round(h / aspect));
            }
            drawSize = QSize(w,h);
            if (w < avail.width()) topLeft.setX((avail.width()-w)/2);
            if (h < avail.height()) topLeft.setY((avail.height()-h)/2);
        } else {
            drawSize = source.size();
        }
    } else { // StretchToWindow (fill entire widget, no letterbox)
        drawSize = avail;
    }
    lastDrawRect_ = QRect(topLeft, drawSize);
    QPainter p(this);
    if (mode!=DisplayMode::StretchToWindow && (lastDrawRect_.width() < avail.width() || lastDrawRect_.height() < avail.height())) {
        p.fillRect(rect(), Qt::black); // letterbox background
    }
    p.drawImage(lastDrawRect_, source);
    auto avg = [](const std::deque<double> &d){ if (d.empty()) return 0.0; double s=0; for(double v: d) s+=v; return s/d.size(); };
    auto minv = [](const std::deque<double> &d){ if (d.empty()) return 0.0; return *std::min_element(d.begin(), d.end()); };
    auto maxv = [](const std::deque<double> &d){ if (d.empty()) return 0.0; return *std::max_element(d.begin(), d.end()); };
    double dispFps = avg(dispIntervals)>0 ? 1000.0/avg(dispIntervals) : 0;
    double minFps = maxv(dispIntervals)>0 ? 1000.0/maxv(dispIntervals) : 0;
    double maxFps = minv(dispIntervals)>0 ? 1000.0/minv(dispIntervals) : 0;
    emit displayFpsUpdated(dispFps, minFps, maxFps);
}

void ImageWidget::mousePressEvent(QMouseEvent *e) {
    if (source.isNull()) return;
    int x,y; if (!widgetToImage(e->pos(), x,y)) return;
    if (e->button()==Qt::LeftButton) emit pixelClickedLeft(x,y);
    else if (e->button()==Qt::RightButton) emit pixelClickedRight(x,y);
}

void ImageWidget::wheelEvent(QWheelEvent *e) { Q_UNUSED(e); }

void ImageWidget::mouseMoveEvent(QMouseEvent *e) {
    if (source.isNull()) return;
    int x,y; if (!widgetToImage(e->pos(), x,y)) return; 
    QColor c = QColor::fromRgba(source.pixel(x,y));
    emit pixelHovered(x,y,c.red(),c.green(),c.blue(),c.alpha());
}

bool ImageWidget::widgetToImage(const QPoint &wpt, int &ix, int &iy) const {
    if (source.isNull()) return false;
    if (!lastDrawRect_.contains(wpt)) return false;
    int sx = source.width();
    int sy = source.height();
    if (mode==DisplayMode::OriginalSize) {
        ix = wpt.x() - lastDrawRect_.x();
        iy = wpt.y() - lastDrawRect_.y();
        if (ix<0||iy<0||ix>=sx||iy>=sy) return false;
        return true;
    }
    if (mode==DisplayMode::StretchToWindow) {
        double rx = double(wpt.x()) / double(lastDrawRect_.width());
        double ry = double(wpt.y()) / double(lastDrawRect_.height());
        ix = std::clamp(int(rx * sx), 0, sx-1);
        iy = std::clamp(int(ry * sy), 0, sy-1);
        return true;
    }
    if (mode==DisplayMode::AspectRatio) {
        double rx = double(wpt.x() - lastDrawRect_.x()) / double(lastDrawRect_.width());
        double ry = double(wpt.y() - lastDrawRect_.y()) / double(lastDrawRect_.height());
        ix = std::clamp(int(rx * sx), 0, sx-1);
        iy = std::clamp(int(ry * sy), 0, sy-1);
        return true;
    }
    return false;
}
