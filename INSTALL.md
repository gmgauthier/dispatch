# Installing Dispatch

v1 packaging (`.deb`, tarball, AppImage) is **M6**. Until then, build from git.

## Runtime needs

- GTK 3 / gtkmm-3.0

On Debian / Devuan / LCOS:

```
sudo apt install libgtkmm-3.0-1t64
```

(Package names on older Debian may be `libgtkmm-3.0-1v5`.)

## Git build

```
sudo apt install build-essential meson ninja-build pkg-config g++ libgtkmm-3.0-dev
meson setup build
meson compile -C build
./build/dispatch
```

Prefix install:

```
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

That installs:

- `/usr/bin/dispatch`
- `/usr/share/applications/dispatch.desktop`
- `/usr/share/icons/hicolor/scalable/apps/dispatch.svg`
- `/usr/share/dispatch/skin/lcos/lcos.css`
- `/usr/share/dispatch/brand/icon-tile.svg`
