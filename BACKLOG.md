# Dispatch backlog

Current release: **v1.1.16**. Last updated: 2026-10-06.

Outlook Express shell. Feed (RSS 2 / Atom) and Mail are modes of this binary, not a second guest app. Binary `dispatch`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Design: `lcos-projects/DISPATCH.md`. Mail plan: `lcos-projects/DISPATCH-MAIL.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

None.

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

**v1.1.16** — A stylesheet or script whose tag has attributes is left out of the quoted text.

**v1.1.15** — Two messages sent in the same second each keep a local copy. A failed Sent write leaves the Outbox file in place.

**v1.1.14** — Archiving, moving, or undeleting a message leaves the server copy where it is. A move into Trash still queues a server delete.

**v1.1.13** — An RFC 822 date shows the day, month, and year with no trailing space.

**v1.1.12** — Send/Recv merges newly discovered mailboxes after the sync worker has finished.

**v1.1.11** — Send/Recv selects the same extra folder after a newly listed mailbox sorts ahead of it.

**v1.1.10** — A clicked link opens only when it is http or https, so a file URL in a message cannot open a local file.

**v1.1.9** — Return in the middle of a bullet splits that item instead of adding the next bullet at the end of the line.

**v1.1.8** — Unsubscribing during Refresh All drops that fetch instead of writing it onto another feed.

**v1.1.7** — Reply and Forward of an HTML-only message quote `Tom & Jerry`, not the entity text.

**v1.1.6** — A numeric entity above the BMP, such as `&#128512;`, is four UTF-8 bytes.

**v1.1.5** — A comma in `Doe, Jane <jane@example.com>` stays with that mailbox.

**v1.1.4** — A server Seen flag stays on mail already in the folder. Send/Recv stores Seen only for a message marked in Dispatch.

**v1.1.3** — A failed UID EXPUNGE leaves other deleted messages in the mailbox.

**v1.1.2** — `INBOX.Trash` and `[Gmail]/Trash` are the trash mailbox. A failed move leaves the server message.

**v1.1.1** — Headless meson test suite, and known defects recorded in BUG-BACKLOG.md.

**v1.1.0** — Inbox right-click: Move to archive, Move to folder, Delete (Trash).

**v1.0.0** — Mail M7 rich compose (`multipart/alternative`); IMAP LIST folders after Trash; Outlook Express From/To/Cc/Date/Subject headers; server-side delete and unread; RSS/Atom item cache under `~/.local/share/dispatch/feeds/`.

**v0.2.0** — Mail M0–M5: IMAP/SMTP STARTTLS, local Maildir, compose, threads, Ephemeris address picker. Feed mode as v0.1.x.

**v0.1.0 (M0–M6)** — Stacked panes (feeds left, headlines over body); View → Preview Pane; article window on double-click / Enter; subscribe / unsubscribe; unread counts; OPML import / export; `Gtk::TextView` HTML subset (links, bold, headings, lists; images in-pane; mp3/mp4 Play via the system handler); Appearance; persist last feed / sash / preview-visible / window size; auto-refresh while open (15 min); `[MAIL]` / `[FEED]` mode switch with Mail stubbed; `.deb` / tarball / AppImage. Config: `~/.config/dispatch/dispatch.ini`.
