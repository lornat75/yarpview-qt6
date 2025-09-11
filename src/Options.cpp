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

    // Detect help early (ResourceFinder strips leading dashes when checking)
    if (rf.check("help") || rf.check("h")) {
        opt.helpRequested = true;
        return opt; // no need to parse further
    }

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
    auto out = &std::cout;
    *out << "Usage: yarpview-qt6 [options]" << std::endl;
    *out << "\nOptions:" << std::endl;
    struct Item { const char* opt; const char* desc; };
    const Item items[] = {
        {"--help",                "Show this help and exit"},
        {"--name <basename>",    "Base name (default /yarpview-qt6)"},
        {"--title <title>",      "Window title"},
        {"--leftClick",          "Enable left-click output port (<basename>/left:click)"},
        {"--rightClick",         "Enable right-click output port (<basename>/right:click)"},
        {"--autosize",           "Auto-resize window client area to image size"},
        {"--synch",              "Synchronous display (update only on new image)"},
        {"--p <ms>",             "Refresh period ms (alias: --refresh)"},
        {"--refresh <ms>",       "Same as --p <ms> (default 30)"},
        {"--compact",            "Hide menu and status bar"},
        {"--minimal",            "Hide chrome (frameless) and UI elements"},
        {"--keep-above",         "Start with window always on top"},
        {"--x <px>",             "Initial X position"},
        {"--y <px>",             "Initial Y position"},
        {"--w <px>",             "Initial window width"},
        {"--h <px>",             "Initial window height"},
        {"--saveoptions",        "Save options file on exit (not yet implemented)"}
    };
    size_t pad = 0; for (auto &i: items) { pad = std::max(pad, strlen(i.opt)); }
    for (auto &i: items) {
        *out << "  " << i.opt;
        size_t len = strlen(i.opt);
        if (len < pad) *out << std::string(pad - len, ' ');
        *out << "  " << i.desc << '\n';
    }
    *out << std::endl;
}
