# Dispatch

**Vended by Grok Build**

An **Outlook Express-shaped** feed reader for The Lunduke Computer Operating System (LCOS). v1 is RSS 2 / Atom. Mail is a later mode of this same window.

Binary: `dispatch`. Unlicense.

LCOS itself: [https://github.com/BryanLunduke/LCOS](https://github.com/BryanLunduke/LCOS)

## Status

**M4 — HTML subset.** Preview renders feed HTML, images, and Play for mp3/mp4. OPML import/export. Feeds persist in `~/.config/dispatch/dispatch.ini`.

| Doc | What |
|---|---|
| [INSTALL.md](INSTALL.md) | git build (packaging is M6) |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Locked decisions, architecture, milestones M0–M6 |

## Build

```
sudo apt install build-essential meson ninja-build pkg-config g++ libgtkmm-3.0-dev libxml2-dev libsoup-3.0-dev libfontconfig1-dev
meson setup build
meson compile -C build
./build/dispatch
```

## License

[The Unlicense](https://unlicense.org). See [UNLICENSE](UNLICENSE).
