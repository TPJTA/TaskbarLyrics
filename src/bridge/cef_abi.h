#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define TASKBAR_LYRICS_CEF_CALLBACK __stdcall
#else
#define TASKBAR_LYRICS_CEF_CALLBACK
#endif

namespace taskbar_lyrics::cef_abi {

struct BaseRefCounted {
    std::size_t size;
    void(TASKBAR_LYRICS_CEF_CALLBACK* add_ref)(BaseRefCounted* self);
    int(TASKBAR_LYRICS_CEF_CALLBACK* release)(BaseRefCounted* self);
    int(TASKBAR_LYRICS_CEF_CALLBACK* has_one_ref)(BaseRefCounted* self);
    int(TASKBAR_LYRICS_CEF_CALLBACK* has_at_least_one_ref)(BaseRefCounted* self);
};

struct String {
    wchar_t* str;
    std::size_t length;
    void (*dtor)(wchar_t* str);
};

struct Browser;
struct Frame;
struct Client;
struct DisplayHandler;
struct Task;

struct WindowInfo;
struct BrowserSettings;
struct DictionaryValue;
struct RequestContext;

struct Frame {
    BaseRefCounted base;
    int(TASKBAR_LYRICS_CEF_CALLBACK* is_valid)(Frame* self);
    void* undo;
    void* redo;
    void* cut;
    void* copy;
    void* paste;
    void* del;
    void* select_all;
    void* view_source;
    void* get_source;
    void* get_text;
    void* load_request;
    void* load_url;
    void(TASKBAR_LYRICS_CEF_CALLBACK* execute_java_script)(
        Frame* self,
        const String* code,
        const String* script_url,
        int start_line);
};

struct Browser {
    BaseRefCounted base;
    void* get_host;
    void* can_go_back;
    void* go_back;
    void* can_go_forward;
    void* go_forward;
    void* is_loading;
    void* reload;
    void* reload_ignore_cache;
    void* stop_load;
    void* get_identifier;
    void* is_same;
    void* is_popup;
    int(TASKBAR_LYRICS_CEF_CALLBACK* has_document)(Browser* self);
    Frame*(TASKBAR_LYRICS_CEF_CALLBACK* get_main_frame)(Browser* self);
};

using ConsoleCallback = int(TASKBAR_LYRICS_CEF_CALLBACK*)(
    DisplayHandler* self,
    Browser* browser,
    int level,
    const String* message,
    const String* source,
    int line);

using TitleCallback = void(TASKBAR_LYRICS_CEF_CALLBACK*)(
    DisplayHandler* self,
    Browser* browser,
    const String* title);

struct DisplayHandler {
    BaseRefCounted base;
    void* on_address_change;
    TitleCallback on_title_change;
    void* on_favicon_urlchange;
    void* on_fullscreen_mode_change;
    void* on_tooltip;
    void* on_status_message;
    ConsoleCallback on_console_message;
};

struct Client {
    BaseRefCounted base;
    void* get_audio_handler;
    void* get_context_menu_handler;
    void* get_dialog_handler;
    DisplayHandler*(TASKBAR_LYRICS_CEF_CALLBACK* get_display_handler)(
        Client* self);
};

struct Task {
    BaseRefCounted base;
    void(TASKBAR_LYRICS_CEF_CALLBACK* execute)(Task* self);
};

constexpr int thread_ui = 0;

}  // namespace taskbar_lyrics::cef_abi
