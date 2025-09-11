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

void ImageReceiver::ImagePort::onRead(yarp::sig::ImageOf<yarp::sig::PixelBgra> &img) {
    if (!owner || owner->frozen.load()) return;

    yarp::os::Stamp stamp;
    getEnvelope(stamp);

    // PixelBgra (BGRA bytes) matches QImage::Format_ARGB32 in-memory layout on little-endian.
    QImage qimg(img.getRawImage(),
                img.width(),
                img.height(),
                img.getRowSize(),
                QImage::Format_ARGB32);

    ImageReceiver* target = owner;
    QMetaObject::invokeMethod(owner, [target, qimg, stamp]() {
        if (target) emit target->imageArrived(qimg.copy(), stamp); // deep copy happens here
    }, Qt::QueuedConnection);
}
