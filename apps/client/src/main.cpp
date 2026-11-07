#include "ClientApplication.h"
#include <iostream>

int main(int argc, char* argv[]) {
    mist::client::ClientApplication app;

    auto initRes = app.initialize(argc, argv);
    if (!initRes) {
        std::cerr << "Initialization error: " << initRes.error().format() << std::endl;
        return 1;
    }

    // Immediate exit requested by CLI option (--help, --version, --self-test)
    if (initRes.value().has_value()) {
        return initRes.value().value();
    }

    int exitCode = app.run();
    auto shutdownRes = app.shutdown();
    if (!shutdownRes) {
        std::cerr << "Shutdown error: " << shutdownRes.error().format() << std::endl;
        return 1;
    }

    return exitCode;
}
