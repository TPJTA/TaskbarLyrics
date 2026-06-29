---
status: accepted
---

# Load through an x64 msimg32 proxy

Taskbar Lyrics will enter NetEase Cloud Music 3.1.34.205281 through an x64 `msimg32.dll` proxy placed beside `cloudmusic.exe`. The client imports only `AlphaBlend` from this module; the proxy preserves all five standard system exports and schedules plugin bootstrap outside `DllMain`, giving the plugin the same lifetime as the host without a resident companion process. A controlled load test confirmed that the client starts and remains responsive with this proxy.
