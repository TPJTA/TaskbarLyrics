---
status: accepted
---

# Consume playback and lyric state from the host

The injected plugin will consume the NetEase Cloud Music host client's existing playback, line-timed lyric, and translation state instead of independently calling a web API or reconstructing state from cache files. This avoids duplicate authentication and matching logic and preserves the host's exact track identity and timing, accepting that the bridge must be adapted when the host's bundled frontend changes.
