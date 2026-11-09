#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <fstream>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

constexpr int RES_MIST_DESKTOP    = 201;
constexpr int RES_MIST_HOST       = 202;
constexpr int RES_MIST_CLIENT     = 203;
constexpr int RES_MIST_SIGNALING  = 204;
constexpr int RES_MIST_ICO        = 205;
constexpr int RES_FONT_ROBOTO_REG = 206;
constexpr int RES_FONT_ROBOTO_MED = 207;
constexpr int RES_FONT_ROBOTO_BOLD= 208;
constexpr int RES_FONT_MAT_ICONS  = 209;

constexpr int IDC_BROWSE_BTN     = 1001;
constexpr int IDC_INSTALL_BTN    = 1002;
constexpr int IDC_CANCEL_BTN     = 1003;
constexpr int IDC_FINISH_BTN     = 1004;
constexpr int IDC_DIR_EDIT       = 1005;
constexpr int IDC_PROGRESS       = 1006;
constexpr int IDC_STATUS_TXT     = 1007;
constexpr int IDC_DESKTOP_CHK    = 1008;
constexpr int IDC_STARTMENU_CHK  = 1009;
constexpr int IDC_LAUNCH_CHK     = 1010;

COLORREF MINT_BRAND   = RGB(70, 222, 181);  // #46deb5 (--mint)
COLORREF MINT_ACCENT  = RGB(15, 132, 102);  // #0f8466 (--acc)
COLORREF MINT_CONTAIN = RGB(191, 240, 221); // #bff0dd (--ct)
COLORREF MINT_SOFT    = RGB(227, 246, 238); // #e3f6ee (--ct2)
COLORREF BG_CLOUD     = RGB(240, 250, 246); // #f0faf6 (--bg)
COLORREF SF_WHITE     = RGB(255, 255, 255); // #ffffff (--sf)
COLORREF TEXT_INK     = RGB(16, 35, 29);    // #10231d (--ink)
COLORREF TEXT_MUTED   = RGB(79, 101, 93);   // #4f655d (--mute)
COLORREF BORDER_LINE  = RGB(212, 231, 223); // #d4e7df (--line)

bool ExtractResourceToFile(HINSTANCE hInstance, int resourceId, const std::wstring& outputPath) {
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10)); // RT_RCDATA
    if (!hRes) {
        DWORD err = GetLastError();
        std::wstring msg = L"FindResourceW failed for " + std::to_wstring(resourceId) + L", err=" + std::to_wstring(err) + L"\n";
        OutputDebugStringW(msg.c_str());
        return false;
    }

    HGLOBAL hData = LoadResource(hInstance, hRes);
    if (!hData) return false;

    DWORD size = SizeofResource(hInstance, hRes);
    const void* pData = LockResource(hData);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::wstring msg = L"CreateFileW failed for " + outputPath + L", err=" + std::to_wstring(err) + L"\n";
        OutputDebugStringW(msg.c_str());
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);

    return (ok && written == size);
}

bool EnsureDirectoryRecursive(const std::wstring& path) {
    int ret = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return (ret == ERROR_SUCCESS || ret == ERROR_FILE_EXISTS || ret == ERROR_ALREADY_EXISTS);
}

std::wstring GetDefaultInstallPath() {
    PWSTR pLocalApp = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pLocalApp))) {
        std::wstring path(pLocalApp);
        CoTaskMemFree(pLocalApp);
        return path + L"\\Programs\\Mist";
    }
    return L"C:\\Mist";
}

std::wstring GetStartMenuProgramsPath() {
    PWSTR pPrograms = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &pPrograms))) {
        std::wstring path(pPrograms);
        CoTaskMemFree(pPrograms);
        return path + L"\\Mist";
    }
    return L"";
}

std::wstring GetDesktopPath() {
    PWSTR pDesktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop))) {
        std::wstring path(pDesktop);
        CoTaskMemFree(pDesktop);
        return path;
    }
    return L"";
}

HRESULT CreateShellShortcut(
    const std::wstring& targetPath,
    const std::wstring& shortcutPath,
    const std::wstring& iconPath,
    const std::wstring& description,
    const std::wstring& workingDir
) {
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (FAILED(hr)) return hr;

    shellLink->SetPath(targetPath.c_str());
    shellLink->SetWorkingDirectory(workingDir.c_str());
    shellLink->SetDescription(description.c_str());
    if (!iconPath.empty()) {
        shellLink->SetIconLocation(iconPath.c_str(), 0);
    }

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    hr = shellLink.As(&persistFile);
    if (FAILED(hr)) return hr;

    return persistFile->Save(shortcutPath.c_str(), TRUE);
}

