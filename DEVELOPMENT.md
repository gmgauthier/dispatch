# Dispatch development plan

A gtkmm-3 **Outlook Express** shell for LCOS. v1 is the Feed mode (RSS 2 / Atom). Mail is a later mode of this binary.

Display name: **Dispatch**
Binary / repo / package: `dispatch`
APP_ID: `org.gmgauthier.Dispatch`
License: The Unlicense (`UNLICENSE`)
Repos: https://gitea.scriptorium/gmgauthier/dispatch (origin), https://github.com/gmgauthier/dispatch

Catalog note: `lcos-projects/DISPATCH.md`.

## Status (2026-09-19)

**v1.0.0.** Feed M0–M6 plus Mail M0–M7, IMAP folders, OE headers, server flags, feed item cache. Tag `v1.0.0`.

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original. Chrome is Outlook Express, not Liferea |
| Name | Dispatch. Binary `dispatch`. APP_ID `org.gmgauthier.Dispatch` |
| Rejected | Lookout, Tabloid, Mailbox, WhatsUp |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Look | Feeds left, headlines **over** body. Not three columns. Options → Appearance (font, size, palette) like Read-O-Matic / YOLO-dex |
| Preview | View → Preview Pane, on by default |
| Article | Double-click / Enter opens a top-level window. Several may be open. Not MDI |
| Mode | Toolbar far right: `[MAIL]` `[FEED]`. Feed pressed. Mail tooltip `Coming soon...` |
| Body | `Gtk::TextView`. No WebKit |
| Fetch | Worker thread. **libsoup-3.0** GET, **libxml2** RSS 2 / RDF / Atom |
| Store | `~/.config/dispatch/` — **ini** (pick locked in M0) |
| Network | User-added feed URLs. No account, no Fever / Miniflux sync, no daemon |
| Never as v1 | Podcasts, enclosures, header bar, tray-as-identity, IMAP/SMTP, NNTP, WebKit |
| Brand | LCOS beige / navy. No Bryan’s seal |
| License | The Unlicense |
| Versioning | Semantic (`MAJOR.MINOR.PATCH`). `meson.build` is the source of truth. Debian changelog and git tag `vX.Y.Z` match it. See **Process**. |

## 2. Window

```
+------------------------------------------------------------------+
| File  Edit  View  Feeds  Options  Help                           |
+------------------------------------------------------------------+
| [Refresh] [Subscribe…] [Toggle Unread]          [MAIL] [FEED]    |
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
| **M0 — Window** | Menus, toolbar, stacked panes, preview toggle, stub article window, About. Done. |
| **M1 — One feed** | Add a URL; fetch; headlines fill; click shows body; double-click opens the article window. Done. |
| **M2 — Subscribe list** | Several feeds persist. Unread counts. Refresh all. Done. |
| **M3 — OPML** | Import / export. Done. |
| **M4 — HTML subset** | Links, bold, headings, lists; images in-pane; mp3/mp4 Play via the system handler. CDATA / `type=html` in the feed. Done. |
| **M5 — Polish** | Keys, last-selected feed, sash + preview-visible in ini, interval refresh while open. Done. |
| **M6 — Package** | `debian/`, `scripts/release.sh` → `.deb`, tarball, AppImage. Tag `v0.1.0`. **This tree.** |

Folders, search-all-feeds, full-text search, podcasts-as-a-product, **Mail mode**: after v1. Do not fetch the item’s HTML page to invent a body.

## Process

Do not commit to `master`. Every change lands through a pull request.

### Branches

- `feature/<short-name>` — new user-visible work
- `fix/<short-name>` — bugs, packaging nits, regressions

Open a pull request into `master`. Merge only after review.

### Gates

A pull request must pass **lint** before merge. CI runs `./scripts/lint.sh` (no `--fix`). Locally:

- `./scripts/lint.sh --fix` — clang-format rewrites `src/`
- `./scripts/lint.sh` — SPDX headers, no tabs, clang-format `--dry-run --Werror`, cppcheck (`warning`) on `src/`
- `meson compile` with this tree’s `warning_level=2` is clean (no new warnings)

Do not pass `--fix` in CI. Do not merge a red PR.

**Tests** are required when they exist (`meson test -C build`). Until a test suite lands, the gate is lint plus a clean compile plus a manual pass of the change.

### Semantic versioning

Every **shipped** pull request — merged to `master` and tagged as a release — bumps the version. `meson.build` is the source of truth. Keep these in lockstep in the same PR:

- `meson.build` `version:`
- `debian/changelog` (new stanza)
- git tag `vMAJOR.MINOR.PATCH` after merge

Then `./scripts/release.sh` produces `.deb`, tarball, and AppImage.

| Bump | When |
|---|---|
| **PATCH** (`x.y.Z`) | Bug fix or packaging. No new user-facing feature. |
| **MINOR** (`x.Y.0`) | New backward-compatible feature. |
| **MAJOR** (`X.0.0`) | Breaking change: native file format, dropped config keys, removed UI users rely on. |

While the version is `0.y.z`, still bump MINOR and PATCH this way. Do not treat 0.x as a free-for-all. The Debian revision (`-1`, `-2`) is only for rebuilding the same upstream version with no source change.
