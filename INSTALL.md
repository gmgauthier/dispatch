# Installing Dispatch

Four ways to get a binary, in the order LCOS cares about:

| Artifact | Who it is for |
|---|---|
| **`.deb`** | LCOS, Devuan Excalibur, Debian Trixie. Preferred. |
| **Source tarball** | Distro packagers and `meson setup && ninja install`. |
| **AppImage** | Fallback for distros that do not install `.deb` files. gtkmm only. Published on the GitHub/Gitea release. |
| **Git build** | Developers. See below. |

Version comes from `meson.build` (currently `0.1.0`).

## Runtime needs

- GTK 3 / gtkmm-3.0
- libxml2, libsoup-3.0, **glib-networking** (HTTPS), fontconfig

On Debian / Devuan / LCOS:

```
sudo apt install libgtkmm-3.0-1t64 libsoup-3.0-0 libxml2 glib-networking libfontconfig1
```

(Package names on older Debian may be `libgtkmm-3.0-1v5`.)

## 1. Debian package (preferred)

From a release `.deb`:

```
sudo apt install ./dist/dispatch_0.1.0-1_amd64.deb
```

Or, from this tree:

```
./scripts/release.sh deb
sudo apt install ./dist/dispatch_0.1.0-1_amd64.deb
```

That installs:

- `/usr/bin/dispatch`
- `/usr/share/applications/dispatch.desktop`
- `/usr/share/icons/hicolor/scalable/apps/dispatch.svg`
- `/usr/share/dispatch/skin/lcos/lcos.css`
- `/usr/share/dispatch/brand/icon-tile.svg`

Launch from the menu or `dispatch`. Config is `~/.config/dispatch/dispatch.ini` (created on first Subscribe).

Uninstall: `sudo apt remove dispatch`.

## 2. Source tarball

`meson dist` produces `build/meson-dist/dispatch-VERSION.tar.xz`.

```
tar -xf dispatch-0.1.0.tar.xz
cd dispatch-0.1.0
sudo apt install build-essential meson ninja-build pkg-config \
  libgtkmm-3.0-dev libxml2-dev libsoup-3.0-dev libfontconfig1-dev
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

`./scripts/release.sh tarball` runs `meson dist` for you.

## 3. AppImage (fallback)

LCOS 0.3 already runs AppImages. The image bundles gtkmm from the build host and the GIO GnuTLS module for HTTPS.

```
./scripts/release.sh appimage
```

Requires `linuxdeploy` on `$PATH` (see <https://github.com/linuxdeploy/linuxdeploy>). Output lands under `dist/`.

```
chmod +x Dispatch-*.AppImage dispatch-*.AppImage
./Dispatch-*.AppImage
```

The AppImage runtime sets `APPDIR`; Dispatch looks for skin and brand under `$APPDIR/usr/share/dispatch`. Leave `APPDIR` unset for `.deb` and `meson install` builds.

## 4. Developer build (no install)

```
meson setup build
meson compile -C build
./build/dispatch
```

The binary finds CSS via `SOURCE_ROOT` in the build tree. `DISPATCH_DATA` overrides that.

## One command for every artifact

```
./scripts/release.sh all
```

Writes tarball, `.deb`, and AppImage (if `linuxdeploy` is there) under `dist/`. The GitHub/Gitea release includes the AppImage as the non-deb fallback.

## What this project will not ship

- A feed server (Miniflux, tt-rss)
- WebKit / a landing-page scraper
- Podcasts as a product identity
- A systemd unit
- IMAP/SMTP in v1 (Mail mode is later)
