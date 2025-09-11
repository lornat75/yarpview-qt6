// Rewritten implementation with corrected auto-resize semantics, display modes, and status panels
#include "MainWindow.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QTimer>
#include <yarp/os/Network.h>
#include <yarp/os/LogStream.h>

MainWindow::MainWindow(const YarpViewOptions &opt, QWidget *parent) : QMainWindow(parent), options(opt) {
    buildUi();
    createMenus();
    // Apply compact/minimal modes (after menus created so actions exist for logic)
    if (options.compact || options.minimal) {
        if (menuBar()) menuBar()->hide();
        if (statusBar()) statusBar()->hide();
    }
    if (options.minimal) {
        // Frameless window for pure image display; title bar removed
        setWindowFlag(Qt::FramelessWindowHint, true);
    }
    // Ensure keep-above flag applied before initial show
    if (options.keepAbove) {
        Qt::WindowFlags f = windowFlags();
        f |= Qt::WindowStaysOnTopHint;
        setWindowFlags(f);
    }
    openPorts();
    receiver.open(options.imgInputPortName, true);
    connect(&receiver, &ImageReceiver::imageArrived, this, &MainWindow::onImage);
    if (!options.synch) {
        displayTimer = new QTimer(this);
        displayTimer->setInterval(options.refreshMs);
        connect(displayTimer, &QTimer::timeout, this, &MainWindow::displayTick);
        displayTimer->start();
    }
    // Apply requested geometry components (each flag independent)
    {
        // Start from current geometry / size hint
        QRect g = geometry();
        int defaultW = imageWidget ? imageWidget->sizeHint().width() : 320;
        int defaultH = imageWidget ? imageWidget->sizeHint().height() : 240;
        int x = options.hasX ? options.winX : g.x();
        int y = options.hasY ? options.winY : g.y();
        int w = options.hasW ? options.winW : (g.width() > 0 ? g.width() : defaultW);
        int h = options.hasH ? options.winH : (g.height() > 0 ? g.height() : defaultH);
        // Ensure positive dimensions
        if (w <= 0) w = defaultW;
        if (h <= 0) h = defaultH;
        // Only set if any geometry option present, otherwise fallback to default client size
        if (options.hasX || options.hasY || options.hasW || options.hasH) {
            setGeometry(x,y,w,h);
        } else {
                // Only apply default client size if neither width nor height was specified
                if (!(options.hasW || options.hasH)) {
                    setClientImageSize(320,240);
                }
        }
    }
    portTimer.start();
}

MainWindow::~MainWindow() {
    receiver.close();
    if (options.leftClickEnabled) leftClickPort.close();
    if (options.rightClickEnabled) rightClickPort.close();
}

