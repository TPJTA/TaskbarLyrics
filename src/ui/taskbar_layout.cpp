#include "ui/taskbar_layout.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace taskbar_lyrics {
namespace {

int Width(const RECT& rectangle) {
    return rectangle.right - rectangle.left;
}

int Height(const RECT& rectangle) {
    return rectangle.bottom - rectangle.top;
}

bool CoversMonitor(HWND window, const RECT& monitor) {
    // Raw window rectangle (as trafficmonitor does), not the DWM extended frame
    // bounds, which come back slightly inside the monitor for borderless
    // fullscreen windows and made the cover test flip once the window settled.
    RECT bounds{};
    if (!GetWindowRect(window, &bounds)) {
        return false;
    }
    return bounds.left <= monitor.left &&
           bounds.top <= monitor.top &&
           bounds.right >= monitor.right &&
           bounds.bottom >= monitor.bottom;
}

RECT Expanded(const RECT& rectangle, int padding, const RECT& limit) {
    RECT result{
        std::max(limit.left, rectangle.left - padding),
        std::max(limit.top, rectangle.top),
        std::min(limit.right, rectangle.right + padding),
        std::min(limit.bottom, rectangle.bottom),
    };
    return result;
}

}  // namespace

TaskbarLayout::~TaskbarLayout() {
    if (automation_ != nullptr) {
        automation_->Release();
    }
}

bool TaskbarLayout::ForegroundIsFullScreen(HWND lyric_window) const {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar == nullptr) {
        return false;
    }

    MONITORINFO monitor_info{sizeof(monitor_info)};
    HMONITOR taskbar_monitor =
        MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
    if (taskbar_monitor == nullptr ||
        !GetMonitorInfoW(taskbar_monitor, &monitor_info)) {
        return false;
    }

    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || foreground == taskbar ||
        foreground == lyric_window || foreground == GetDesktopWindow() ||
        foreground == GetShellWindow()) {
        return false;
    }

    wchar_t class_name[128]{};
    GetClassNameW(foreground, class_name, static_cast<int>(std::size(class_name)));
    if (_wcsicmp(class_name, L"Progman") == 0 ||
        _wcsicmp(class_name, L"WorkerW") == 0 ||
        _wcsicmp(class_name, L"Shell_TrayWnd") == 0) {
        return false;
    }

    // Only a fullscreen window on the taskbar's own monitor should hide the
    // lyrics; a fullscreen app on another display must not.
    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST) !=
        taskbar_monitor) {
        return false;
    }

    // Geometry alone separates true fullscreen (covers the whole monitor,
    // including the taskbar strip) from a merely maximized window (which stops
    // at the work area). A maximized "windowed fullscreen" app therefore does
    // NOT hide the lyrics.
    return CoversMonitor(foreground, monitor_info.rcMonitor);
}

bool TaskbarLayout::EnsureAutomation() {
    if (automation_ != nullptr) {
        return true;
    }
    return SUCCEEDED(CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&automation_)));
}

