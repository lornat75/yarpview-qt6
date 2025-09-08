#include "ImageReceiver.h"
#include <yarp/os/LogStream.h>
#include <QImage>
#include <QMetaObject>

ImageReceiver::ImageReceiver(QObject *parent) : QObject(parent) {
    port.owner = this;
}

ImageReceiver::~ImageReceiver() { 
    close(); 
}

bool ImageReceiver::open(const std::string &portName, bool useCallback) {
    if (!port.open(portName)) {
        yError() << "Failed to open input port" << portName;
        return false;
    }
    if (useCallback) {
        port.useCallback();
    }
    return true;
}

void ImageReceiver::close() {
    port.close();
}

void ImageReceiver::Port::onRead(yarp::sig::ImageOf<yarp::sig::PixelBgra> &img) {
    if (!owner || owner->frozen.load()) return;

    yarp::os::Stamp stamp;
    this->getEnvelope(stamp);

    // Wrap without copy then copy into Qt-owned image (QImage doesn't deep copy until detach).
    QImage qimg(img.getRawImage(), img.width(), img.height(), img.getRowSize(), QImage::Format_RGBA8888);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    // PixelBgra -> QImage expects RGBA8888 little endian means BGRA memory order; convert.
    QImage converted = qimg.rgbSwapped();
    qimg = converted;
#endif
    ImageReceiver *target = owner;
    QMetaObject::invokeMethod(owner, [target, qimg, stamp]() {
        if (target) emit target->imageArrived(qimg.copy(), stamp); // ensure safe copy
    }, Qt::QueuedConnection);
}
