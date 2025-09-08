#include <QApplication>
#include "Options.h"
#include "MainWindow.h"
#include <yarp/os/Network.h>
#include <cstdlib>  // for EXIT_FAILURE / EXIT_SUCCESS

int main(int argc, char **argv) {
    yarp::os::Network yarp;
    if (!yarp.checkNetwork()) {
        fprintf(stderr, "YARP network not available.\n");
        return EXIT_FAILURE; // was: return 1;
    }

    QApplication app(argc, argv);

    yarp::os::ResourceFinder rf;
    auto options = OptionsParser::parse(argc, argv, rf);

    MainWindow w(options);
    w.show();

    return app.exec(); // returns 0 (EXIT_SUCCESS) on normal exit
}
