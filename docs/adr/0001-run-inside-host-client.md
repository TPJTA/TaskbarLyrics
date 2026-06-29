---
status: accepted
---

# Run inside the host client

Taskbar Lyrics will be loaded as an x64 injected plugin inside the NetEase Cloud Music host client instead of running as a separate background application. This ties startup and shutdown to the player and removes an independently resident process, accepting tighter coupling to client versions and a greater risk that plugin failures affect the host.