void MainWindow::buildUi() {
    imageWidget = new ImageWidget(this);
    setCentralWidget(imageWidget);
    setWindowTitle(options.windowTitle);
    statusPortName = new QLabel(QString::fromStdString(options.imgInputPortName), this);
    statusPort = new QLabel("Port: -", this);
    statusDisplay = new QLabel("Display: -", this);
    statusPixelValue = new QLabel("Pixel: -", this);
    statusPixelPatch = new QLabel(this);
    statusPixelPatch->setFixedWidth(24); // will adjust height later
    statusPixelPatch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    QWidget *statusPanel = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(statusPanel);
    vbox->setContentsMargins(0,0,0,0);
    vbox->setSpacing(0);
    // Row 1: Port Name
    vbox->addWidget(statusPortName);
    // Row 2: Port stats
    vbox->addWidget(statusPort);
    // Row 3: Display stats
    vbox->addWidget(statusDisplay);
    // Row 4: Pixel value (label + inline color patch closely spaced)
    {
        QWidget *pixelRow = new QWidget(this);
        QHBoxLayout *ph = new QHBoxLayout(pixelRow);
        ph->setContentsMargins(0,0,0,0);
        ph->setSpacing(2); // tighter spacing so patch is close to text
        statusPixelValue->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        ph->addWidget(statusPixelValue);
        ph->addWidget(statusPixelPatch);
        ph->addStretch(); // push row contents left, patch remains adjacent to text
        statusPixelValue->setVisible(false);
        statusPixelPatch->setVisible(false);
        vbox->addWidget(pixelRow);
    }
    statusBar()->addPermanentWidget(statusPanel, 1);
    connect(imageWidget, &ImageWidget::displayFpsUpdated, this, &MainWindow::updateDisplayFps);
    connect(imageWidget, &ImageWidget::pixelClickedLeft, this, &MainWindow::onLeftClick);
    connect(imageWidget, &ImageWidget::pixelClickedRight, this, &MainWindow::onRightClick);
    connect(imageWidget, &ImageWidget::pixelHovered, this, [this](int x,int y,int r,int g,int b,int a){
        if (!statusPixelValue->isVisible()) return;
        // Text shows RGBA as requested
        QString hexText = QString("#%1%2%3%4")
            .arg(r,2,16,QChar('0'))
            .arg(g,2,16,QChar('0'))
            .arg(b,2,16,QChar('0'))
            .arg(a,2,16,QChar('0'))
            .toUpper();
        statusPixelValue->setText(QString("Pixel: %1 @ %2,%3").arg(hexText).arg(x).arg(y));
        // Style sheet expects #RRGGBB (or #AARRGGBB). Use RGB only to avoid channel order confusion.
        QString hexStyle = QString("#%1%2%3")
            .arg(r,2,16,QChar('0'))
            .arg(g,2,16,QChar('0'))
            .arg(b,2,16,QChar('0'))
            .toUpper();
        QString style = QString("background:%1; border:1px solid #444;").arg(hexStyle);
        statusPixelPatch->setStyleSheet(style);
        // Enforce square patch sized to row height
        int h = statusPixelValue->height();
        if (h>0) statusPixelPatch->setFixedSize(h,h);
    });
    if (options.keepAbove) setWindowFlag(Qt::WindowStaysOnTopHint, true);
    // Geometry now applied in constructor block with independent flags
}

void MainWindow::createMenus() {
    QMenu *fileMenu = menuBar()->addMenu("&File");
    actSaveSingle = new QAction("Save Single Image", this);
    actSaveSingle->setShortcut(QKeySequence::Save);
    connect(actSaveSingle, &QAction::triggered, this, &MainWindow::saveSingleImage);
    fileMenu->addAction(actSaveSingle);
    actSaveSet = new QAction("Save a set of images", this);
    connect(actSaveSet, &QAction::triggered, this, &MainWindow::saveImageSet);
    fileMenu->addAction(actSaveSet);

    QMenu *imageMenu = menuBar()->addMenu("&Image");
    actOriginalSize = new QAction("Original Size", this);
    actOriginalSize->setCheckable(true);
    connect(actOriginalSize, &QAction::triggered, this, &MainWindow::originalSize);
    imageMenu->addAction(actOriginalSize);
    actOriginalAspect = new QAction("Original Aspect Ratio", this);
    actOriginalAspect->setCheckable(true);
    connect(actOriginalAspect, &QAction::triggered, this, &MainWindow::originalAspectRatio);
    imageMenu->addAction(actOriginalAspect);
    imageMenu->addSeparator();
    actFreeze = new QAction("Freeze", this);
    actFreeze->setCheckable(true);
    connect(actFreeze, &QAction::triggered, this, &MainWindow::toggleFreeze);
    imageMenu->addAction(actFreeze);
    actSynch = new QAction("Synch Display", this);
    actSynch->setCheckable(true);
    actSynch->setChecked(options.synch);
    connect(actSynch, &QAction::triggered, this, &MainWindow::toggleSynch);
    imageMenu->addAction(actSynch);
    actAutoResize = new QAction("Auto Resize", this);
    actAutoResize->setCheckable(true);
    actAutoResize->setChecked(options.autosize);
    connect(actAutoResize, &QAction::triggered, this, &MainWindow::toggleAutoResize);
    imageMenu->addAction(actAutoResize);
    imageMenu->addSeparator();
    actDisplayPixelValue = new QAction("Display Pixel Value", this);
    actDisplayPixelValue->setCheckable(true);
        actDisplayPixelValue->setChecked(false);
    connect(actDisplayPixelValue, &QAction::triggered, this, &MainWindow::toggleDisplayPixelValue);
    imageMenu->addAction(actDisplayPixelValue);
    actChangeRefresh = new QAction("Change Refresh Interval...", this);
    connect(actChangeRefresh, &QAction::triggered, this, &MainWindow::changeRefreshInterval);
    imageMenu->addAction(actChangeRefresh);

    imageMenu->addSeparator();
    actKeepAbove = new QAction("Keep Above", this);
    actKeepAbove->setCheckable(true);
    actKeepAbove->setChecked(options.keepAbove);
    connect(actKeepAbove, &QAction::triggered, this, &MainWindow::toggleKeepAbove);
    imageMenu->addAction(actKeepAbove);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *actAbout = new QAction("About", this);
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(actAbout);
}

