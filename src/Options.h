#pragma once
#include <QString>
#include <string>
#include <yarp/os/Property.h>
#include <yarp/os/ResourceFinder.h>

struct YarpViewOptions {
    QString windowTitle;
    std::string imgInputPortName;      // image input port name (defaults to --name value)
    std::string leftClickOutPortName;  // <basename>/left:click when --leftClick flag present
    std::string rightClickOutPortName; // <basename>/right:click when --rightClick flag present
    bool leftClickEnabled = false;     // true if --leftClick flag supplied
    bool rightClickEnabled = false;    // true if --rightClick flag supplied
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
    bool explicitGeometry = false; // retained for backward compatibility (true if both w and h specified)
    bool hasX = false;
    bool hasY = false;
    bool hasW = false;
    bool hasH = false;
};

class OptionsParser {
public:
    static YarpViewOptions parse(int &argc, char **argv, yarp::os::ResourceFinder &rf);
    static void fillResourceFinderDefaults(yarp::os::ResourceFinder &rf);
    static void printHelp();
};
