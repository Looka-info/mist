#include "DesktopApplication.h"
#include <iostream>

int main(int argc, char* argv[]) {
    mist::desktop::DesktopApplication app;

    auto initRes = app.initialize(argc, argv);
    if (!initRes) {
        std::cerr << "Initialization error: " << initRes.error().format() << std::endl;
        return 1;
    }

    if (initRes.value().has_value()) {
        return initRes.value().value();
    }

    int exitCode = app.run();
    (void)app.shutdown();
    return exitCode;
}
