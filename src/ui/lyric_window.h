#pragma once

#include "core/config.h"
#include "ui/taskbar_layout.h"

#include <windows.h>

#include <gdiplus.h>

#include <cstdint>
#include <string>

namespace taskbar_lyrics {

class LyricWindow {
public:
    LyricWindow() = default;
    ~LyricWindow() = default;

    LyricWindow(const LyricWindow&) = delete;
    LyricWindow& operator=(const LyricWindow&) = delete;

    int Run();

private:
    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM w_param,
        LPARAM l_param);

    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    bool Create();
    void RefreshPlacement();
    void UpdateVisibility();
    void ReassertTopmost();
    void RefreshHostState();
    void Render();
    void ShowContextMenu(POINT screen_point);
    bool UsesLightTaskbar() const;
    double ScrollElapsedMilliseconds() const;

    // Shared by both WinEvent hooks below: on any relevant z-order change it
    // re-raises the overlay above the taskbar.
    static void CALLBACK ZOrderEventProc(
        HWINEVENTHOOK hook,
        DWORD event,
        HWND window,
        LONG object_id,
        LONG child_id,
        DWORD thread_id,
        DWORD time);

    HWND window_ = nullptr;
    // Re-raises the overlay above the (also top-most) taskbar the instant any
    // window takes the foreground, so a right-click reliably hits the lyrics
    // rather than the taskbar underneath.
    HWINEVENTHOOK foreground_hook_ = nullptr;
    // Clicking the taskbar restacks it above us WITHOUT a foreground change, so
    // a second hook scoped to explorer fires on the taskbar's z-order reorder
    // and re-raises us immediately — the main defense against click flicker.
    HWINEVENTHOOK reorder_hook_ = nullptr;
    TaskbarLayout taskbar_layout_;
    TaskbarPlacement placement_{};
    Config config_;
    ULONG_PTR gdiplus_token_ = 0;

    // Visibility state machine (see UpdateVisibility). The window stays at its
    // last known gap when a layout query transiently finds none, and the
    // shown/hidden flip is debounced so a maximizing app cannot flicker it.
    bool have_placement_ = false;
    int gap_miss_streak_ = 0;
    bool shown_ = false;
    int visibility_streak_ = 0;
    RECT applied_rect_{};
    // While the context menu is up (and briefly after), keep the lyrics shown:
    // forcing the window foreground for the menu perturbs the host's foreground
    // and would otherwise blink the lyrics off as the menu is dismissed.
    ULONGLONG suppress_hide_until_ = 0;

    std::wstring primary_text_ = L"歌词加载中…";
    std::wstring secondary_text_;
    std::wstring song_id_;
    std::wstring line_key_;
    bool translation_available_ = false;
    bool playback_state_known_ = false;
    bool playing_ = false;
    bool previous_available_ = false;
    bool next_available_ = false;
    bool content_visible_ = true;
    bool needs_animation_ = false;
    std::uint32_t line_duration_ms_ = 5000;
    ULONGLONG line_started_at_ = 0;
    double frozen_scroll_elapsed_ms_ = 0;
    ULONGLONG loading_started_at_ = 0;
};

}  // namespace taskbar_lyrics