void MainWindow::openPorts() {
    if (options.leftClickEnabled) {
        if (!leftClickPort.open(options.leftClickOutPortName)) yError() << "Cannot open left click output port" << options.leftClickOutPortName;
    }
    if (options.rightClickEnabled) {
        if (!rightClickPort.open(options.rightClickOutPortName)) yError() << "Cannot open right click output port" << options.rightClickOutPortName;
    }
}

void MainWindow::onImage(const QImage &img, const yarp::os::Stamp &stamp) {
    Q_UNUSED(stamp);
    double ms = portTimer.restart();
    if (ms>0) {
        portIntervals.push_back(ms);
        if ((int)portIntervals.size()>PORT_WINDOW) portIntervals.pop_front();
    }
    bufferedImage = img;
    hasBufferedImage = true;
    lastImgW = img.width();
    lastImgH = img.height();
    if (lastImgW>0 && lastImgH>0) currentImageAspect = double(lastImgH)/double(lastImgW);
    if (options.synch) displayTick();
    if (savingImageSet && !imageSetDirectory.isEmpty()) {
        QString filename = QString("%1/image_%2.png").arg(imageSetDirectory).arg(imageSetCounter++, 6, 10, QChar('0'));
        img.save(filename);
    }
}

void MainWindow::onLeftClick(int x,int y) {
    if (!options.leftClickEnabled) return;
    auto &b = leftClickPort.prepare(); b.clear(); b.addInt32(x); b.addInt32(y); leftClickPort.write();
}
void MainWindow::onRightClick(int x,int y) {
    if (!options.rightClickEnabled) return;
    auto &b = rightClickPort.prepare(); b.clear(); b.addInt32(x); b.addInt32(y); rightClickPort.write();
}

void MainWindow::updateDisplayFps(double dispFps, double minFps, double maxFps) {
    auto avg = [](const std::deque<double> &d){ if (d.empty()) return 0.0; double s=0; for(double v: d) s+=v; return s/d.size(); };
    auto minv = [](const std::deque<double> &d){ if (d.empty()) return 0.0; return *std::min_element(d.begin(), d.end()); };
    auto maxv = [](const std::deque<double> &d){ if (d.empty()) return 0.0; return *std::max_element(d.begin(), d.end()); };
    double avgMs = avg(portIntervals);
    double portHz = avgMs>0 ? 1000.0/avgMs : 0;
    double minHz = maxv(portIntervals)>0 ? 1000.0/maxv(portIntervals) : 0;
    double maxHz = minv(portIntervals)>0 ? 1000.0/minv(portIntervals) : 0;
    int imgW = lastImgW>0? lastImgW:0;
    int imgH = lastImgH>0? lastImgH:0;
    statusPort->setText(QString("Port: %1 (%2..%3) Hz (size: %4x%5)")
                        .arg(portHz,0,'f',1).arg(minHz,0,'f',1).arg(maxHz,0,'f',1)
                        .arg(imgW).arg(imgH));
    // Client image area size (central widget / image widget)
    int cw = imageWidget ? imageWidget->width() : 0;
    int ch = imageWidget ? imageWidget->height() : 0;
    statusDisplay->setText(QString("Display: %1 (%2..%3) Hz (size: %4x%5)")
                           .arg(dispFps,0,'f',1).arg(minFps,0,'f',1).arg(maxFps,0,'f',1)
                           .arg(cw).arg(ch));
}

