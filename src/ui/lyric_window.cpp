#include "ui/lyric_window.h"

#include "bridge/host_bridge.h"
#include "core/logging.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>

namespace taskbar_lyrics {
namespace {

constexpr wchar_t window_class_name[] = L"TaskbarLyrics.Window";

// The process hosts a single lyric window; the WinEvent callback (which fires
// for other applications' windows) uses this to reach the instance.
LyricWindow* g_lyric_window_instance = nullptr;

constexpr UINT_PTR layout_timer_id = 1;
constexpr UINT_PTR host_state_timer_id = 2;
constexpr UINT_PTR animation_timer_id = 3;
constexpr UINT_PTR reassert_timer_id = 4;
constexpr UINT layout_timer_interval_ms = 750;
constexpr UINT host_state_timer_interval_ms = 100;
constexpr UINT animation_timer_interval_ms = 33;
// One-frame safety net: even if both z-order hooks miss a restack, this re-raises
// us within ~16 ms (a single cheap SetWindowPos), so any flash is sub-frame.
constexpr UINT reassert_timer_interval_ms = 16;
constexpr ULONGLONG loading_failure_timeout_ms = 8000;
// Consecutive 100 ms ticks the desired visibility must hold before it is
// applied, so a transient blip while an app maximizes cannot flicker the
// lyrics (and true fullscreen still hides within ~300 ms).
constexpr int visibility_debounce_ticks = 3;
// Consecutive failed layout queries (at 750 ms each) before the lyrics hide for
// a genuinely missing taskbar gap, as opposed to a one-frame query miss.
constexpr int gap_miss_threshold = 2;

constexpr UINT command_show_translation = 100;
constexpr UINT command_previous = 101;
constexpr UINT command_playback = 102;
constexpr UINT command_next = 103;

int Width(const RECT& rectangle) {
    return rectangle.right - rectangle.left;
}

int Height(const RECT& rectangle) {
    return rectangle.bottom - rectangle.top;
}

bool SameRectangle(const RECT& left, const RECT& right) {
    return left.left == right.left &&
           left.top == right.top &&
           left.right == right.right &&
           left.bottom == right.bottom;
}

const wchar_t* PreferredFontName() {
    return L"Microsoft YaHei";
}

void DrawTextLine(
    Gdiplus::Graphics& graphics,
    const std::wstring& text,
    Gdiplus::Font& font,
    float x,
    float y,
    const Gdiplus::Color& color,
    const Gdiplus::Color& shadow_color,
    float shadow_offset) {
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    format.SetFormatFlags(
        Gdiplus::StringFormatFlagsNoWrap |
        Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    format.SetTrimming(Gdiplus::StringTrimmingNone);

    Gdiplus::SolidBrush shadow(shadow_color);
    Gdiplus::SolidBrush foreground(color);
    const Gdiplus::PointF shadow_position(
        x + shadow_offset, y + shadow_offset);
    const Gdiplus::PointF foreground_position(x, y);

    graphics.DrawString(
        text.c_str(), static_cast<INT>(text.size()), &font, shadow_position, &format, &shadow);
    graphics.DrawString(
        text.c_str(), static_cast<INT>(text.size()), &font, foreground_position, &format, &foreground);
}

float MeasureTextWidth(
    Gdiplus::Graphics& graphics,
    const std::wstring& text,
    Gdiplus::Font& font) {
    if (text.empty()) {
        return 0.0F;
    }
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    format.SetFormatFlags(
        Gdiplus::StringFormatFlagsNoWrap |
        Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    Gdiplus::RectF bounds;
    graphics.MeasureString(
        text.c_str(),
        static_cast<INT>(text.size()),
        &font,
        Gdiplus::PointF(0.0F, 0.0F),
        &format,
        &bounds);
    return bounds.Width;
}

}  // namespace

int LyricWindow::Run() {
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitialize_com = SUCCEEDED(com_result);

    Gdiplus::GdiplusStartupInput startup_input;
    if (Gdiplus::GdiplusStartup(&gdiplus_token_, &startup_input, nullptr) != Gdiplus::Ok) {
        LogInfo(L"GDI+ startup failed; lyric window disabled.");
        if (uninitialize_com) {
            CoUninitialize();
        }
        return 1;
    }

    if (!Create()) {
        LogInfo(L"Lyric window creation failed.");
        Gdiplus::GdiplusShutdown(gdiplus_token_);
        if (uninitialize_com) {
            CoUninitialize();
        }
        return 2;
    }

    LogInfo(L"Lyric window message loop started.");
    line_started_at_ = GetTickCount64();
    loading_started_at_ = line_started_at_;
    RefreshPlacement();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Gdiplus::GdiplusShutdown(gdiplus_token_);
    if (uninitialize_com) {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}

bool LyricWindow::Create() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{
        sizeof(window_class),
        CS_HREDRAW | CS_VREDRAW,
        WindowProcedure,
        0,
        0,
        instance,
        nullptr,
        LoadCursorW(nullptr, IDC_ARROW),
        nullptr,
        nullptr,
        window_class_name,
        nullptr,
    };

    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    window_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        window_class_name,
        L"Taskbar Lyrics",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance,
        this);
    if (window_ == nullptr) {
        return false;
    }
    g_lyric_window_instance = this;

    // Re-raise above the taskbar the instant any window takes the foreground
    // (out-of-context hooks are delivered on this thread inside the message
    // loop, so the callback may touch member state directly).
    foreground_hook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        &LyricWindow::ZOrderEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Clicking the taskbar restacks it above us but does NOT change the
    // foreground window, so the hook above never fires for it. Scope a second
    // hook to explorer (the taskbar's process) and react to its z-order reorder.
    DWORD taskbar_pid = 0;
    if (HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        GetWindowThreadProcessId(taskbar, &taskbar_pid);
    }
    reorder_hook_ = SetWinEventHook(
        EVENT_OBJECT_REORDER,
        EVENT_OBJECT_REORDER,
        nullptr,
        &LyricWindow::ZOrderEventProc,
        taskbar_pid,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    SetTimer(window_, layout_timer_id, layout_timer_interval_ms, nullptr);
    SetTimer(
        window_, host_state_timer_id, host_state_timer_interval_ms, nullptr);
    SetTimer(
        window_, animation_timer_id, animation_timer_interval_ms, nullptr);
    SetTimer(
        window_, reassert_timer_id, reassert_timer_interval_ms, nullptr);
    return true;
}

void CALLBACK LyricWindow::ZOrderEventProc(
    HWINEVENTHOOK /*hook*/,
    DWORD /*event*/,
    HWND /*window*/,
    LONG object_id,
    LONG /*child_id*/,
    DWORD /*thread_id*/,
    DWORD /*time*/) {
    if (object_id != OBJID_WINDOW) {
        return;
    }
    LyricWindow* self = g_lyric_window_instance;
    if (self != nullptr) {
        self->ReassertTopmost();
    }
}

void LyricWindow::ReassertTopmost() {
    // Keep the overlay above the (also top-most) taskbar, but BELOW any open
    // menu. Clicking the taskbar or an app maximizing raises the taskbar over us
    // without a foreground change, so this must run on a timer; positioning
    // below a visible menu window (#32768) stops it from covering our own menu.
    if (!shown_ || !IsWindowVisible(window_)) {
        return;
    }
    HWND menu = FindWindowW(L"#32768", nullptr);
    HWND insert_after =
        (menu != nullptr && IsWindowVisible(menu)) ? menu : HWND_TOPMOST;
    SetWindowPos(
        window_, insert_after, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

LRESULT CALLBACK LyricWindow::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {
    LyricWindow* self =
        reinterpret_cast<LyricWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<LyricWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

LRESULT LyricWindow::HandleMessage(
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {
    switch (message) {
        case WM_TIMER:
            if (w_param == layout_timer_id) {
                RefreshPlacement();
                return 0;
            }
            if (w_param == host_state_timer_id) {
                RefreshHostState();
                UpdateVisibility();
                return 0;
            }
            if (w_param == animation_timer_id) {
                if (needs_animation_ && playing_) {
                    Render();
                }
                return 0;
            }
            if (w_param == reassert_timer_id) {
                ReassertTopmost();
                return 0;
            }
            break;

        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_DPICHANGED:
            RefreshPlacement();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window_, &paint);
            EndPaint(window_, &paint);
            Render();
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_LBUTTONUP:
            return 0;

        case WM_RBUTTONUP: {
            POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            ClientToScreen(window_, &point);
            ShowContextMenu(point);
            return 0;
        }

        case WM_CONTEXTMENU: {
            POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            if (point.x == -1 && point.y == -1) {
                RECT bounds{};
                GetWindowRect(window_, &bounds);
                point = POINT{
                    bounds.left + MulDiv(
                        12, static_cast<int>(placement_.dpi), 96),
                    bounds.top};
            }
            ShowContextMenu(point);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(window_, layout_timer_id);
            KillTimer(window_, host_state_timer_id);
            KillTimer(window_, animation_timer_id);
            KillTimer(window_, reassert_timer_id);
            if (foreground_hook_ != nullptr) {
                UnhookWinEvent(foreground_hook_);
                foreground_hook_ = nullptr;
            }
            if (reorder_hook_ != nullptr) {
                UnhookWinEvent(reorder_hook_);
                reorder_hook_ = nullptr;
            }
            g_lyric_window_instance = nullptr;
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(window_, message, w_param, l_param);
}

void LyricWindow::RefreshPlacement() {
    // Only refresh the cached gap/position here (the expensive UI Automation
    // query). Whether the window is actually shown is decided, with debounce,
    // by UpdateVisibility. A transient query miss (e.g. the taskbar reflowing
    // while an app maximizes) keeps the last placement instead of hiding, which
    // is what used to flicker.
    TaskbarPlacement next{};
    if (taskbar_layout_.Query(window_, next) && next.visible) {
        const bool changed =
            !have_placement_ || !SameRectangle(placement_.window, next.window);
        placement_ = next;
        have_placement_ = true;
        gap_miss_streak_ = 0;
        if (changed) {
            LogInfo(
                L"Lyric window placed at " +
                std::to_wstring(placement_.window.left) + L"," +
                std::to_wstring(placement_.window.top) + L" " +
                std::to_wstring(Width(placement_.window)) + L"x" +
                std::to_wstring(Height(placement_.window)));
        }
    } else if (have_placement_ && ++gap_miss_streak_ >= gap_miss_threshold) {
        // A sustained loss of the taskbar gap (e.g. an auto-hide taskbar that
        // actually slid away) really should hide; a one-off miss does not.
        have_placement_ = false;
    }
    UpdateVisibility();
}

void LyricWindow::UpdateVisibility() {
    const bool fullscreen = taskbar_layout_.ForegroundIsFullScreen(window_);
    bool desired = content_visible_ && have_placement_ && !fullscreen;

    // Hold the lyrics shown through a context-menu interaction so the menu's
    // foreground steal cannot blink them off.
    if (shown_ && GetTickCount64() < suppress_hide_until_) {
        desired = true;
    }

    // Debounce the flip: the desired state must hold for a few ticks before it
    // is applied, so brief blips while an app maximizes/restores cannot flicker
    // the lyrics. A stable change (e.g. entering fullscreen) still applies.
    if (desired != shown_) {
        if (++visibility_streak_ >= visibility_debounce_ticks) {
            shown_ = desired;
            visibility_streak_ = 0;
        }
    } else {
        visibility_streak_ = 0;
    }

    if (!shown_) {
        if (IsWindowVisible(window_)) {
            ShowWindow(window_, SW_HIDE);
        }
        return;
    }

    const bool was_visible = IsWindowVisible(window_);
    const bool moved = !SameRectangle(applied_rect_, placement_.window);
    if (!was_visible || moved) {
        SetWindowPos(
            window_,
            HWND_TOPMOST,
            placement_.window.left,
            placement_.window.top,
            Width(placement_.window),
            Height(placement_.window),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        applied_rect_ = placement_.window;
        Render();
    }

    // Re-assert z-order every tick (menu-aware) so the taskbar can't bury us.
    ReassertTopmost();
}

void LyricWindow::RefreshHostState() {
    const HostSnapshot host = GetHostSnapshot();
    if (!host.bridge_connected) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    const bool was_playing = playing_;
    const std::wstring previous_primary = primary_text_;
    const std::wstring previous_secondary = secondary_text_;
    const std::wstring previous_line_key = line_key_;

    if (song_id_ != host.song_id) {
        song_id_ = host.song_id;
        loading_started_at_ = now;
        line_started_at_ = now;
        frozen_scroll_elapsed_ms_ = 0;
    }

    playback_state_known_ = host.playback_state_known;
    playing_ = host.playing;
    previous_available_ = host.has_track;
    next_available_ = host.has_track;
    line_duration_ms_ = host.line_duration_ms;

    switch (host.status) {
        case HostContentStatus::idle:
            content_visible_ = false;
            translation_available_ = false;
            secondary_text_.clear();
            line_key_.clear();
            break;

        case HostContentStatus::pure_music:
            content_visible_ = true;
            primary_text_ = L"纯音乐，请欣赏";
            secondary_text_.clear();
            translation_available_ = false;
            line_key_ = host.song_id + L":pure";
            loading_started_at_ = 0;
            break;

        case HostContentStatus::failed:
            content_visible_ = true;
            primary_text_ = L"歌词加载失败";
            secondary_text_.clear();
            translation_available_ = false;
            line_key_ = host.song_id + L":failed";
            break;

        case HostContentStatus::ready:
            content_visible_ = true;
            loading_started_at_ = 0;
            if (!host.primary.empty()) {
                primary_text_ = host.primary;
                secondary_text_ = host.translation;
                translation_available_ = !host.translation.empty();
                line_key_ = host.line_key;
            }
            break;

        case HostContentStatus::loading:
        default:
            content_visible_ = true;
            if (loading_started_at_ == 0) {
                loading_started_at_ = now;
            }
            primary_text_ =
                now - loading_started_at_ >= loading_failure_timeout_ms
                    ? L"歌词加载失败"
                    : L"歌词加载中…";
            secondary_text_.clear();
            translation_available_ = false;
            line_key_ = host.song_id + L":loading";
            break;
    }

    const bool line_changed =
        line_key_ != previous_line_key ||
        primary_text_ != previous_primary ||
        secondary_text_ != previous_secondary;
    if (line_changed) {
        line_started_at_ = now;
        frozen_scroll_elapsed_ms_ = 0;
    } else if (was_playing && !playing_) {
        frozen_scroll_elapsed_ms_ =
            static_cast<double>(now - line_started_at_);
    } else if (!was_playing && playing_) {
        const ULONGLONG frozen = static_cast<ULONGLONG>(
            std::max(0.0, frozen_scroll_elapsed_ms_));
        line_started_at_ = now >= frozen ? now - frozen : 0;
    }

    if (!content_visible_) {
        needs_animation_ = false;
        return;  // UpdateVisibility() (host-state timer) applies the hide.
    }

    if (line_changed || was_playing != playing_) {
        Render();
    }
}

double LyricWindow::ScrollElapsedMilliseconds() const {
    if (!playing_) {
        return frozen_scroll_elapsed_ms_;
    }
    const ULONGLONG now = GetTickCount64();
    return now >= line_started_at_
        ? static_cast<double>(now - line_started_at_)
        : 0.0;
}

bool LyricWindow::UsesLightTaskbar() const {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS && value != 0;
}

void LyricWindow::Render() {
    if (!placement_.visible || window_ == nullptr || !IsWindowVisible(window_)) {
        return;
    }

    const int width = Width(placement_.window);
    const int height = Height(placement_.window);
    if (width <= 0 || height <= 0) {
        return;
    }

    const bool light = UsesLightTaskbar();

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap =
        CreateDIBSection(screen, &bitmap_info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (memory == nullptr || bitmap == nullptr || pixels == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (memory != nullptr) {
            DeleteDC(memory);
        }
        ReleaseDC(nullptr, screen);
        return;
    }

    HGDIOBJ previous_bitmap = SelectObject(memory, bitmap);
    needs_animation_ = false;
    {
        Gdiplus::Bitmap surface(
            width,
            height,
            width * 4,
            PixelFormat32bppPARGB,
            static_cast<BYTE*>(pixels));
        Gdiplus::Graphics graphics(&surface);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        graphics.SetClip(Gdiplus::Rect(0, 0, width, height));

        const bool use_secondary =
            config_.ShowTranslation() &&
            translation_available_ &&
            !secondary_text_.empty();
        const float dpi_scale =
            static_cast<float>(placement_.dpi) / 96.0F;
        const float lyric_font_size = 14.0F * dpi_scale;
        const float primary_size = lyric_font_size;
        const float secondary_size = lyric_font_size;

        Gdiplus::FontFamily family(PreferredFontName());
        Gdiplus::Font primary_font(
            &family, primary_size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font secondary_font(
            &family, secondary_size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        const float primary_height = primary_font.GetHeight(&graphics);
        const float secondary_height =
            use_secondary ? secondary_font.GetHeight(&graphics) : 0.0F;
        const float line_gap = use_secondary ? std::max(0.0F, height * 0.01F) : 0.0F;
        const float block_height = primary_height + secondary_height + line_gap;
        const float top = std::max(0.0F, (height - block_height) / 2.0F);
        const float left =
            std::max(6.0F * dpi_scale, height * 0.16F);
        const float available_width =
            std::max(
                1.0F * dpi_scale,
                static_cast<float>(width) - left * 2.0F);
        const double progress = std::clamp(
            ScrollElapsedMilliseconds() /
                static_cast<double>(std::max<std::uint32_t>(
                    line_duration_ms_, 1)),
            0.0,
            1.0);

        const Gdiplus::Color primary_color =
            light ? Gdiplus::Color(255, 32, 32, 32)
                  : Gdiplus::Color(255, 245, 245, 245);
        const Gdiplus::Color secondary_color =
            light ? Gdiplus::Color(255, 90, 90, 90)
                  : Gdiplus::Color(255, 200, 200, 200);
        const Gdiplus::Color shadow_color(128, 0, 0, 0);

        const auto draw_scrolling_line =
            [&](const std::wstring& text,
                Gdiplus::Font& font,
                float y,
                const Gdiplus::Color& color) {
                const float text_width =
                    MeasureTextWidth(graphics, text, font);
                const float overflow =
                    std::max(0.0F, text_width - available_width);
                if (overflow > 0.5F * dpi_scale && progress < 1.0) {
                    needs_animation_ = true;
                }
                const float x =
                    left - overflow * static_cast<float>(progress);
                const Gdiplus::GraphicsState state = graphics.Save();
                graphics.SetClip(
                    Gdiplus::RectF(
                        left,
                        0.0F,
                        available_width,
                        static_cast<float>(height)),
                    Gdiplus::CombineModeReplace);
                DrawTextLine(
                    graphics,
                    text,
                    font,
                    x,
                    y,
                    color,
                    shadow_color,
                    1.0F * dpi_scale);
                graphics.Restore(state);
            };

        draw_scrolling_line(
            primary_text_, primary_font, top, primary_color);
        if (use_secondary) {
            draw_scrolling_line(
                secondary_text_,
                secondary_font,
                top + primary_height + line_gap,
                secondary_color);
        }
    }

    POINT destination{placement_.window.left, placement_.window.top};
    SIZE size{width, height};
    POINT source{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(
        window_, screen, &destination, &size, memory, &source, 0, &blend, ULW_ALPHA);

    SelectObject(memory, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
}

void LyricWindow::ShowContextMenu(POINT screen_point) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    UINT translation_flags = MF_STRING;
    if (config_.ShowTranslation()) {
        translation_flags |= MF_CHECKED;
    }
    if (!translation_available_) {
        translation_flags |= MF_GRAYED;
    }
    AppendMenuW(
        menu, translation_flags, command_show_translation, L"显示翻译");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(
        menu,
        MF_STRING | (previous_available_ ? MF_ENABLED : MF_GRAYED),
        command_previous,
        L"❚◀  上一首");
    AppendMenuW(
        menu,
        MF_STRING | (playback_state_known_ ? MF_ENABLED : MF_GRAYED),
        command_playback,
        playing_ ? L"❚❚  暂停" : L"▶  播放");
    AppendMenuW(
        menu,
        MF_STRING | (next_available_ ? MF_ENABLED : MF_GRAYED),
        command_next,
        L"▶❚  下一首");

    // The lyric window is WS_EX_NOACTIVATE, so it is never the foreground
    // window. TrackPopupMenu only dismisses on an outside click when its owner
    // is the foreground window, so force it foreground first and post the
    // documented dummy message afterwards (see MSDN TrackPopupMenu remarks).
    // That foreground steal perturbs the host, so suppress hiding around it.
    suppress_hide_until_ = GetTickCount64() + 2000;
    SetForegroundWindow(window_);

    const UINT command = TrackPopupMenuEx(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        screen_point.x,
        screen_point.y,
        window_,
        nullptr);

    PostMessageW(window_, WM_NULL, 0, 0);
    // Extend the window past dismissal, when the host's foreground (and the
    // lyric content state) settles back. The per-tick reassert in
    // UpdateVisibility restores z-order once the menu window is gone.
    suppress_hide_until_ = GetTickCount64() + 2000;

    if (command == command_show_translation && translation_available_) {
        if (config_.SetShowTranslation(!config_.ShowTranslation())) {
            Render();
        }
    } else if (command == command_previous && previous_available_) {
        QueueHostCommand(HostCommand::previous);
    } else if (command == command_playback && playback_state_known_) {
        QueueHostCommand(HostCommand::toggle_playback);
    } else if (command == command_next && next_available_) {
        QueueHostCommand(HostCommand::next);
    }

    DestroyMenu(menu);
}

}  // namespace taskbar_lyrics
