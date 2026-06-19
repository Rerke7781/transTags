# transTags Linux Qt

C++/Qt Widgets version of transTags for Ubuntu.

The settings window and tray menu are built with Qt Widgets. Window control uses X11/XFixes/EWMH, so the full feature set requires an `Ubuntu on Xorg` session.

## Build

```bash
cd transTags_linux/qt
chmod +x install_qt_deps_ubuntu.sh build.sh run.sh install_desktop_entry.sh
./install_qt_deps_ubuntu.sh
./build.sh
./install_desktop_entry.sh
```

## Run

```bash
./run.sh
```

Or directly:

```bash
./build/transTags_linux
```

After running `./install_desktop_entry.sh`, you can also launch `transTags Linux` from the applications menu.

## Hotkeys

- `Alt + Left`: make the active window more transparent and enable click-through.
- `Alt + Right`: make the active window less transparent; fully opaque windows stop click-through.
- `Alt + Up`: toggle click-through.
- `Alt + Down`: lock/unlock always-on-top.
- `Ctrl + Numpad 5`: center the active window.

## Ubuntu Session Note

Ubuntu's default Wayland session does not allow ordinary Qt programs to globally control arbitrary native windows. Use `Ubuntu on Xorg` for full functionality.
