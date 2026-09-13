# Dispatch development plan

A gtkmm-3 **Outlook Express** shell for LCOS. v1 is the Feed mode (RSS 2 / Atom). Mail is a later mode of this binary.

Display name: **Dispatch**
Binary / repo / package: `dispatch`
APP_ID: `org.gmgauthier.Dispatch`
License: The Unlicense (`UNLICENSE`)
Repos: https://gitea.scriptorium/gmgauthier/dispatch (origin), https://github.com/gmgauthier/dispatch

Catalog note: `lcos-projects/DISPATCH.md`.

## Status (2026-09-13)

**M0 — Window.** Stub feeds, stacked panes, preview toggle, article pop-out, Mail tooltip `Coming soon...`. No HTTP.

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original. Chrome is Outlook Express, not Liferea |
| Name | Dispatch. Binary `dispatch`. APP_ID `org.gmgauthier.Dispatch` |
| Rejected | Lookout, Tabloid, Mailbox, WhatsUp |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Look | Feeds left, headlines **over** body. Not three columns |
| Preview | View → Preview Pane, on by default |
| Article | Double-click / Enter opens a top-level window. Several may be open. Not MDI |
| Mode | Toolbar far right: `[MAIL]` `[FEED]`. Feed pressed. Mail tooltip `Coming soon...` |
| Body | `Gtk::TextView`. No WebKit |
| Fetch | Worker thread, later (M1). libxml2 or libmrss |
| Store | `~/.config/dispatch/` — **ini** (pick locked in M0) |
| Network | User-added feed URLs. No account, no Fever / Miniflux sync, no daemon |
| Never as v1 | Podcasts, enclosures, header bar, tray-as-identity, IMAP/SMTP, NNTP, WebKit |
| Brand | LCOS beige / navy. No Bryan’s seal |
| License | The Unlicense |
| Versioning | `meson.build` is the source of truth |

## 2. Window

```
+------------------------------------------------------------------+
| File  Edit  View  Feeds  Help                                    |
+------------------------------------------------------------------+
| [Refresh] [Subscribe…] [Mark read]              [MAIL] [FEED]    |
+------------+-----------------------------------------------------+
| feeds      |  headlines                                          |
|            +-----------------------------------------------------+
|            |  body (preview)                                     |
+------------+-----------------------------------------------------+
| status                                                           |
+------------------------------------------------------------------+
```

## 3. Work plan

| Milestone | Done when |
|---|---|
| **M0 — Window** | Menus, toolbar, stacked panes, preview toggle, stub article window, About. **This tree.** |
| **M1 — One feed** | Add a URL; fetch; headlines fill; click shows body; double-click opens the article window. |
| **M2 — Subscribe list** | Several feeds persist. Unread counts. Refresh all. |
| **M3 — OPML** | Import / export. |
| **M4 — HTML subset** | Links, bold, paragraphs in preview *and* article window. Links spawn the browser. |
| **M5 — Polish** | Keys, last-selected feed, sash + preview-visible in ini, interval refresh while open. |
| **M6 — Package** | `debian/`, `scripts/release.sh` → `.deb`, tarball, AppImage. Tag `v0.1.0`. |

Folders, search-all-feeds, full-text search, podcasts, **Mail mode**: after v1.
