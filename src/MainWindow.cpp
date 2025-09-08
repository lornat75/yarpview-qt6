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
#include <yarp/os/Network.h>
#include <yarp/os/LogStream.h>

MainWindow::MainWindow(const YarpViewOptions &opt, QWidget *parent) : QMainWindow(parent), options(opt) {
    buildUi();
    createMenus();
    openPorts();
    receiver.open(options.imgInputPortName, true);
    connect(&receiver, &ImageReceiver::imageArrived, this, &MainWindow::onImage);
    if (!options.synch) {
        displayTimer = new QTimer(this);
        displayTimer->setInterval(options.refreshMs);
        connect(displayTimer, &QTimer::timeout, this, &MainWindow::displayTick);
        displayTimer->start();
    }
    if (!options.explicitGeometry) setClientImageSize(320,240);
    portTimer.start();
}

MainWindow::~MainWindow() {
    receiver.close();
    leftClickPort.close();
    rightClickPort.close();
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
    if (options.explicitGeometry) setGeometry(options.winX, options.winY, options.winW, options.winH);
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

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *actAbout = new QAction("About", this);
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(actAbout);
}

void MainWindow::openPorts() {
    if (!leftClickPort.open(options.leftClickOutPortName)) yError() << "Cannot open left click output port" << options.leftClickOutPortName;
    if (!rightClickPort.open(options.rightClickOutPortName)) yError() << "Cannot open right click output port" << options.rightClickOutPortName;
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
    auto &b = leftClickPort.prepare(); b.clear(); b.addInt32(x); b.addInt32(y); leftClickPort.write();
}
void MainWindow::onRightClick(int x,int y) {
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
