# transTags Linux

Ubuntu/Linux versions of transTags.

The recommended version is `qt/`, implemented with C++/Qt Widgets. The `legacy/` directory keeps older experiments for reference.

## Qt Version (Recommended)

```bash
cd transTags_linux/qt
chmod +x install_qt_deps_ubuntu.sh build.sh run.sh install_desktop_entry.sh
./install_qt_deps_ubuntu.sh
./build.sh
./install_desktop_entry.sh
./run.sh
```

The Qt version requires `Ubuntu on Xorg` for full-window control. On Ubuntu's default Wayland session, a normal Qt program cannot reliably control arbitrary native windows.

## Legacy X11 C Version

This is kept for reference only.

```bash
cd transTags_linux/legacy/x11-c
chmod +x install_deps_ubuntu.sh run.sh
./install_deps_ubuntu.sh
make
./run.sh
```

## Legacy GNOME Shell Extension

This is kept for reference and Wayland experiments.

```bash
cd transTags_linux/legacy/gnome-extension
chmod +x install_gnome_extension.sh
./install_gnome_extension.sh
```

Disable:

```bash
gnome-extensions disable transtags@sim.local
```

## Hotkeys

- `Alt + Left`: make the active window more transparent and enable click-through.
- `Alt + Right`: make the active window less transparent.
- `Alt + Up`: toggle click-through.
- `Alt + Down`: lock/unlock always-on-top.
- `Ctrl + Numpad 5`: center the active window.
