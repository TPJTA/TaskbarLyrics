#include "bridge/cef_probe.h"

#include "bridge/host_bridge.h"
#include "core/logging.h"
#include "bridge/cef_abi.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace taskbar_lyrics {
namespace {

using namespace cef_abi;

using CreateBrowser = int (*)(
    const WindowInfo*,
    Client*,
    const String*,
    const BrowserSettings*,
    DictionaryValue*,
    RequestContext*);

using CreateBrowserSync = Browser* (*)(
    const WindowInfo*,
    Client*,
    const String*,
    const BrowserSettings*,
    DictionaryValue*,
    RequestContext*);

using GetDisplayHandlerCallback =
    DisplayHandler*(TASKBAR_LYRICS_CEF_CALLBACK*)(Client*);

std::atomic<CreateBrowser> original_create_browser = nullptr;
std::atomic<CreateBrowserSync> original_create_browser_sync = nullptr;
std::atomic<bool> hooks_installed = false;
std::atomic<ConsoleCallback> original_console_callback = nullptr;
std::atomic<TitleCallback> original_title_callback = nullptr;
std::atomic<GetDisplayHandlerCallback> original_display_getter = nullptr;
std::atomic<bool> handler_wrap_logged = false;

std::wstring SafeString(const String* value, std::size_t limit = 512) {
    if (value == nullptr || value->str == nullptr || value->length == 0) {
        return L"<empty>";
    }
    const std::size_t length = std::min(value->length, limit);
    return std::wstring(value->str, length);
}

int TASKBAR_LYRICS_CEF_CALLBACK HookConsoleMessage(
    DisplayHandler* self,
    Browser* browser,
    int level,
    const String* message,
    const String* source,
    int line) {
    const std::wstring text = SafeString(message, 32768);
    if (PublishHostBridgeMessage(text)) {
        return 1;
    }

    const ConsoleCallback original = original_console_callback.load();
    return original != nullptr
        ? original(self, browser, level, message, source, line)
        : 0;
}

void TASKBAR_LYRICS_CEF_CALLBACK HookTitleChange(
    DisplayHandler* self,
    Browser* browser,
    const String* title) {
    const std::wstring text = SafeString(title, 32768);
    if (PublishHostBridgeMessage(text)) {
        return;
    }
    const TitleCallback original = original_title_callback.load();
    if (original != nullptr) {
        original(self, browser, title);
    }
}

void PatchReturnedDisplayHandler(DisplayHandler* handler) {
    if (handler == nullptr) {
        return;
    }

    if (handler->on_console_message != nullptr &&
        handler->on_console_message != HookConsoleMessage) {
        ConsoleCallback expected = nullptr;
        original_console_callback.compare_exchange_strong(
            expected, handler->on_console_message);
        handler->on_console_message = HookConsoleMessage;
    }
    if (handler->on_title_change != nullptr &&
        handler->on_title_change != HookTitleChange) {
        TitleCallback expected = nullptr;
        original_title_callback.compare_exchange_strong(
            expected, handler->on_title_change);
        handler->on_title_change = HookTitleChange;
    }
    if (!handler_wrap_logged.exchange(true)) {
        LogInfo(L"CEF display bridge handler wrapped.");
    }
}

DisplayHandler* TASKBAR_LYRICS_CEF_CALLBACK HookGetDisplayHandler(
    Client* client) {
    const GetDisplayHandlerCallback original = original_display_getter.load();
    DisplayHandler* handler =
        original != nullptr ? original(client) : nullptr;
    PatchReturnedDisplayHandler(handler);
    return handler;
}

void PatchConsoleHandler(Client* client) {
    if (client == nullptr || client->get_display_handler == nullptr) {
        return;
    }

    if (client->get_display_handler != HookGetDisplayHandler) {
        GetDisplayHandlerCallback expected = nullptr;
        if (!original_display_getter.compare_exchange_strong(
                expected, client->get_display_handler) &&
            original_display_getter.load() != client->get_display_handler) {
            LogInfo(L"CEF display getter differs; bridge left unchanged.");
            return;
        }
        client->get_display_handler = HookGetDisplayHandler;
        LogInfo(L"CEF display-handler getter hook installed.");
    }
}

constexpr wchar_t bridge_script[] = LR"JS(
(function () {
  try {
    var root = window;
    var stateApi = root.__taskbarLyricsStateApi;
    var webpackRequire = root.__taskbarLyricsWebpackRequire;
    if (!stateApi) {
      if (!webpackRequire && root.webpackJsonp && root.webpackJsonp.push) {
        var bridgeModuleId = 987654321;
        var bridgeModules = {};
        bridgeModules[bridgeModuleId] = function (module, exports, require) {
          webpackRequire = require;
          root.__taskbarLyricsWebpackRequire = require;
        };
        root.webpackJsonp.push([
          [bridgeModuleId],
          bridgeModules,
          [[bridgeModuleId]]
        ]);
      }
      if (webpackRequire) {
        var candidates = [];
        try {
          var known = webpackRequire(11);
          candidates.push(known, known && known.a, known && known.default);
        } catch (_) {}
        var cache = webpackRequire.c || {};
        Object.keys(cache).some(function (key) {
          var exported = cache[key] && cache[key].exports;
          candidates.push(exported, exported && exported.a, exported && exported.default);
          return false;
        });
        for (var candidateIndex = 0; candidateIndex < candidates.length; candidateIndex++) {
          var candidate = candidates[candidateIndex];
          if (candidate &&
              typeof candidate.getStore === "function" &&
              typeof candidate.getDispatch === "function") {
            stateApi = candidate;
            root.__taskbarLyricsStateApi = candidate;
            break;
          }
        }
      }
    }

    if (!stateApi) {
      document.title = "__TASKBAR_LYRICS_V1__|loading|0|0|0|||5000||";
      return;
    }

    var dispatch = stateApi.getDispatch();
    var command = "__TASKBAR_LYRICS_COMMAND__";
    if (command !== "none") {
      if (dispatch) {
        if (command === "toggle") {
          dispatch({
            type: "playing/switchResumeOrPause",
            payload: { triggerScene: "taskbarLyrics" }
          });
        } else if (command === "previous" || command === "next") {
          dispatch({
            type: "playingList/jump2Track",
            payload: {
              flag: command === "previous" ? -1 : 1,
              type: "call",
              triggerScene: "taskbarLyrics"
            }
          });
        }
      }
    }

    var state = stateApi.getStore() || {};
    var playing = state.playing || {};
    var lyric = state["async:lyric"];
    var track = playing.curPlaying || playing.curTrack || playing.curVoice;
    var hasTrack = !!(track || playing.resourceTrackId || playing.onlineResourceId);
    var songId = String(
      playing.resourceTrackId ||
      playing.onlineResourceId ||
      (track && (track.resourceId || track.trackId || track.id)) ||
      ""
    );
    var playState = playing.playingState;
    var playStateKnown = playState !== undefined && playState !== null;
    var isPlaying =
      playState === 2 ||
      String(playState).toLowerCase() === "playing";

    var waitingForRequestedLyric = false;
    if (hasTrack && songId && dispatch) {
      var fetchNow = Date.now();
      var fetchLines =
        lyric && Array.isArray(lyric.lyricLines) ? lyric.lyricLines : [];
      var fetchResultUsable = !!(
        lyric &&
        (fetchLines.length ||
         lyric.displayType === "pure" ||
         lyric.displayType === "ask")
      );
      if (root.__taskbarLyricsRequestedSong !== songId) {
        root.__taskbarLyricsRequestedSong = songId;
        root.__taskbarLyricsRequestTime = fetchNow;
        root.__taskbarLyricsLyricBeforeFetch = lyric;
        root.__taskbarLyricsAwaitingSong = songId;
        dispatch({
          type: "async:lyric/fetchLyric",
          payload: { force: true }
        });
      } else if (!fetchResultUsable &&
                 fetchNow - Number(root.__taskbarLyricsRequestTime || 0) >= 5000) {
        root.__taskbarLyricsRequestTime = fetchNow;
        dispatch({
          type: "async:lyric/fetchLyric",
          payload: { force: true }
        });
      }

      if (root.__taskbarLyricsAwaitingSong === songId) {
        if (lyric &&
            lyric !== root.__taskbarLyricsLyricBeforeFetch &&
            fetchResultUsable) {
          root.__taskbarLyricsAwaitingSong = "";
          root.__taskbarLyricsLyricBeforeFetch = null;
        } else {
          waitingForRequestedLyric = true;
        }
      }
    }

    var status = "loading";
    var lineKey = "";
    var primary = "";
    var translation = "";
    var durationMs = 5000;

    if (!hasTrack) {
      status = "idle";
    } else if (waitingForRequestedLyric) {
      status = "loading";
    } else if (lyric) {
      var lines = Array.isArray(lyric.lyricLines) ? lyric.lyricLines : [];
      var translated = Array.isArray(lyric.tlyricLines) ? lyric.tlyricLines : [];
      if (lyric.displayType === "pure" || lyric.displayType === "ask") {
        status = "pure";
      } else if (lines.length) {
        var currentSeconds = NaN;
        try {
          var progressModule = webpackRequire && webpackRequire(106);
          var progressSource = progressModule && progressModule.b;
          if (progressSource && typeof progressSource.getValue === "function") {
            currentSeconds = Number(progressSource.getValue());
          }
        } catch (_) {}

        var index = 0;
        if (Number.isFinite(currentSeconds)) {
          for (var lineIndex = 0; lineIndex < lines.length; lineIndex++) {
            var lineTime = Number(lines[lineIndex] && lines[lineIndex].time);
            if (Number.isFinite(lineTime) && lineTime <= currentSeconds) {
              index = lineIndex;
            }
          }
        } else {
          index = Number(playing.playingLyricLineNumber);
          if (!Number.isFinite(index)) index = 0;
          index = Math.max(0, Math.min(lines.length - 1, Math.floor(index)));
          if (playing.playingTrackId &&
              String(playing.playingTrackId) !== String(playing.resourceTrackId)) {
            index = 0;
          }
        }
        var current = lines[index] || {};
        var next = lines[index + 1] || {};
        primary = String(current.lyric || "");
        translation = String((translated[index] && translated[index].lyric) || "");
        lineKey = songId + ":" + index + ":" + String(current.time || 0);
        if (Number.isFinite(current.time) && Number.isFinite(next.time)) {
          durationMs = Math.round((next.time - current.time) * 1000);
        }
        durationMs = Math.max(1200, Math.min(30000, durationMs || 5000));
        status = primary ? "ready" : "loading";
      }
    }

    function encode(value) {
      return encodeURIComponent(String(value == null ? "" : value));
    }
    document.title = [
      "__TASKBAR_LYRICS_V1__",
      status,
      playStateKnown ? "1" : "0",
      isPlaying ? "1" : "0",
      hasTrack ? "1" : "0",
      encode(songId),
      encode(lineKey),
      String(durationMs),
      encode(primary),
      encode(translation)
    ].join("|");
  } catch (error) {
    document.title = "__TASKBAR_LYRICS_V1__|failed|0|0|0|||5000||";
  }
})();
)JS";

