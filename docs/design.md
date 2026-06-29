# Visual direction

Taskbar Lyrics is a quiet extension of the Windows 11 taskbar for listeners who want the current line available without opening the player. Its single job is to keep synchronized lyrics legible without looking like a floating card.

## Tokens

- **Dark-taskbar primary** `#F5F5F5`
- **Dark-taskbar secondary** `#C8C8C8`
- **Light-taskbar primary** `#202020`
- **Light-taskbar secondary** `#5A5A5A`
- **Text shadow** `#80000000`
- **Surface** `#00000000`

Primary and translated text use Microsoft YaHei at 14 logical px. All visual pixel measurements are multiplied by the active taskbar DPI scale (`dpi / 96`) before rendering.

## Layout

```text
single line                 translated
┌────────────────────┐      ┌────────────────────┐
│ Current lyric      │      │ Current lyric      │
│                    │      │ Translation        │
└────────────────────┘      └────────────────────┘
       ↑ vertically centered as one block ↑
```

Text is always left-aligned. No border, fill, icon, gradient, or decorative separator is drawn. Short lines remain still; overflow motion is the signature element and advances once over the line's duration.

## Lyric loading

Track changes actively trigger the host's `async:lyric/fetchLyric` action with `force: true`. The plugin therefore initiates lyric loading even when the playback page has never been opened, while retaining the host session, cloud-track support, and lyric parser.

## Self-critique

A rounded acrylic card would be a generic widget treatment and would visibly sit above the taskbar instead of belonging to it. Removing the card makes spacing and typography carry the design, while the synchronized one-pass movement gives the component a music-specific identity.
