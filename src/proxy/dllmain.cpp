#include "core/bootstrap.h"

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
        // CEF probe-hook installation (IAT patching, which depends on
        // cloudmusic.dll already being mapped) is deferred to the bootstrap
        // thread rather than run here under the loader lock; see ADR 0004 and
        // BootstrapThread, which retries until the host module is present.
    }
    return TRUE;
}
