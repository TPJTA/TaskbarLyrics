#include "ui/lyric_window.h"

#include <windows.h>

namespace {

DWORD WINAPI ClosePreviewAfterDelay(void*) {
    Sleep(15000);
    if (HWND window = FindWindowW(L"TaskbarLyrics.Window", nullptr);
        window != nullptr) {
        PostMessageW(window, WM_CLOSE, 0, 0);
    }
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
    HANDLE closer =
        CreateThread(nullptr, 0, ClosePreviewAfterDelay, nullptr, 0, nullptr);
    if (closer != nullptr) {
        CloseHandle(closer);
    }

    taskbar_lyrics::LyricWindow window;
    return window.Run();
}

