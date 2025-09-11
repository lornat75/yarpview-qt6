#include "Options.h"
#include <yarp/os/Value.h>
#include <yarp/os/LogStream.h>
#include <iostream>

void OptionsParser::fillResourceFinderDefaults(yarp::os::ResourceFinder &rf) {
    rf.setVerbose(false);
    rf.setDefaultConfigFile("yarpview-qt6.ini");
    rf.setDefaultContext("yarpview-qt6");
}

YarpViewOptions OptionsParser::parse(int &argc, char **argv, yarp::os::ResourceFinder &rf) {
    fillResourceFinderDefaults(rf);
    rf.configure(argc, argv);

    YarpViewOptions opt;

    opt.windowTitle = rf.check("title", yarp::os::Value("yarpview-qt6")).asString().c_str();

    std::string baseName = rf.check("name", yarp::os::Value("/yarpview-qt6")).asString();
    opt.imgInputPortName = baseName; // image input port (user supplies full name)

    // Optional click output ports: flags only. When flag present, construct default port name.
    if (rf.check("leftClick")) {
        opt.leftClickEnabled = true;
        opt.leftClickOutPortName = baseName + "/left:click";
    }
    if (rf.check("rightClick")) {
        opt.rightClickEnabled = true;
        opt.rightClickOutPortName = baseName + "/right:click";
    }

    opt.autosize = rf.check("autosize");
    opt.synch = rf.check("synch");
    opt.compact = rf.check("compact");
    opt.minimal = rf.check("minimal");
    opt.keepAbove = rf.check("keep-above");
    opt.saveOptions = rf.check("saveoptions") || rf.check("SaveOptions");

    if (rf.check("p")) opt.refreshMs = rf.find("p").asInt32();
    if (rf.check("refresh")) opt.refreshMs = rf.find("refresh").asInt32();

    if (rf.check("x")) { opt.winX = rf.find("x").asInt32(); opt.hasX = true; }
    if (rf.check("y")) { opt.winY = rf.find("y").asInt32(); opt.hasY = true; }
    if (rf.check("w")) { opt.winW = rf.find("w").asInt32(); opt.hasW = true; }
    if (rf.check("h")) { opt.winH = rf.find("h").asInt32(); opt.hasH = true; }
    opt.explicitGeometry = (opt.hasW && opt.hasH);

    return opt;
}

void OptionsParser::printHelp() {
    std::cout << "yarpview-qt6 options:\n"
              << "  --name <basename>           Base name (default /yarpview-qt6)\n"
              << "  --title <title>             Window title\n"
              << "  --p <ms> / --refresh <ms>   Refresh period ms (default 30)\n"
              << "  --autosize                  Auto-resize to incoming image\n"
              << "  --synch                     Synchronous display (no timer)\n"
              << "  --leftClick                 Enable left-click output port (<basename>/left:click)\n"
              << "  --rightClick                Enable right-click output port (<basename>/right:click)\n"
              << "  --compact                   Compact UI mode\n"
              << "  --minimal                   Minimal UI mode\n"
              << "  --keep-above                Keep window above others\n"
              << "  --saveoptions               Save options file on exit\n"
              << "  --x <px> --y <px>           Initial top-left position\n"
              << "  --w <px> --h <px>           Initial window width/height (each optional)\n"
              << std::endl;
}
