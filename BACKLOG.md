# Dispatch backlog

Current release: **v0.1.1**. Last updated: 2026-09-16.

Outlook Express shell. v1 is Feed mode (RSS 2 / Atom). Mail is a later mode of this binary, not a second guest app. Binary `dispatch`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Design: `lcos-projects/DISPATCH.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

- **Mail mode.** Toolbar `[MAIL]` is a stub (tooltip `Coming soon...`). Same OE chrome: folders left, list over preview, double-click opens a message window. IMAP/SMTP the user configures; no “online account” product identity. v1 of mail: one mailbox, send/receive, local folders, an address-book *picker* from Ephemeris Contacts or a tiny local nick list — not a second people database. Multi-year; do not mix IMAP into Feed-mode work.

## Low Priority

- **Feed categories** (depth 1). Left pane is a tree: category names as branches, feeds as leaves. No nested categories. Click a feed → that feed’s titles (today). Click the **category** → aggregated titles from every feed in that group, in the title pane (same sort as a single feed). Examples: Tech, Politics, Entertainment. Persist in the ini / OPML if OPML already has `<outline>` groups. Uncategorized feeds stay at the root. Do not mix this with Mail-mode folders.
- Search all feeds
- Full-text search
- Podcasts-as-a-product (in-item images / mp3 / mp4 Play already shipped; a dedicated podcast UI is not)

## Out of Scope

- WebKit / WebEngine “just for HTML”
- Fetching the item’s HTML page to invent a body
- A feed *server* (Miniflux, tt-rss, Fever, Google Reader, Nextcloud News)
- NNTP (that is Pan)
- EPUB (that is Read-O-Matic)
- Calendar or Contacts in this window (those are Ephemeris)
- A second Outlook-shaped mail guest that duplicates this chrome
- An Outlook bar / left icon rail. Mode switch is the two toolbar buttons, far right
- Three **columns** (feeds | headlines | body)
- Pop-out as the *only* way to read; MDI article children
- Tray icon as the app; header bar; adaptive phone layout
- Electron
- Custom title bar; Bryan’s seal
- Re-theming Liferea

## Shipped

**v0.1.0 (M0–M6)** — Stacked panes (feeds left, headlines over body); View → Preview Pane; article window on double-click / Enter; subscribe / unsubscribe; unread counts; OPML import / export; `Gtk::TextView` HTML subset (links, bold, headings, lists; images in-pane; mp3/mp4 Play via the system handler); Appearance; persist last feed / sash / preview-visible / window size; auto-refresh while open (15 min); `[MAIL]` / `[FEED]` mode switch with Mail stubbed; `.deb` / tarball / AppImage. Config: `~/.config/dispatch/dispatch.ini`.
