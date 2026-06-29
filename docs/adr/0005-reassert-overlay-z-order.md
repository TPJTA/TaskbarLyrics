---
status: accepted
---

# Actively re-assert overlay z-order above the taskbar

Because the lyric window is an independent top-most overlay rather than a child of Explorer (see [ADR 0002](0002-overlay-within-taskbar-bounds.md)), it shares the top-most z-order band with the taskbar; clicking the taskbar restacks it above the overlay **without** changing the foreground window, briefly covering the lyrics. We keep the overlay on top by re-asserting its z-order on every relevant restack: a `EVENT_SYSTEM_FOREGROUND` hook for foreground changes, a second `EVENT_OBJECT_REORDER` hook scoped to the taskbar's process (Explorer) for restacks that do not change the foreground, and a ~16 ms timer as a one-frame safety net. Each reassert positions the overlay just below any open context menu (`#32768`) so the menu is never covered.

## Considered options

- **Embed the overlay into the taskbar via `SetParent` into `Shell_TrayWnd`** (trafficmonitor's approach), which would remove the z-order competition entirely. Rejected: as an injected in-process plugin we run inside the host client, so a cross-process `SetParent` couples our message thread to Explorer and lags the host. trafficmonitor avoids this only because it is a standalone executable, not an in-process plugin.
- **Rely on a single low-frequency timer.** Rejected: a taskbar click leaves the lyrics covered until the next tick, which is visible as a flicker; the event hooks remove the latency and the fast timer bounds any miss to roughly one frame.
