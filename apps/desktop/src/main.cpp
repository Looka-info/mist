#include "DesktopApplication.h"
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <string_view>

static void SetupConsoleIfRequested(int argc, char* argv[]) {
    bool requestedCli = false;
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) continue;
        std::string_view a = argv[i];
        if (a == "--help" || a == "-h" || a == "--version" || a == "-v" ||
            a == "--self-test" || a == "--headless" || a == "--home" ||
            a == "--host-status" || a == "--connect" || a == "--devices" ||
            a == "--pairing" || a == "--network" || a == "--live-session" ||
            a == "--settings" || a == "--profile") {
            requestedCli = true;
            break;
        }
    }
    if (requestedCli) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
}

static int RunDesktopWithArgs(int argc, char* argv[]) {
    SetupConsoleIfRequested(argc, argv);
    mist::desktop::DesktopApplication app;

    auto initRes = app.initialize(argc, argv);
    if (!initRes) {
        std::string err = "Initialization error: " + initRes.error().format();
        std::cerr << err << std::endl;
        MessageBoxA(nullptr, err.c_str(), "Mist Desktop — Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (initRes.value().has_value()) {
        return initRes.value().value();
    }

    int exitCode = app.run();
    (void)app.shutdown();
    return exitCode;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    std::vector<char*> argv;
    if (argvW) {
        for (int i = 0; i < argc; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(len > 1 ? len - 1 : 0, '\0');
            if (len > 1) {
                WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &s[0], len, nullptr, nullptr);
            }
            args.push_back(s);
        }
        LocalFree(argvW);
    }
    for (auto& s : args) {
        argv.push_back(&s[0]);
    }
    return RunDesktopWithArgs(static_cast<int>(argv.size()), argv.data());
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR, int nCmdShow) {
    return wWinMain(hInst, hPrev, GetCommandLineW(), nCmdShow);
}
#endif

int main(int argc, char* argv[]) {
#if defined(_WIN32)
    return RunDesktopWithArgs(argc, argv);
#else
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
#endif
}
