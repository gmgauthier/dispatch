# Dispatch

**Vended by Grok Build**

![Dispatch on LCOS](brand/screenshot-feed.png)

An **Outlook Express-shaped** feed reader and mail client for The Lunduke Computer Operating System (LCOS). MAIL and FEED are modes of the same window.

Binary: `dispatch`. Unlicense.

LCOS itself: [https://github.com/BryanLunduke/LCOS](https://github.com/BryanLunduke/LCOS)

![About Dispatch](brand/screenshot-about.png)

## Status

**v1.1.11.** Send/Recv keeps the open extra folder when a new mailbox sorts ahead of it. A clicked link opens only for http and https. Return in the middle of a bullet splits that item. Unsubscribing during a refresh does not store that fetch on another feed. An HTML-only reply quotes decoded text. A numeric entity above the BMP is four UTF-8 bytes. A comma in a display name stays with that mailbox. A server Seen flag stays on mail already in the folder. A failed UID EXPUNGE leaves the other deleted messages. A trash mailbox such as `INBOX.Trash` is used instead of expunging the message. Outlook Express-shaped feed reader and mail client. Inbox right-click: archive, move to folder, Trash. Headless test suite and BUG-BACKLOG.md. See [INSTALL.md](INSTALL.md).

| Doc | What |
|---|---|
| [INSTALL.md](INSTALL.md) | `.deb`, tarball, AppImage, git build |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Locked decisions, architecture, milestones M0–M6, branching, semver, lint |

## Build

```
sudo apt install build-essential meson ninja-build pkg-config g++ libgtkmm-3.0-dev libxml2-dev libsoup-3.0-dev libfontconfig1-dev libetpan-dev libgmime-3.0-dev clang-format cppcheck
meson setup build
meson compile -C build
./build/dispatch
```

PR lint gate: `./scripts/lint.sh` (CI runs this; no `--fix`). Format `src/` locally with `./scripts/lint.sh --fix`.

## License

[The Unlicense](https://unlicense.org). See [UNLICENSE](UNLICENSE).