void WriteDefaultConfig(const std::wstring& configPath) {
    const std::filesystem::path cfgPath(configPath);
    std::ofstream out(cfgPath);
    if (!out.is_open()) return;

    out << "[general]\n"
        << "app_name = Mist Native Streaming\n"
        << "version = 0.3.0-rc3\n"
        << "log_level = info\n\n"
        << "[video]\n"
        << "width = 1920\n"
        << "height = 1080\n"
        << "target_fps = 60\n"
        << "codec = h264\n"
        << "bitrate_kbps = 8000\n"
        << "vsync = false\n\n"
        << "[audio]\n"
        << "enabled = true\n"
        << "sample_rate = 48000\n"
        << "channels = 2\n"
        << "bitrate_kbps = 128\n\n"
        << "[input]\n"
        << "allow_remote_keyboard = false\n"
        << "allow_remote_mouse = false\n"
        << "allow_remote_gamepad = false\n";
}

void RegisterUninstall(const std::wstring& installDir) {
    HKEY hKey;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Mist";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* displayName = L"Mist Native Game Streaming Studio";
        const wchar_t* displayVersion = L"0.3.0-rc3";
        const wchar_t* publisher = L"Mist Systems Architecture";
        std::wstring displayIcon = installDir + L"\\mist_desktop.exe,0";
        std::wstring uninstallStr = L"\"" + installDir + L"\\uninstall.exe\" --uninstall";
        DWORD estSize = 25000;
        DWORD noModify = 1;

        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, reinterpret_cast<const BYTE*>(displayName), static_cast<DWORD>((wcslen(displayName) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, reinterpret_cast<const BYTE*>(displayVersion), static_cast<DWORD>((wcslen(displayVersion) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, reinterpret_cast<const BYTE*>(publisher), static_cast<DWORD>((wcslen(publisher) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, reinterpret_cast<const BYTE*>(displayIcon.c_str()), static_cast<DWORD>((displayIcon.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, reinterpret_cast<const BYTE*>(installDir.c_str()), static_cast<DWORD>((installDir.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, reinterpret_cast<const BYTE*>(uninstallStr.c_str()), static_cast<DWORD>((uninstallStr.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&estSize), sizeof(DWORD));
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&noModify), sizeof(DWORD));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&noModify), sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

void UnregisterUninstall() {
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Mist");
}

bool PerformInstall(
    HINSTANCE hInstance,
    const std::wstring& installDir,
    bool createDesktopLnk,
    bool createStartMenuLnk,
    std::function<void(int progress, const wchar_t* status)> progressCb
) {
    if (!EnsureDirectoryRecursive(installDir)) return false;
    EnsureDirectoryRecursive(installDir + L"\\config");

    progressCb(10, L"Staging directories created...");

    // Extract Binaries
    progressCb(20, L"Extracting mist_desktop.exe...");
    ExtractResourceToFile(hInstance, RES_MIST_DESKTOP, installDir + L"\\mist_desktop.exe");

    progressCb(40, L"Extracting mist_host.exe...");
    ExtractResourceToFile(hInstance, RES_MIST_HOST, installDir + L"\\mist_host.exe");

    progressCb(55, L"Extracting mist_client.exe...");
    ExtractResourceToFile(hInstance, RES_MIST_CLIENT, installDir + L"\\mist_client.exe");

    progressCb(70, L"Extracting mist-signaling.exe...");
    ExtractResourceToFile(hInstance, RES_MIST_SIGNALING, installDir + L"\\mist-signaling.exe");

    progressCb(80, L"Installing typography, icons and runtime configuration...");
    ExtractResourceToFile(hInstance, RES_MIST_ICO, installDir + L"\\mist.ico");
    EnsureDirectoryRecursive(installDir + L"\\resources\\fonts");
    ExtractResourceToFile(hInstance, RES_FONT_ROBOTO_REG, installDir + L"\\resources\\fonts\\Roboto-Regular.ttf");
    ExtractResourceToFile(hInstance, RES_FONT_ROBOTO_MED, installDir + L"\\resources\\fonts\\Roboto-Medium.ttf");
    ExtractResourceToFile(hInstance, RES_FONT_ROBOTO_BOLD, installDir + L"\\resources\\fonts\\Roboto-Bold.ttf");
    ExtractResourceToFile(hInstance, RES_FONT_MAT_ICONS, installDir + L"\\resources\\fonts\\MaterialSymbolsRounded.ttf");
    WriteDefaultConfig(installDir + L"\\config\\mist.ini");

    // Copy Self as uninstaller
    wchar_t currentExe[MAX_PATH];
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    CopyFileW(currentExe, (installDir + L"\\uninstall.exe").c_str(), FALSE);

    progressCb(90, L"Creating system shortcuts...");
    std::wstring desktopExe = installDir + L"\\mist_desktop.exe";
    std::wstring iconFile = installDir + L"\\mist.ico";

    if (createStartMenuLnk) {
        std::wstring smDir = GetStartMenuProgramsPath();
        if (!smDir.empty()) {
            EnsureDirectoryRecursive(smDir);
            CreateShellShortcut(desktopExe, smDir + L"\\Mist Game Streaming.lnk", iconFile, L"Mist Native Game Streaming Studio", installDir);
        }
    }

    if (createDesktopLnk) {
        std::wstring dDir = GetDesktopPath();
        if (!dDir.empty()) {
            CreateShellShortcut(desktopExe, dDir + L"\\Mist Game Streaming.lnk", iconFile, L"Mist Native Game Streaming Studio", installDir);
        }
    }

    progressCb(95, L"Registering with Windows Application Manager...");
    RegisterUninstall(installDir);

    progressCb(100, L"Installation completed successfully!");
    return true;
}

bool PerformUninstall(const std::wstring& installDir, bool silent) {
    if (!silent) {
        int resp = MessageBoxW(nullptr, L"Are you sure you want to completely uninstall Mist Native Game Streaming Platform?", L"Mist Uninstall", MB_YESNO | MB_ICONQUESTION);
        if (resp != IDYES) return false;
    }

    // 1. Remove Shortcuts
    std::wstring smDir = GetStartMenuProgramsPath();
    if (!smDir.empty()) {
        DeleteFileW((smDir + L"\\Mist Game Streaming.lnk").c_str());
        RemoveDirectoryW(smDir.c_str());
    }

    std::wstring dDir = GetDesktopPath();
    if (!dDir.empty()) {
        DeleteFileW((dDir + L"\\Mist Game Streaming.lnk").c_str());
    }

    // 2. Remove Registry
    UnregisterUninstall();

    // 3. Remove Installed Files
    DeleteFileW((installDir + L"\\mist_desktop.exe").c_str());
    DeleteFileW((installDir + L"\\mist_host.exe").c_str());
    DeleteFileW((installDir + L"\\mist_client.exe").c_str());
    DeleteFileW((installDir + L"\\mist-signaling.exe").c_str());
    DeleteFileW((installDir + L"\\mist.ico").c_str());
    DeleteFileW((installDir + L"\\resources\\fonts\\Roboto-Regular.ttf").c_str());
    DeleteFileW((installDir + L"\\resources\\fonts\\Roboto-Medium.ttf").c_str());
    DeleteFileW((installDir + L"\\resources\\fonts\\Roboto-Bold.ttf").c_str());
    DeleteFileW((installDir + L"\\resources\\fonts\\MaterialSymbolsRounded.ttf").c_str());
    RemoveDirectoryW((installDir + L"\\resources\\fonts").c_str());
    RemoveDirectoryW((installDir + L"\\resources").c_str());
    DeleteFileW((installDir + L"\\config\\mist.ini").c_str());
    RemoveDirectoryW((installDir + L"\\config").c_str());

    // Schedule self-delete of uninstaller and folder
    std::wstring cmd = L"/c ping 127.0.0.1 -n 2 > nul & del /f /q \"" + installDir + L"\\uninstall.exe\" & rmdir /s /q \"" + installDir + L"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);

    if (!silent) {
        MessageBoxW(nullptr, L"Mist has been successfully removed from your computer.", L"Mist Uninstall Complete", MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

void DrawSleekMintLogoGdi(HDC hdc, int x, int y, int size) {
    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, LR_SHARED);
    if (!hIcon) {
        hIcon = (HICON)LoadImageA(nullptr, "mist.ico", IMAGE_ICON, size, size, LR_LOADFROMFILE);
    }
    if (hIcon) {
        DrawIconEx(hdc, x, y, hIcon, size, size, 0, nullptr, DI_NORMAL);
        return;
    }

    HBRUSH hMintBrush = CreateSolidBrush(MINT_BRAND); // #46deb5
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, BORDER_LINE);

    HGDIOBJ oldBrush = SelectObject(hdc, hMintBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);

    RoundRect(hdc, x, y, x + size, y + size, static_cast<int>(size * 0.35f), static_cast<int>(size * 0.35f));

    // Geometric 'M' streaming paths
    HPEN hInkPen = CreatePen(PS_SOLID, std::max(2, static_cast<int>(size * 0.10f)), TEXT_INK);
    SelectObject(hdc, hInkPen);

    int pad = static_cast<int>(size * 0.22f);
    int w = size - pad * 2;
    int h = size - pad * 2;

    POINT pts[5] = {
        { x + pad, y + pad + h },
        { x + pad, y + pad },
        { x + pad + w / 2, y + pad + static_cast<int>(h * 0.55f) },
        { x + pad + w, y + pad },
        { x + pad + w, y + pad + h }
    };

    Polyline(hdc, pts, 5);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    DeleteObject(hMintBrush);
    DeleteObject(hBorderPen);
    DeleteObject(hInkPen);
}

// Dialog State
struct InstallerContext {
    HINSTANCE hInstance{nullptr};
    HWND hWnd{nullptr};
    HWND hEditDir{nullptr};
    HWND hProgress{nullptr};
    HWND hStatus{nullptr};
    HWND hChkDesktop{nullptr};
    HWND hChkStartMenu{nullptr};
    HWND hChkLaunch{nullptr};
    HWND hBtnInstall{nullptr};
    HWND hBtnCancel{nullptr};
    HWND hBtnFinish{nullptr};
    std::wstring installDir;
    bool isInstalled{false};
};

LRESULT CALLBACK InstallerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* ctx = reinterpret_cast<InstallerContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ctx = reinterpret_cast<InstallerContext*>(cs->lpCreateParams);
            ctx->hWnd = hWnd;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

            HICON hAppIcon = (HICON)LoadImageW(ctx->hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
            if (!hAppIcon) {
                hAppIcon = (HICON)LoadImageA(nullptr, "mist.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            }
            if (hAppIcon) {
                SendMessageW(hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hAppIcon));
                SendMessageW(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hAppIcon));
            }

            HFONT hFontMain = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            // Folder Label
            HWND hLbl = CreateWindowExW(0, L"STATIC", L"Destination Folder:", WS_CHILD | WS_VISIBLE, 30, 110, 300, 20, hWnd, nullptr, ctx->hInstance, nullptr);
            SendMessageW(hLbl, WM_SETFONT, reinterpret_cast<WPARAM>(hFontBold), TRUE);

            // Folder Edit
            ctx->hEditDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ctx->installDir.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 30, 135, 360, 26, hWnd, reinterpret_cast<HMENU>(IDC_DIR_EDIT), ctx->hInstance, nullptr);
            SendMessageW(ctx->hEditDir, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            // Browse Button
            HWND hBtnBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 400, 135, 95, 26, hWnd, reinterpret_cast<HMENU>(IDC_BROWSE_BTN), ctx->hInstance, nullptr);
            SendMessageW(hBtnBrowse, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            // Checkboxes
            ctx->hChkDesktop = CreateWindowExW(0, L"BUTTON", L"Create Desktop shortcut", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 30, 180, 280, 24, hWnd, reinterpret_cast<HMENU>(IDC_DESKTOP_CHK), ctx->hInstance, nullptr);
            SendMessageW(ctx->hChkDesktop, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(ctx->hChkDesktop, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            ctx->hChkStartMenu = CreateWindowExW(0, L"BUTTON", L"Create Start Menu program shortcut", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 30, 210, 320, 24, hWnd, reinterpret_cast<HMENU>(IDC_STARTMENU_CHK), ctx->hInstance, nullptr);
            SendMessageW(ctx->hChkStartMenu, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(ctx->hChkStartMenu, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            ctx->hChkLaunch = CreateWindowExW(0, L"BUTTON", L"Launch Mist Desktop now", WS_CHILD | BS_AUTOCHECKBOX, 30, 240, 280, 24, hWnd, reinterpret_cast<HMENU>(IDC_LAUNCH_CHK), ctx->hInstance, nullptr);
            SendMessageW(ctx->hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(ctx->hChkLaunch, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            // Status Text
            ctx->hStatus = CreateWindowExW(0, L"STATIC", L"Ready to install Mist Native Game Streaming Studio.", WS_CHILD | WS_VISIBLE, 30, 280, 470, 20, hWnd, reinterpret_cast<HMENU>(IDC_STATUS_TXT), ctx->hInstance, nullptr);
            SendMessageW(ctx->hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            // Progress Bar
            ctx->hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 30, 305, 465, 18, hWnd, reinterpret_cast<HMENU>(IDC_PROGRESS), ctx->hInstance, nullptr);
            SendMessageW(ctx->hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageW(ctx->hProgress, PBM_SETPOS, 0, 0);

            // Buttons
            ctx->hBtnInstall = CreateWindowExW(0, L"BUTTON", L"Install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 300, 345, 95, 32, hWnd, reinterpret_cast<HMENU>(IDC_INSTALL_BTN), ctx->hInstance, nullptr);
            SendMessageW(ctx->hBtnInstall, WM_SETFONT, reinterpret_cast<WPARAM>(hFontBold), TRUE);

            ctx->hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 405, 345, 90, 32, hWnd, reinterpret_cast<HMENU>(IDC_CANCEL_BTN), ctx->hInstance, nullptr);
            SendMessageW(ctx->hBtnCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFontMain), TRUE);

            ctx->hBtnFinish = CreateWindowExW(0, L"BUTTON", L"Finish", WS_CHILD | BS_DEFPUSHBUTTON, 405, 345, 90, 32, hWnd, reinterpret_cast<HMENU>(IDC_FINISH_BTN), ctx->hInstance, nullptr);
            SendMessageW(ctx->hBtnFinish, WM_SETFONT, reinterpret_cast<WPARAM>(hFontBold), TRUE);

            return 0;
        }

        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            HBRUSH hBg = CreateSolidBrush(BG_CLOUD);
            FillRect(hdc, &rc, hBg);
            DeleteObject(hBg);
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT clientRc;
            GetClientRect(hWnd, &clientRc);

            // Header Banner (Pure White with bottom mint border)
            RECT headerRc = { 0, 0, clientRc.right, 90 };
            HBRUSH hWhiteBrush = CreateSolidBrush(SF_WHITE);
            FillRect(hdc, &headerRc, hWhiteBrush);
            DeleteObject(hWhiteBrush);

            // Border Line
            HPEN hBorderPen = CreatePen(PS_SOLID, 1, BORDER_LINE);
            HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);
            MoveToEx(hdc, 0, 90, nullptr);
            LineTo(hdc, clientRc.right, 90);
            SelectObject(hdc, oldPen);
            DeleteObject(hBorderPen);

            // Draw Sleek Mint Logo
            DrawSleekMintLogoGdi(hdc, 24, 16, 56);

            // Banner Typography
            SetBkMode(hdc, TRANSPARENT);
            HFONT hTitleFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hSubFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            HGDIOBJ oldFont = SelectObject(hdc, hTitleFont);
            SetTextColor(hdc, TEXT_INK);
            TextOutW(hdc, 96, 22, L"Mist Game Streaming Studio", 26);

            SelectObject(hdc, hSubFont);
            SetTextColor(hdc, TEXT_MUTED);
            TextOutW(hdc, 96, 48, L"Play beyond your desk.  *  Version 0.3.0-rc3", 43);

            SelectObject(hdc, oldFont);
            DeleteObject(hTitleFont);
            DeleteObject(hSubFont);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdcStatic, TEXT_INK);
            SetBkMode(hdcStatic, TRANSPARENT);
            static HBRUSH s_bgBrush = CreateSolidBrush(BG_CLOUD);
            return reinterpret_cast<INT_PTR>(s_bgBrush);
        }

        case WM_CTLCOLORBTN: {
            static HBRUSH s_bgBrush = CreateSolidBrush(BG_CLOUD);
            return reinterpret_cast<INT_PTR>(s_bgBrush);
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdcEdit, TEXT_INK);
            SetBkColor(hdcEdit, SF_WHITE);
            static HBRUSH s_whiteBrush = CreateSolidBrush(SF_WHITE);
            return reinterpret_cast<INT_PTR>(s_whiteBrush);
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_CANCEL_BTN) {
                DestroyWindow(hWnd);
                return 0;
            } else if (wmId == IDC_BROWSE_BTN && ctx) {
                wchar_t path[MAX_PATH];
                BROWSEINFOW bi{};
                bi.hwndOwner = hWnd;
                bi.lpszTitle = L"Select Installation Directory for Mist:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    if (SHGetPathFromIDListW(pidl, path)) {
                        ctx->installDir = std::wstring(path) + L"\\Mist";
                        SetWindowTextW(ctx->hEditDir, ctx->installDir.c_str());
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (wmId == IDC_INSTALL_BTN && ctx && !ctx->isInstalled) {
                // Read directory from edit box
                wchar_t dirBuf[MAX_PATH];
                GetWindowTextW(ctx->hEditDir, dirBuf, MAX_PATH);
                ctx->installDir = dirBuf;

                bool chkDesktop = (SendMessageW(ctx->hChkDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool chkStart = (SendMessageW(ctx->hChkStartMenu, BM_GETCHECK, 0, 0) == BST_CHECKED);

                EnableWindow(ctx->hBtnInstall, FALSE);
                EnableWindow(ctx->hBtnCancel, FALSE);
                EnableWindow(ctx->hEditDir, FALSE);

                PerformInstall(ctx->hInstance, ctx->installDir, chkDesktop, chkStart, [ctx](int progress, const wchar_t* status) {
                    SendMessageW(ctx->hProgress, PBM_SETPOS, static_cast<WPARAM>(progress), 0);
                    SetWindowTextW(ctx->hStatus, status);
                    UpdateWindow(ctx->hWnd);
                });

                ctx->isInstalled = true;
                ShowWindow(ctx->hBtnInstall, SW_HIDE);
                ShowWindow(ctx->hBtnCancel, SW_HIDE);
                ShowWindow(ctx->hChkLaunch, SW_SHOW);
                ShowWindow(ctx->hBtnFinish, SW_SHOW);
                SetFocus(ctx->hBtnFinish);
            } else if (wmId == IDC_FINISH_BTN && ctx) {
                bool launch = (SendMessageW(ctx->hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (launch) {
                    std::wstring exe = ctx->installDir + L"\\mist_desktop.exe";
                    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, ctx->installDir.c_str(), SW_SHOWNORMAL);
                }
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace

static int RunInstaller(HINSTANCE hInstance, const wchar_t* pCmdLine) {
    (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    bool silent = false;
    bool uninstall = false;
    std::wstring installDir = GetDefaultInstallPath();

    int numArgs = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    if (argv) {
        for (int i = 1; i < numArgs; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"/S" || arg == L"/s" || arg == L"--silent") {
                silent = true;
            } else if (arg == L"/U" || arg == L"/u" || arg == L"--uninstall") {
                uninstall = true;
            } else if (arg.rfind(L"/D=", 0) == 0) {
                installDir = arg.substr(3);
            } else if (arg == L"--dir" && i + 1 < numArgs) {
                installDir = argv[++i];
            }
        }
        LocalFree(argv);
    }

    wchar_t fullPath[MAX_PATH];
    if (GetFullPathNameW(installDir.c_str(), MAX_PATH, fullPath, nullptr)) {
        installDir = fullPath;
    }

    if (uninstall) {
        wchar_t currentExe[MAX_PATH];
        GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
        PathRemoveFileSpecW(currentExe);
        std::wstring unDir = currentExe;
        bool ok = PerformUninstall(unDir, silent);
        CoUninitialize();
        return ok ? 0 : 1;
    }

    if (silent) {
        bool ok = PerformInstall(hInstance, installDir, true, true, [](int, const wchar_t*) {});
        CoUninitialize();
        return ok ? 0 : 1;
    }

    // Launch Modern Mint & White Setup Wizard Window
    InstallerContext ctx;
    ctx.hInstance = hInstance;
    ctx.installDir = installDir;

    const wchar_t* clsName = L"MistSetupWindowClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = InstallerWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    HBRUSH hBgBrush = CreateSolidBrush(BG_CLOUD);
    wc.hbrBackground = hBgBrush;
    wc.lpszClassName = clsName;
    RegisterClassExW(&wc);

    int winW = 540;
    int winH = 430;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = std::max(0, (screenW - winW) / 2);
    int posY = std::max(0, (screenH - winH) / 2);

    HWND hWnd = CreateWindowExW(
        0, clsName, L"Mist Game Streaming Studio — Setup Wizard",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        nullptr, nullptr, hInstance, &ctx
    );

    if (!hWnd) {
        DeleteObject(hBgBrush);
        CoUninitialize();
        return 1;
    }

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(hBgBrush);
    UnregisterClassW(clsName, hInstance);
    CoUninitialize();
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int) {
    return RunInstaller(hInstance, pCmdLine ? pCmdLine : L"");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    return RunInstaller(hInstance, GetCommandLineW());
}