void MainWindow::saveSingleImage() {
    QString fn = QFileDialog::getSaveFileName(this, "Save Single Image", QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")+".png", "Images (*.png *.jpg *.bmp)");
    if (fn.isEmpty()) return;
    if (hasBufferedImage) bufferedImage.save(fn);
}
void MainWindow::saveImageSet() {
    if (!savingImageSet) {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Directory for Image Set");
        if (dir.isEmpty()) return; imageSetDirectory = dir; imageSetCounter=0; savingImageSet=true; actSaveSet->setText("Stop saving image set");
    } else { savingImageSet=false; actSaveSet->setText("Save a set of images"); }
}

void MainWindow::originalSize() { currentMode = DisplayMode::OriginalSize; actOriginalAspect->setChecked(false); displayTick(); }
void MainWindow::originalAspectRatio() { currentMode = DisplayMode::AspectRatio; actOriginalSize->setChecked(false); if (lastImgW>0&& lastImgH>0) aspectRatio = double(lastImgH)/double(lastImgW); displayTick(); }

// New helper functions to allow reversible size behavior
void MainWindow::applyStretchMode() {
    currentMode = DisplayMode::StretchToWindow;
    actOriginalSize->setChecked(false);
    actOriginalAspect->setChecked(false);
    imageWidget->setMode(DisplayMode::StretchToWindow);
    imageWidget->update();
}
void MainWindow::applyOriginalSizeMode() {
    currentMode = DisplayMode::OriginalSize;
    actOriginalSize->setChecked(true);
    actOriginalAspect->setChecked(false);
    imageWidget->setMode(DisplayMode::OriginalSize);
    if (hasBufferedImage) setClientImageSize(bufferedImage.width(), bufferedImage.height());
    imageWidget->update();
}
void MainWindow::applyAspectRatioMode() {
    currentMode = DisplayMode::AspectRatio;
    actOriginalAspect->setChecked(true);
    actOriginalSize->setChecked(false);
    imageWidget->setMode(DisplayMode::AspectRatio);
    // Force window client area aspect to match image aspect while keeping current width (or height if width unavailable)
    if (hasBufferedImage && currentImageAspect>0.0) {
        // Choose a target based on current client width
        int clientW = imageWidget->width();
        if (clientW <= 0) clientW = bufferedImage.width();
        int targetH = int(std::round(clientW * currentImageAspect));
        // Adjust client size exactly
        setClientImageSize(clientW, targetH);
    }
    imageWidget->update();
}
void MainWindow::toggleFreeze() { bool frz=!receiver.isFrozen(); receiver.setFrozen(frz); actFreeze->setChecked(frz); actFreeze->setText(frz?"Unfreeze":"Freeze"); }
void MainWindow::toggleSynch() { options.synch=!options.synch; actSynch->setChecked(options.synch); if (options.synch){ if (displayTimer) displayTimer->stop(); displayTick(); } else { if (!displayTimer){ displayTimer=new QTimer(this); connect(displayTimer,&QTimer::timeout,this,&MainWindow::displayTick);} displayTimer->setInterval(options.refreshMs); displayTimer->start(); } }
void MainWindow::toggleAutoResize() { options.autosize=!options.autosize; actAutoResize->setChecked(options.autosize); if (options.autosize) currentMode=DisplayMode::OriginalSize; displayTick(); }
void MainWindow::toggleDisplayPixelValue() { 
    bool vis = actDisplayPixelValue->isChecked();
    statusPixelValue->setVisible(vis);
    if (statusPixelPatch) statusPixelPatch->setVisible(vis);
}
void MainWindow::toggleKeepAbove() {
    bool want = actKeepAbove->isChecked();
    options.keepAbove = want;
    QRect g = geometry();
    Qt::WindowFlags f = windowFlags();
    if (want) f |= Qt::WindowStaysOnTopHint; else f &= ~Qt::WindowStaysOnTopHint;
    bool vis = isVisible();
    setWindowFlags(f);
    if (vis) {
        show();
        setGeometry(g);
        if (options.hasX || options.hasY) {
            int newX = options.hasX ? options.winX : g.x();
            int newY = options.hasY ? options.winY : g.y();
            move(newX,newY);
        }
        raise();
        activateWindow();
        // Schedule a couple of delayed raises (some WMs reorder shortly after flag change)
        QTimer::singleShot(50, this, [this]{ if (options.keepAbove) { raise(); activateWindow(); } });
        QTimer::singleShot(300, this, [this]{ if (options.keepAbove) { raise(); } });
    }
}
// Ensure patch toggles with label
// (placed after function definition for brevity)
void MainWindow::changeRefreshInterval() { bool ok=false; int v=QInputDialog::getInt(this,"Change Refresh Interval","Refresh period (ms):",options.refreshMs,1,10000,1,&ok); if (ok){ options.refreshMs=v; if (!options.synch && displayTimer) displayTimer->setInterval(v);} }
void MainWindow::showAbout() { QMessageBox::about(this, "About yarpview-qt6-gpt5", "yarpview-qt6-gpt5\nQt6 Widgets YARP image viewer"); }

void MainWindow::displayTick() {
    if (!hasBufferedImage) return;
    // Determine effective mode based on actions (reversible logic)
    if (actOriginalSize->isChecked()) {
        applyOriginalSizeMode();
    } else if (actOriginalAspect->isChecked()) {
        applyAspectRatioMode();
    } else {
        applyStretchMode();
    }
    imageWidget->setSourceImage(bufferedImage);
}

void MainWindow::setClientImageSize(int w,int h){ if (!centralWidget()) return; int frameW=width()-centralWidget()->width(); int frameH=height()-centralWidget()->height(); frameW=std::max(frameW,0); frameH=std::max(frameH,0); resize(w+frameW,h+frameH); }

void MainWindow::resizeEvent(QResizeEvent *e){ 
    QMainWindow::resizeEvent(e);
    if (!hasBufferedImage) return;
    if (currentMode==DisplayMode::AspectRatio && currentImageAspect>0.0) {
        // Enforce aspect after user resize: keep width, adjust height
        int clientW = imageWidget->width();
        int desiredH = int(std::round(clientW * currentImageAspect));
        if (imageWidget->height()!=desiredH) {
            setClientImageSize(clientW, desiredH);
        }
    }
    if (currentMode!=DisplayMode::OriginalSize) imageWidget->update(); 
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    if (options.minimal && e->key()==Qt::Key_Escape) {
        close();
        return;
    }
    QMainWindow::keyPressEvent(e);
}

void MainWindow::showEvent(QShowEvent *e) {
    QMainWindow::showEvent(e);
    if (!initialPosApplied && (options.hasX || options.hasY)) {
        QPoint current = pos();
        int newX = options.hasX ? options.winX : current.x();
        int newY = options.hasY ? options.winY : current.y();
        auto applyPos = [this,newX,newY]{
            if (!(options.hasX || options.hasY)) return;
            move(newX,newY);
        };
        // Immediate + several delayed retries (handles some Wayland/X11 race conditions)
        applyPos();
        const int delays[] = {0, 30, 120, 300};
        for (int d: delays) {
            QTimer::singleShot(d, this, applyPos);
        }
        // Final check & warn once if not honored
        QTimer::singleShot(350, this, [this,newX,newY]{
            if (pos().x()!=newX || pos().y()!=newY) {
                static bool warned=false; if (!warned) {
                    warned=true;
                    yWarning() << "Window manager did not honor --x/--y (platform: "
                               << QGuiApplication::platformName().toStdString()
                               << ")";
                }
            }
        });
        initialPosApplied = true;
    }
}
