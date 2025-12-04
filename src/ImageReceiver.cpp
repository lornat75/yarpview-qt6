#include "ImageReceiver.h"
#include <yarp/os/LogStream.h>
#include <QImage>
#include <QMetaObject>

#include <yarp/os/Time.h>

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

    // PixelBgra (BGRA bytes) matches QImage::Format_ARGB32 in-memory 
    QImage qimg(img.getRawImage(),
                img.width(),
                img.height(),
                img.getRowSize(),
                QImage::Format_ARGB32);

    //copies to avoid race condition with subsequent calls to onRead()
    QImage safe=qimg.copy();

    ImageReceiver* target = owner;
    QMetaObject::invokeMethod(owner, [target, safe=std::move(safe), stamp]() {
        //check for race condition with memory
        if (target) emit target->imageArrived(safe, stamp); 
    }, Qt::QueuedConnection);
}