std::vector<RECT> TaskbarLayout::OccupiedRegions(
    HWND taskbar,
    const RECT& taskbar_rect) {
    std::vector<RECT> regions;
    if (!EnsureAutomation()) {
        return regions;
    }

    IUIAutomationElement* root = nullptr;
    IUIAutomationCondition* condition = nullptr;
    IUIAutomationElementArray* elements = nullptr;

    if (FAILED(automation_->ElementFromHandle(taskbar, &root)) ||
        FAILED(automation_->CreateTrueCondition(&condition)) ||
        FAILED(root->FindAll(TreeScope_Descendants, condition, &elements))) {
        if (elements != nullptr) {
            elements->Release();
        }
        if (condition != nullptr) {
            condition->Release();
        }
        if (root != nullptr) {
            root->Release();
        }
        return regions;
    }

    int length = 0;
    const UINT taskbar_dpi = GetDpiForWindow(taskbar);
    const int minimum_element_size = MulDiv(
        4, static_cast<int>(taskbar_dpi == 0 ? 96 : taskbar_dpi), 96);
    elements->get_Length(&length);
    for (int index = 0; index < length; ++index) {
        IUIAutomationElement* element = nullptr;
        if (FAILED(elements->GetElement(index, &element)) || element == nullptr) {
            continue;
        }

        BSTR name = nullptr;
        RECT bounds{};
        const bool has_name =
            SUCCEEDED(element->get_CurrentName(&name)) &&
            name != nullptr &&
            SysStringLen(name) > 0;
        const bool has_bounds =
            SUCCEEDED(element->get_CurrentBoundingRectangle(&bounds));

        if (has_name && has_bounds) {
            RECT intersection{};
            if (IntersectRect(&intersection, &bounds, &taskbar_rect) &&
                Width(intersection) >= minimum_element_size &&
                Height(intersection) >= minimum_element_size &&
                Width(intersection) < (Width(taskbar_rect) * 9) / 10) {
                regions.push_back(intersection);
            }
        }

        if (name != nullptr) {
            SysFreeString(name);
        }
        element->Release();
    }

    elements->Release();
    condition->Release();
    root->Release();
    return regions;
}

bool TaskbarLayout::Query(HWND /*lyric_window*/, TaskbarPlacement& placement) {
    placement = {};

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar == nullptr || !IsWindowVisible(taskbar)) {
        return false;
    }

    RECT taskbar_rect{};
    if (!GetWindowRect(taskbar, &taskbar_rect)) {
        return false;
    }
    const UINT window_dpi = GetDpiForWindow(taskbar);
    const UINT dpi = window_dpi == 0 ? 96 : window_dpi;

    MONITORINFO monitor_info{sizeof(monitor_info)};
    HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
        return false;
    }

    RECT visible_taskbar{};
    if (!IntersectRect(&visible_taskbar, &taskbar_rect, &monitor_info.rcMonitor) ||
        Height(visible_taskbar) <
            MulDiv(16, static_cast<int>(dpi), 96) ||
        visible_taskbar.bottom != monitor_info.rcMonitor.bottom) {
        return false;
    }

    const int target_width = MulDiv(420, static_cast<int>(dpi), 96);
    const int minimum_width = MulDiv(160, static_cast<int>(dpi), 96);
    const int padding = MulDiv(6, static_cast<int>(dpi), 96);

    std::vector<RECT> occupied = OccupiedRegions(taskbar, visible_taskbar);
    std::sort(
        occupied.begin(),
        occupied.end(),
        [](const RECT& left, const RECT& right) {
            return left.left < right.left ||
                   (left.left == right.left && left.right < right.right);
        });

    int cursor = visible_taskbar.left + padding;
    const int right_limit = visible_taskbar.right - padding;
    for (const RECT& raw_region : occupied) {
        const RECT region = Expanded(raw_region, padding, visible_taskbar);
        if (region.right <= cursor) {
            continue;
        }

        const int gap = region.left - cursor;
        if (gap >= minimum_width) {
            const int width = std::min(target_width, gap);
            placement.taskbar = visible_taskbar;
            placement.window =
                RECT{cursor, visible_taskbar.top, cursor + width, visible_taskbar.bottom};
            placement.dpi = dpi;
            placement.visible = true;
            return true;
        }
        cursor = std::max(cursor, static_cast<int>(region.right));
    }

    const int trailing_gap = right_limit - cursor;
    if (trailing_gap >= minimum_width) {
        const int width = std::min(target_width, trailing_gap);
        placement.taskbar = visible_taskbar;
        placement.window =
            RECT{cursor, visible_taskbar.top, cursor + width, visible_taskbar.bottom};
        placement.dpi = dpi;
        placement.visible = true;
        return true;
    }
    return false;
}

}  // namespace taskbar_lyrics
