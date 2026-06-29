#include "bridge/host_bridge.h"

#include <cassert>

int main() {
    using namespace taskbar_lyrics;

    const bool consumed = PublishHostBridgeMessage(
        L"__TASKBAR_LYRICS_V1__|ready|1|1|1|123|123%3A4|4200|"
        L"%E4%BD%A0%E5%A5%BD|Hello");
    assert(consumed);

    const HostSnapshot snapshot = GetHostSnapshot();
    assert(snapshot.bridge_connected);
    assert(snapshot.status == HostContentStatus::ready);
    assert(snapshot.playback_state_known);
    assert(snapshot.playing);
    assert(snapshot.has_track);
    assert(snapshot.song_id == L"123");
    assert(snapshot.line_key == L"123:4");
    assert(snapshot.line_duration_ms == 4200);
    assert(snapshot.primary == L"你好");
    assert(snapshot.translation == L"Hello");

    QueueHostCommand(HostCommand::next);
    assert(TakeHostCommand() == HostCommand::next);
    assert(TakeHostCommand() == HostCommand::none);
    return 0;
}
