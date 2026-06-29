#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace taskbar_lyrics {

enum class HostContentStatus {
    loading,
    ready,
    pure_music,
    failed,
    idle,
};

struct HostSnapshot {
    bool bridge_connected = false;
    HostContentStatus status = HostContentStatus::loading;
    bool playback_state_known = false;
    bool playing = false;
    bool has_track = false;
    bool has_previous = false;
    bool has_next = false;
    std::wstring song_id;
    std::wstring line_key;
    std::wstring primary;
    std::wstring translation;
    std::uint32_t line_duration_ms = 5000;
    std::uint64_t revision = 0;
};

enum class HostCommand {
    none,
    previous,
    toggle_playback,
    next,
};

HostSnapshot GetHostSnapshot();
bool PublishHostBridgeMessage(std::wstring_view message);
void QueueHostCommand(HostCommand command) noexcept;
HostCommand TakeHostCommand() noexcept;

}  // namespace taskbar_lyrics