std::wstring ScriptForCommand(HostCommand command) {
    std::wstring command_name = L"none";
    switch (command) {
        case HostCommand::previous:
            command_name = L"previous";
            break;
        case HostCommand::toggle_playback:
            command_name = L"toggle";
            break;
        case HostCommand::next:
            command_name = L"next";
            break;
        case HostCommand::none:
            break;
    }

    std::wstring script(bridge_script);
    constexpr std::wstring_view placeholder = L"__TASKBAR_LYRICS_COMMAND__";
    const std::size_t position = script.find(placeholder);
    if (position != std::wstring::npos) {
        script.replace(position, placeholder.size(), command_name);
    }
    return script;
}

struct BridgeWorkerContext {
    Browser* browser = nullptr;
};

DWORD WINAPI BridgeWorker(void* parameter) {
    auto* context = static_cast<BridgeWorkerContext*>(parameter);
    Browser* browser = context->browser;
    delete context;

    Sleep(1500);
    bool first_execution_logged = false;
    while (browser != nullptr) {
        bool executed = false;
        Frame* frame = browser->get_main_frame(browser);
        if (frame != nullptr &&
            frame->execute_java_script != nullptr &&
            (frame->is_valid == nullptr || frame->is_valid(frame))) {
            const std::wstring script = ScriptForCommand(TakeHostCommand());
            String code{
                const_cast<wchar_t*>(script.data()),
                script.size(),
                nullptr,
            };
            const std::wstring source_name = L"taskbar-lyrics://bridge.js";
            String source{
                const_cast<wchar_t*>(source_name.data()),
                source_name.size(),
                nullptr,
            };
            frame->execute_java_script(frame, &code, &source, 1);
            executed = true;
        }
        if (frame != nullptr && frame->base.release != nullptr) {
            frame->base.release(&frame->base);
        }
        if (executed && !first_execution_logged) {
            first_execution_logged = true;
            LogInfo(L"CEF page-state bridge first script executed.");
        }
        Sleep(250);
    }
    return 0;
}

