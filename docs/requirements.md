# Requirements

## Persistence

- The listener's display preference must persist across NetEase Cloud Music restarts.
- Plugin settings must be stored in a plugin-owned configuration file.

## Lyric menu

- Right-clicking the lyric window must open the lyric menu.
- The lyric menu must use the native Windows context-menu implementation rather than a custom-drawn surface.
- The menu must inherit system DPI scaling, theme, disabled-state rendering, and keyboard navigation.
- The menu must contain one checkable “显示翻译” preference item.
- When the current track has no translation, “显示翻译” must be disabled without changing its checked state or the persisted preference.
- When a translation becomes available, “显示翻译” must become enabled again and apply the persisted preference.
- The menu must contain one context-sensitive playback item rather than separate Play and Pause items.
- While playback is active, the playback item must read “暂停” and pause the host.
- While playback is paused, the playback item must read “播放” and resume the host.
- When no track is loaded or playback state is indeterminate, the playback item must be disabled.
- The menu must contain “上一首” and “下一首” items.
- Previous and Next must be enabled only when the host reports that the corresponding navigation action is available; both must be disabled when no track is loaded.
- The menu must not include an “打开网易云” or focus-host action.
- Left-clicking the lyric window must perform no action.

## Failure behavior

- Any compatibility check, hook, lyric bridge, or window initialization failure must disable the plugin for that host session.
- Plugin failure must never block NetEase Cloud Music startup.
- Failures must be written to a plugin-owned log without showing a blocking dialog.
- On a track change, stale lyrics must be cleared immediately and replaced with “歌词加载中…” until the host reports the new lyric result.
- The plugin must actively dispatch `async:lyric/fetchLyric` with `force: true` on every track change and retry while no result exists; loading must not depend on opening the playback page.
- A host lyric-request failure must display “歌词加载失败” and recover automatically when the host later supplies lyrics.

## Build and installation

- The source tree must provide separate build, install, and uninstall PowerShell scripts.
- The build script must compile an x64 proxy plugin DLL without modifying the installed host client.
- The install script must locate NetEase Cloud Music from its Windows uninstall registry entry without prompting during the normal path.
- Registry discovery must consider `InstallLocation`, `DisplayIcon`, and `UninstallString`, then validate the resolved directory.
- The install script may ask the user for a directory only when registry discovery is missing or invalid.
- An explicit command-line installation path may remain available for automation, but must not be required for normal installation.
- The install script must copy the built DLL into the NetEase Cloud Music installation directory only after validating the target installation and confirming that the host is not running.
- The install script must refuse to overwrite an unknown pre-existing proxy DLL.
- The uninstall script must remove only artifacts recorded as belonging to Taskbar Lyrics and must restore any explicitly backed-up file.
- Installation and removal must not patch `cloudmusic.exe`, `cloudmusic.dll`, or `package/orpheus.ntpk`.

## Layout

- The first release supports only the standard Windows 11 taskbar positioned along the bottom edge of the primary display.
- Third-party taskbar replacements and taskbars moved to other screen edges are out of scope for the first release.
- The complete lyric block must remain vertically centered within the current taskbar height.
- Switching between one and two lines must recalculate the block's vertical position.
- The lyric window must target a stable width of 420 device-independent pixels and shrink only when the available taskbar region is narrower.
- The lyric window width must not change to match individual lyric lines.
- The lyric window must hide when no non-overlapping region of minimum readable width exists and restore when sufficient space returns.
- All lyric lines must remain left-aligned, whether stationary or scrolling.
- In Assisted Mode, the primary and translated lines must scroll independently so each line can reveal its full width within the shared lyric duration.
- Primary and translated lyric text must use Microsoft YaHei at 14 logical px, scaled by the taskbar DPI (`dpi / 96`).
- The lyric menu must not expose manual font-size controls.
- Lyric colors must automatically contrast with the current taskbar appearance.
- Text must use a subtle shadow for readability over transparent taskbars.
- The lyric menu must not expose manual color controls.
- Lyrics must switch by whole line and must not use word-by-word highlighting.
- The lyric window must follow taskbar auto-hide transitions and must never remain visible while the taskbar is hidden.
- The lyric window must hide while another application occupies the primary display in full-screen mode and restore afterward at the current playback position.
