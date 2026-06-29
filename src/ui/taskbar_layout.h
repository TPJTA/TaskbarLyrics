#pragma once

#include <windows.h>

#include <ole2.h>
#include <UIAutomation.h>

#include <vector>

namespace taskbar_lyrics {

struct TaskbarPlacement {
    RECT taskbar{};
    RECT window{};
    UINT dpi = 96;
    bool visible = false;
};

class TaskbarLayout {
public:
    TaskbarLayout() = default;
    ~TaskbarLayout();

    TaskbarLayout(const TaskbarLayout&) = delete;
    TaskbarLayout& operator=(const TaskbarLayout&) = delete;

    bool Query(HWND lyric_window, TaskbarPlacement& placement);

    // Cheap (no UI Automation) check: true when a window on the taskbar's
    // monitor covers it entirely (true fullscreen), so the lyrics should hide.
    bool ForegroundIsFullScreen(HWND lyric_window) const;

private:
    bool EnsureAutomation();
    std::vector<RECT> OccupiedRegions(HWND taskbar, const RECT& taskbar_rect);

    IUIAutomation* automation_ = nullptr;
};

}  // namespace taskbar_lyrics
