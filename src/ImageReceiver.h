#pragma once
#include <QObject>
#include <QImage>
#include <QMutex>
#include <atomic>
#include <yarp/os/BufferedPort.h>
#include <yarp/sig/Image.h>
#include <yarp/os/Stamp.h>

class ImageReceiver : public QObject {
    Q_OBJECT
public:
    explicit ImageReceiver(QObject *parent=nullptr);
    ~ImageReceiver() override;

    bool open(const std::string &portName, bool useCallback=true);
    void close();

    void setFrozen(bool f) { frozen.store(f); }
    bool isFrozen() const { return frozen.load(); }

signals:
    void imageArrived(const QImage &img, const yarp::os::Stamp &stamp);

private:
    class Port : public yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelBgra>> {
    public:
        ImageReceiver *owner{nullptr};
        void onRead(yarp::sig::ImageOf<yarp::sig::PixelBgra> &img) override;
    };

    Port port;
    std::atomic<bool> frozen{false};
};
