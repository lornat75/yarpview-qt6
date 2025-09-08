#pragma once
#include <QString>
#include <string>
#include <yarp/os/Property.h>
#include <yarp/os/ResourceFinder.h>

struct YarpViewOptions {
    QString windowTitle;
    std::string imgInputPortName; // default /yarpview-gpt5/img:i
    std::string leftClickOutPortName; // default /yarpview-gpt5/o:point
    std::string rightClickOutPortName; // default /yarpview-gpt5/r:o:point
    bool autosize = false;
    bool synch = false; // synchronous display
    bool freeze = false;
    bool compact = false;
    bool minimal = false;
    bool keepAbove = false;
    bool saveOptions = false; // --saveoptions
    int refreshMs = 30; // polling/refresh period
    int winX = -1;
    int winY = -1;
    int winW = 0;
    int winH = 0;

    // internal derived
    bool explicitGeometry = false;
};

class OptionsParser {
public:
    static YarpViewOptions parse(int &argc, char **argv, yarp::os::ResourceFinder &rf);
    static void fillResourceFinderDefaults(yarp::os::ResourceFinder &rf);
    static void printHelp();
};
