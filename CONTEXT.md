# Taskbar Lyrics

Taskbar Lyrics adds a focused lyric presentation to NetEase Cloud Music on Windows. It exists to make synchronized lyrics visible near the taskbar without requiring a separate application to be managed by the listener.

## Language

**Host Client**:
The running NetEase Cloud Music desktop application that owns playback state and lyric data.
_Avoid_: Main program, parent app

**Injected Plugin**:
The Taskbar Lyrics component loaded into the **Host Client** for the same lifetime as that client.
_Avoid_: Background service, companion app

**Lyric Window**:
The single visual surface that presents the current synchronized lyric for the **Host Client**.
_Avoid_: Desktop lyrics, lyric panel

**Primary Lyric**:
The current line-timed lyric in the song's original language. Every lyric-bearing presentation contains exactly one Primary Lyric, and words within the line are not highlighted individually.
_Avoid_: Current lyric, original text

**Secondary Lyric**:
An optional translation synchronized with the **Primary Lyric**. It exists only when the current song provides a translation.
_Avoid_: Sub-lyric, extra lyric

**Lyricless Track**:
A loaded track for which the **Host Client** provides no **Primary Lyric**. The **Lyric Window** represents it with the fixed text “纯音乐，请欣赏”.
_Avoid_: No playback, loading state

**Loading Track**:
A newly loaded track whose lyric result has not yet arrived. The plugin actively requests lyrics through the **Host Client** on every track change; it does not wait for the playback page to open. The previous track's lyric is cleared immediately and the **Lyric Window** displays “歌词加载中…” until the result is known.
_Avoid_: Lyricless track, stale lyric

**Failed Lyric**:
A loaded track whose lyric request failed in the **Host Client**. The **Lyric Window** displays “歌词加载失败” until the host retries successfully or the track changes.
_Avoid_: Lyricless track, plugin failure

**Idle Host**:
A running **Host Client** with no loaded track. The **Lyric Window** is hidden while the host is idle.
_Avoid_: Paused playback, lyricless track

**Primary-only Mode**:
A **Lyric Window** layout containing one larger **Primary Lyric** and no **Secondary Lyric**.
_Avoid_: One-line mode

**Assisted Mode**:
A **Lyric Window** layout containing a **Primary Lyric** and one smaller translated **Secondary Lyric** on two lines. It is the default mode on first use; when no translation exists, the window falls back to **Primary-only Mode** without reserving a blank second line.
_Avoid_: Two-line mode, translation mode

**Display Preference**:
The listener's remembered choice between **Assisted Mode** and **Primary-only Mode**. It survives restarts of the **Host Client** until changed through the **Lyric Menu**.
_Avoid_: Temporary mode, session setting

**Lyric Block**:
The **Primary Lyric** together with its optional **Secondary Lyric**, treated as one visual unit that is always vertically centered within the **Taskbar Bounds**. Its vertical position is recalculated whenever it changes between one and two lines.
_Avoid_: Text box, lyric lines

**Fixed Taskbar Type**:
Primary and translated lyric text use Microsoft YaHei at 14 logical px, multiplied by the active taskbar DPI scale (`dpi / 96`) for rendering. The listener does not manually choose a font size.
_Avoid_: Per-line font controls, mixed font families

**Adaptive Contrast**:
The automatically selected lyric foreground that contrasts with the current taskbar appearance. It uses a subtle text shadow for readability and does not expose manual color controls.
_Avoid_: Fixed lyric color, custom theme

**Line Scroll**:
The single horizontal movement used to reveal a lyric line that is wider than its available **Taskbar Placement**. Every line is left-aligned; an overflowing line advances from that same left edge according to the lyric line's duration so its end becomes visible before the next line, freezes in place while playback is paused, and resets when the line changes. In **Assisted Mode**, the Primary Lyric and Secondary Lyric calculate independent scroll distances over the same lyric duration.
_Avoid_: Text clipping, ellipsis

**Lyric Menu**:
The menu opened by right-clicking the **Lyric Window**, through which the listener controls translation visibility, playback, and track navigation. It contains one context-sensitive Playback Action: “暂停” while playing and “播放” while paused, plus Previous Track and Next Track actions; left-clicking the Lyric Window deliberately performs no action.
_Avoid_: Menu bar, settings window

**Taskbar Placement**:
The leftmost available region for the **Lyric Window** within the visible bounds of a Windows taskbar. It has a stable target width of 420 logical pixels, shrinks when less space is available, moves right past existing taskbar controls, never covers them, and remains fully contained by the taskbar. If no readable placement exists, the Lyric Window hides until sufficient space returns.
_Avoid_: Lower-left corner

**Taskbar Bounds**:
The current visible rectangle of the primary display's standard Windows 11 bottom taskbar that contains the **Taskbar Placement**. The bounds may change while the **Host Client** is running.
_Avoid_: Taskbar height

**Taskbar Visibility**:
The effective availability of the primary taskbar. The **Lyric Window** is visible only while a lyric presentation exists, the taskbar itself is visible, and no full-screen application occupies the primary display.
_Avoid_: Plugin enabled state

**Occupied Taskbar Region**:
Any part of the **Taskbar Bounds** already used by a Windows taskbar control or application button. A **Taskbar Placement** never overlaps an Occupied Taskbar Region.
_Avoid_: Icon area

## Example dialogue

> Developer: When does the Injected Plugin exist?
>
> Domain expert: It has the same lifetime as the Host Client.
>
> Developer: Where does it display the lyric?
>
> Domain expert: In one Lyric Window fully contained by the Taskbar Bounds, so it visually belongs to the taskbar.
>
> Developer: Can the listener make the lyric easier to read?
>
> Domain expert: Yes. The Lyric Menu switches between Primary-only Mode and Assisted Mode.
