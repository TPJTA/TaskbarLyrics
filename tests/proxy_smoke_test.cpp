#include <windows.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using AlphaBlendFunction = BOOL(WINAPI*)(
    HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

bool HasExport(HMODULE module, const char* name) {
    if (GetProcAddress(module, name) != nullptr) {
        return true;
    }
    std::cerr << "Missing export: " << name << '\n';
    return false;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "Usage: proxy_smoke_test <proxy-dll>\n";
        return 2;
    }

    HMODULE proxy = LoadLibraryW(argv[1]);
    if (proxy == nullptr) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 3;
    }

    bool valid = true;
    for (const char* name :
         std::array{"vSetDdrawflag", "AlphaBlend", "DllInitialize", "GradientFill", "TransparentBlt"}) {
        valid = HasExport(proxy, name) && valid;
    }

    auto alpha_blend =
        reinterpret_cast<AlphaBlendFunction>(GetProcAddress(proxy, "AlphaBlend"));
    if (alpha_blend != nullptr) {
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, 0};
        alpha_blend(nullptr, 0, 0, 0, 0, nullptr, 0, 0, 0, 0, blend);
    }

    Sleep(500);
    FreeLibrary(proxy);

    const wchar_t* log_directory = _wgetenv(L"TASKBARLYRICS_LOG_DIR");
    if (log_directory == nullptr ||
        !std::filesystem::exists(std::filesystem::path(log_directory) / L"taskbar-lyrics.log")) {
        std::cerr << "Bootstrap log was not created.\n";
        valid = false;
    }

    return valid ? 0 : 4;
}

