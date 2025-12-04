#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QShowEvent>
#include <deque>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Bottle.h>
#include "Options.h"
#include "ImageReceiver.h"
#include "ImageWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(const YarpViewOptions &opt, QWidget *parent=nullptr);
    ~MainWindow() override;

private slots:
    void onImage(const QImage &img, const yarp::os::Stamp &stamp);
    void onLeftClick(int x,int y);
    void onRightClick(int x,int y);
    void updateDisplayFps(double dispFps, double minFps, double maxFps);
    
    // File menu
    void saveSingleImage();
    void saveImageSet();
    
    // Image menu
    void originalSize();
    void originalAspectRatio();
    void applyStretchMode();
    void applyOriginalSizeMode();
    void applyAspectRatioMode();
    void toggleFreeze();
    void toggleSynch();
    void toggleAutoResize();
    void toggleDisplayPixelValue();
    void changeRefreshInterval();
    void toggleKeepAbove(); // newly added
    
    // Help menu
    void showAbout();

    // Display timer tick (asynchronous refresh)
    void displayTick();
    void setClientImageSize(int w, int h); // resize outer window so central image area matches (w,h)

private:
    void buildUi();
    void createMenus();
    void openPorts();

    YarpViewOptions options;
    ImageReceiver receiver;
    ImageWidget *imageWidget{nullptr};

    yarp::os::BufferedPort<yarp::os::Bottle> leftClickPort;  // opened only if options.leftClickEnabled
    yarp::os::BufferedPort<yarp::os::Bottle> rightClickPort; // opened only if options.rightClickEnabled

    QLabel *statusPortName{nullptr};
    QLabel *statusPort{nullptr};
    QLabel *statusDisplay{nullptr};
    QLabel *statusPixelValue{nullptr};
    QLabel *statusPixelPatch{nullptr};

    // Menu actions
    QAction *actSaveSingle{nullptr};
    QAction *actSaveSet{nullptr};
    QAction *actOriginalSize{nullptr};
    QAction *actOriginalAspect{nullptr};
    QAction *actFreeze{nullptr};
    QAction *actSynch{nullptr};
    QAction *actAutoResize{nullptr};
    QAction *actDisplayPixelValue{nullptr};
    QAction *actChangeRefresh{nullptr};
    QAction *actKeepAbove{nullptr}; // new action
    
    // Image set saving state
    bool savingImageSet{false};
    QString imageSetDirectory;
    int imageSetCounter{0};

    // Asynchronous display buffering
    QTimer *displayTimer{nullptr};
    QImage bufferedImage;
    bool hasBufferedImage{false};
    QElapsedTimer portTimer; // arrival intervals
    std::deque<double> portIntervals;
    static constexpr int PORT_WINDOW = 120;
    DisplayMode currentMode{DisplayMode::StretchToWindow};
    double aspectRatio{0.0};
    int lastImgW{-1};
    int lastImgH{-1};
    bool aspectAdjusting{false};
    double currentImageAspect{0.0};
    bool aspectResizeGuard{false};
protected:
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void showEvent(QShowEvent *e) override;
};
