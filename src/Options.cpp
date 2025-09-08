#include "Options.h"
#include <yarp/os/Value.h>
#include <yarp/os/LogStream.h>
#include <iostream>

void OptionsParser::fillResourceFinderDefaults(yarp::os::ResourceFinder &rf) {
    rf.setVerbose(false);
    rf.setDefaultConfigFile("yarpview-gpt5.ini");
    rf.setDefaultContext("yarpview-gpt5");
}

YarpViewOptions OptionsParser::parse(int &argc, char **argv, yarp::os::ResourceFinder &rf) {
    fillResourceFinderDefaults(rf);
    rf.configure(argc, argv);

    YarpViewOptions opt;

    opt.windowTitle = rf.check("title", yarp::os::Value("yarpview-qt6-gpt5")).asString().c_str();
    
    std::string baseName = rf.check("name", yarp::os::Value("/yarpview-gpt5")).asString();
    opt.imgInputPortName = baseName; //+ "/img:i";
    opt.leftClickOutPortName = rf.check("out", yarp::os::Value(baseName + "/o:point")).asString();
    opt.rightClickOutPortName = rf.check("rightout", yarp::os::Value(baseName + "/r:o:point")).asString();

    opt.autosize = rf.check("autosize");
    opt.synch = rf.check("synch");
    opt.compact = rf.check("compact");
    opt.minimal = rf.check("minimal");
    opt.keepAbove = rf.check("keep-above");
    opt.saveOptions = rf.check("saveoptions") || rf.check("SaveOptions");

    if (rf.check("p")) opt.refreshMs = rf.find("p").asInt32();
    if (rf.check("refresh")) opt.refreshMs = rf.find("refresh").asInt32();

    if (rf.check("x")) opt.winX = rf.find("x").asInt32();
    if (rf.check("y")) opt.winY = rf.find("y").asInt32();
    if (rf.check("w")) opt.winW = rf.find("w").asInt32();
    if (rf.check("h")) opt.winH = rf.find("h").asInt32();
    opt.explicitGeometry = (opt.winW>0 && opt.winH>0);

    return opt;
}

void OptionsParser::printHelp() {
    std::cout << "yarpview-qt6-gpt5 options:\n"
              << "  --name <basename>           Base name (default /yarpview-gpt5)\n"
              << "  --title <title>             Window title\n"
              << "  --p <ms> / --refresh <ms>   Refresh period ms (default 30)\n"
              << "  --autosize                  Auto-resize to incoming image\n"
              << "  --synch                     Synchronous display (no timer)\n"
              << "  --out <portname>            Left click output port\n"
              << "  --rightout <portname>       Right click output port\n"
              << "  --compact                   Compact UI mode\n"
              << "  --minimal                   Minimal UI mode\n"
              << "  --keep-above                Keep window above others\n"
              << "  --saveoptions               Save options file on exit\n"
              << "  --x <px> --y <px>           Initial top-left position\n"
              << "  --w <px> --h <px>           Initial size\n"
              << std::endl;
}