void StartBridgeWorker(Browser* browser) {
    if (browser == nullptr ||
        browser->get_main_frame == nullptr ||
        browser->base.add_ref == nullptr) {
        return;
    }

    browser->base.add_ref(&browser->base);
    auto* context = new BridgeWorkerContext{browser};
    HANDLE thread = CreateThread(
        nullptr, 0, BridgeWorker, context, 0, nullptr);
    if (thread == nullptr) {
        delete context;
        if (browser->base.release != nullptr) {
            browser->base.release(&browser->base);
        }
        LogInfo(L"CEF page-state bridge worker could not be started.");
        return;
    }
    CloseHandle(thread);
    LogInfo(L"CEF page-state bridge worker started.");
}

int HookCreateBrowser(
    const WindowInfo* window_info,
    Client* client,
    const String* url,
    const BrowserSettings* settings,
    DictionaryValue* extra_info,
    RequestContext* request_context) {
    LogInfo(L"CEF async browser creation: " + SafeString(url));
    PatchConsoleHandler(client);
    const CreateBrowser original = original_create_browser.load();
    return original != nullptr
        ? original(window_info, client, url, settings, extra_info, request_context)
        : 0;
}

Browser* HookCreateBrowserSync(
    const WindowInfo* window_info,
    Client* client,
    const String* url,
    const BrowserSettings* settings,
    DictionaryValue* extra_info,
    RequestContext* request_context) {
    LogInfo(L"CEF sync browser creation: " + SafeString(url));
    PatchConsoleHandler(client);
    const CreateBrowserSync original = original_create_browser_sync.load();
    Browser* browser = original != nullptr
        ? original(window_info, client, url, settings, extra_info, request_context)
        : nullptr;
    StartBridgeWorker(browser);
    return browser;
}

