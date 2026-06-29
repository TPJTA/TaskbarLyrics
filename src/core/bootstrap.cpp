#include "core/bootstrap.h"

#include "bridge/cef_probe.h"
#include "core/logging.h"
#include "ui/lyric_window.h"

#include <windows.h>

#include <cwchar>
#include <string>

namespace taskbar_lyrics {
namespace {

INIT_ONCE bootstrap_once = INIT_ONCE_STATIC_INIT;

std::wstring CurrentProcessPath() {
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(GetCurrentProcess(), 0, path.data(), &length)) {
        return {};
    }
    path.resize(length);
    return path;
}

bool IsCloudMusicMainProcess(const std::wstring& process_path) {
    const auto slash = process_path.find_last_of(L"\\/");
    const wchar_t* file_name =
        slash == std::wstring::npos ? process_path.c_str() : process_path.c_str() + slash + 1;

    if (_wcsicmp(file_name, L"cloudmusic.exe") != 0) {
        return false;
    }

    const wchar_t* command_line = GetCommandLineW();
    return command_line == nullptr || wcsstr(command_line, L"--type=") == nullptr;
}

DWORD WINAPI BootstrapThread(void*) {
    const std::wstring process_path = CurrentProcessPath();
    if (!IsCloudMusicMainProcess(process_path)) {
        LogInfo(L"Proxy loaded; plugin bootstrap skipped for non-host process: " + process_path);
        return 0;
    }

    LogInfo(L"Taskbar Lyrics bootstrap started in: " + process_path);

    // Install the CEF browser-creation probe hooks here (outside DllMain's
    // loader lock). cloudmusic.dll may still be loading when the proxy attaches,
    // so retry briefly until the host module is mapped and the IAT patch lands.
    bool hooks_installed = false;
    for (int attempt = 0; attempt < 50 && !hooks_installed; ++attempt) {
        hooks_installed = InstallCefProbeHooks();
        if (!hooks_installed) {
            Sleep(100);
        }
    }
    LogInfo(
        hooks_installed
            ? L"CEF browser-creation probe hooks installed."
            : L"CEF browser-creation probe hooks were not installed.");
    LyricWindow lyric_window;
    return static_cast<DWORD>(lyric_window.Run());
}

BOOL CALLBACK StartBootstrap(PINIT_ONCE, void*, void**) {
    HANDLE thread = CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
    return TRUE;
}

}  // namespace

void ScheduleBootstrap() noexcept {
    InitOnceExecuteOnce(&bootstrap_once, StartBootstrap, nullptr, nullptr);
}

}  // namespace taskbar_lyrics
