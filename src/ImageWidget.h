#pragma once
#include <QWidget>
#include <QImage>
#include <QElapsedTimer>
#include <deque>

enum class DisplayMode { StretchToWindow, OriginalSize, AspectRatio };

class ImageWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent=nullptr);

    QSize sizeHint() const override { return QSize(320,240); }

public slots:
    void setSourceImage(const QImage &img);
    void setMode(DisplayMode m);
    void setAutoResize(bool on); // when true, window (outside) will be resized externally, here just note flag
    void resetZoom(); // deprecated (kept for compatibility)

signals:
    void displayFpsUpdated(double avgFps, double minFps, double maxFps);
    void pixelClickedLeft(int x,int y);
    void pixelClickedRight(int x,int y);
    void pixelHovered(int x,int y,int r,int g,int b,int a);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    QImage source; // original image
    bool autoResize=false;
    DisplayMode mode{DisplayMode::StretchToWindow};
    QRect lastDrawRect_; // where the image was actually drawn inside the widget

    // fps stats (display only)
    QElapsedTimer dispTimer; // time since last paint
    std::deque<double> dispIntervals;
    static constexpr int FPS_WINDOW = 120;

    // Map widget coordinates to image coordinates based on lastDrawRect_
    bool widgetToImage(const QPoint &wpt, int &ix, int &iy) const;
};
