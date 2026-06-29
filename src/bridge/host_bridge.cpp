#include "bridge/host_bridge.h"

#include "core/logging.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <mutex>
#include <string>
#include <vector>

namespace taskbar_lyrics {
namespace {

constexpr std::wstring_view message_prefix = L"__TASKBAR_LYRICS_V1__|";

std::mutex snapshot_mutex;
HostSnapshot snapshot;
std::atomic<HostCommand> pending_command = HostCommand::none;
bool bridge_logged = false;
bool status_logged = false;
HostContentStatus last_logged_status = HostContentStatus::loading;
std::wstring observed_song_id;
std::wstring first_ready_line_key;
bool timeline_progress_logged = false;

std::vector<std::wstring_view> Split(std::wstring_view value, wchar_t delimiter) {
    std::vector<std::wstring_view> fields;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(delimiter, start);
        if (end == std::wstring_view::npos) {
            fields.push_back(value.substr(start));
            break;
        }
        fields.push_back(value.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

int HexValue(wchar_t character) {
    if (character >= L'0' && character <= L'9') {
        return character - L'0';
    }
    if (character >= L'a' && character <= L'f') {
        return character - L'a' + 10;
    }
    if (character >= L'A' && character <= L'F') {
        return character - L'A' + 10;
    }
    return -1;
}

std::wstring DecodeComponent(std::wstring_view encoded) {
    std::string utf8;
    utf8.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const wchar_t character = encoded[index];
        if (character == L'%' && index + 2 < encoded.size()) {
            const int high = HexValue(encoded[index + 1]);
            const int low = HexValue(encoded[index + 2]);
            if (high >= 0 && low >= 0) {
                utf8.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        if (character == L'+') {
            utf8.push_back(' ');
        } else if (character <= 0x7f) {
            utf8.push_back(static_cast<char>(character));
        }
    }

    if (utf8.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring decoded(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        decoded.data(),
        length);
    return decoded;
}

std::uint32_t ParseDuration(std::wstring_view value) {
    std::string ascii;
    ascii.reserve(value.size());
    for (const wchar_t character : value) {
        if (character < L'0' || character > L'9') {
            return 5000;
        }
        ascii.push_back(static_cast<char>(character));
    }
    std::uint32_t result = 5000;
    const auto parsed = std::from_chars(
        ascii.data(), ascii.data() + ascii.size(), result);
    if (parsed.ec != std::errc{}) {
        return 5000;
    }
    return std::clamp<std::uint32_t>(result, 1200, 30000);
}

HostContentStatus ParseStatus(std::wstring_view value) {
    if (value == L"ready") {
        return HostContentStatus::ready;
    }
    if (value == L"pure") {
        return HostContentStatus::pure_music;
    }
    if (value == L"failed") {
        return HostContentStatus::failed;
    }
    if (value == L"idle") {
        return HostContentStatus::idle;
    }
    return HostContentStatus::loading;
}

const wchar_t* StatusName(HostContentStatus status) {
    switch (status) {
        case HostContentStatus::ready:
            return L"ready";
        case HostContentStatus::pure_music:
            return L"pure";
        case HostContentStatus::failed:
            return L"failed";
        case HostContentStatus::idle:
            return L"idle";
        case HostContentStatus::loading:
        default:
            return L"loading";
    }
}

}  // namespace

HostSnapshot GetHostSnapshot() {
    std::scoped_lock lock(snapshot_mutex);
    return snapshot;
}

bool PublishHostBridgeMessage(std::wstring_view message) {
    if (!message.starts_with(message_prefix)) {
        return false;
    }

    const std::vector<std::wstring_view> fields =
        Split(message.substr(message_prefix.size()), L'|');
    if (fields.size() < 8) {
        return true;
    }

    HostSnapshot next;
    next.bridge_connected = true;
    next.status = ParseStatus(fields[0]);
    next.playback_state_known = fields[1] == L"1";
    next.playing = fields[2] == L"1";
    next.has_track = fields[3] == L"1";
    next.song_id = DecodeComponent(fields[4]);
    next.line_key = DecodeComponent(fields[5]);
    next.line_duration_ms = ParseDuration(fields[6]);
    next.primary = DecodeComponent(fields[7]);
    if (fields.size() >= 9) {
        next.translation = DecodeComponent(fields[8]);
    }
    // Per-direction navigation availability (requirements.md). Older/partial
    // messages without these fields fall back to has_track so we never wrongly
    // disable the buttons when the host hasn't reported a definite answer.
    if (fields.size() >= 11) {
        next.has_previous = fields[9] == L"1";
        next.has_next = fields[10] == L"1";
    } else {
        next.has_previous = next.has_track;
        next.has_next = next.has_track;
    }

    {
        std::scoped_lock lock(snapshot_mutex);
        next.revision = snapshot.revision + 1;
        snapshot = std::move(next);
        if (!bridge_logged) {
            bridge_logged = true;
            LogInfo(L"CloudMusic page-state bridge connected.");
        }
        if (!status_logged || last_logged_status != snapshot.status) {
            status_logged = true;
            last_logged_status = snapshot.status;
            LogInfo(
                std::wstring(L"CloudMusic host state: ") +
                StatusName(snapshot.status));
        }
        if (!snapshot.song_id.empty() &&
            observed_song_id != snapshot.song_id) {
            observed_song_id = snapshot.song_id;
            first_ready_line_key.clear();
            timeline_progress_logged = false;
            LogInfo(L"CloudMusic track state changed.");
        }
        if (snapshot.status == HostContentStatus::ready &&
            !snapshot.line_key.empty()) {
            if (first_ready_line_key.empty()) {
                first_ready_line_key = snapshot.line_key;
            } else if (!timeline_progress_logged &&
                       first_ready_line_key != snapshot.line_key) {
                timeline_progress_logged = true;
                LogInfo(L"CloudMusic lyric timeline is advancing.");
            }
        }
    }
    return true;
}

void QueueHostCommand(HostCommand command) noexcept {
    if (command != HostCommand::none) {
        pending_command.store(command);
    }
}

HostCommand TakeHostCommand() noexcept {
    return pending_command.exchange(HostCommand::none);
}

}  // namespace taskbar_lyrics