bool EqualAsciiInsensitive(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    while (*left != '\0' && *right != '\0') {
        char left_character = *left;
        char right_character = *right;
        if (left_character >= 'A' && left_character <= 'Z') {
            left_character += 'a' - 'A';
        }
        if (right_character >= 'A' && right_character <= 'Z') {
            right_character += 'a' - 'A';
        }
        if (left_character != right_character) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

bool PatchImport(
    HMODULE module,
    const char* imported_module,
    const char* imported_function,
    void* replacement,
    void** original) {
    if (module == nullptr || original == nullptr) {
        return false;
    }

    auto* base = reinterpret_cast<std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }

    const IMAGE_DATA_DIRECTORY& imports =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imports.VirtualAddress == 0) {
        return false;
    }

    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + imports.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor) {
        const char* module_name =
            reinterpret_cast<const char*>(base + descriptor->Name);
        if (!EqualAsciiInsensitive(module_name, imported_module)) {
            continue;
        }

        auto* first_thunk =
            reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        auto* name_thunk = descriptor->OriginalFirstThunk != 0
            ? reinterpret_cast<IMAGE_THUNK_DATA64*>(
                  base + descriptor->OriginalFirstThunk)
            : first_thunk;

        for (; name_thunk->u1.AddressOfData != 0; ++name_thunk, ++first_thunk) {
            if (IMAGE_SNAP_BY_ORDINAL64(name_thunk->u1.Ordinal)) {
                continue;
            }
            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                base + name_thunk->u1.AddressOfData);
            if (!EqualAsciiInsensitive(
                    reinterpret_cast<const char*>(import->Name),
                    imported_function)) {
                continue;
            }

            DWORD previous_protection = 0;
            if (!VirtualProtect(
                    &first_thunk->u1.Function,
                    sizeof(first_thunk->u1.Function),
                    PAGE_READWRITE,
                    &previous_protection)) {
                return false;
            }
            *original = reinterpret_cast<void*>(first_thunk->u1.Function);
            first_thunk->u1.Function =
                reinterpret_cast<ULONGLONG>(replacement);
            DWORD ignored = 0;
            VirtualProtect(
                &first_thunk->u1.Function,
                sizeof(first_thunk->u1.Function),
                previous_protection,
                &ignored);
            FlushInstructionCache(
                GetCurrentProcess(),
                &first_thunk->u1.Function,
                sizeof(first_thunk->u1.Function));
            return true;
        }
    }
    return false;
}

}  // namespace

bool InstallCefProbeHooks() noexcept {
    if (hooks_installed.load()) {
        return true;
    }

    const wchar_t* command_line = GetCommandLineW();
    if (command_line != nullptr && wcsstr(command_line, L"--type=") != nullptr) {
        return false;
    }

    HMODULE cloudmusic = GetModuleHandleW(L"cloudmusic.dll");
    if (cloudmusic == nullptr) {
        return false;
    }

    void* async_original = nullptr;
    void* sync_original = nullptr;
    const bool async_hooked = PatchImport(
        cloudmusic,
        "libcef.dll",
        "cef_browser_host_create_browser",
        reinterpret_cast<void*>(HookCreateBrowser),
        &async_original);
    const bool sync_hooked = PatchImport(
        cloudmusic,
        "libcef.dll",
        "cef_browser_host_create_browser_sync",
        reinterpret_cast<void*>(HookCreateBrowserSync),
        &sync_original);

    if (async_hooked) {
        original_create_browser.store(
            reinterpret_cast<CreateBrowser>(async_original));
    }
    if (sync_hooked) {
        original_create_browser_sync.store(
            reinterpret_cast<CreateBrowserSync>(sync_original));
    }
    hooks_installed.store(async_hooked || sync_hooked);
    return hooks_installed.load();
}

bool CefProbeHooksInstalled() noexcept {
    return hooks_installed.load();
}

}  // namespace taskbar_lyrics
