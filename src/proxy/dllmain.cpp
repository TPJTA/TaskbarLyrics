#include "core/bootstrap.h"
#include "bridge/cef_probe.h"

#include <windows.h>

extern "C" BOOL WINAPI TaskbarLyrics_AlphaBlend(
    HDC destination,
    int destination_x,
    int destination_y,
    int destination_width,
    int destination_height,
    HDC source,
    int source_x,
    int source_y,
    int source_width,
    int source_height,
    BLENDFUNCTION blend) {
    taskbar_lyrics::ScheduleBootstrap();
    return GdiAlphaBlend(
        destination,
        destination_x,
        destination_y,
        destination_width,
        destination_height,
        source,
        source_x,
        source_y,
        source_width,
        source_height,
        blend);
}

extern "C" BOOL WINAPI TaskbarLyrics_GradientFill(
    HDC device_context,
    PTRIVERTEX vertices,
    ULONG vertex_count,
    void* mesh,
    ULONG mesh_count,
    ULONG mode) {
    return GdiGradientFill(device_context, vertices, vertex_count, mesh, mesh_count, mode);
}

extern "C" BOOL WINAPI TaskbarLyrics_TransparentBlt(
    HDC destination,
    int destination_x,
    int destination_y,
    int destination_width,
    int destination_height,
    HDC source,
    int source_x,
    int source_y,
    int source_width,
    int source_height,
    UINT transparent_color) {
    return GdiTransparentBlt(
        destination,
        destination_x,
        destination_y,
        destination_width,
        destination_height,
        source,
        source_x,
        source_y,
        source_width,
        source_height,
        transparent_color);
}

extern "C" BOOL WINAPI TaskbarLyrics_DllInitialize(HINSTANCE, DWORD, void*) {
    return TRUE;
}

extern "C" void WINAPI TaskbarLyrics_vSetDdrawflag() {}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        // Install the CEF browser-creation IAT hooks as early as possible:
        // they must be in place BEFORE the host calls cef_browser_host_create_
        // browser(_sync), which happens during host startup, typically before
        // the first AlphaBlend that drives ScheduleBootstrap. IAT patching is
        // loader-lock safe (no LoadLibrary, no thread creation). BootstrapThread
        // additionally retries this if cloudmusic.dll was not yet mapped here.
        taskbar_lyrics::InstallCefProbeHooks();
    }
    return TRUE;
}
