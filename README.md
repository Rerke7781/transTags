# transTags

transTags is a small desktop utility for making a target window transparent, click-through, always-on-top, or centered with global hotkeys.

The project is split by operating system:

- `transTags_windows`: Windows version, implemented with native Win32 APIs.
- `transTags_linux`: Ubuntu/Linux version, implemented mainly with C++/Qt Widgets plus X11/XFixes/EWMH.

## Features

- Adjust the active window opacity.
- Toggle mouse click-through so clicks pass to windows below.
- Lock or unlock a selected window as always-on-top.
- Center the active window.
- Show a small bottom-right toast after each operation.

## Implementation Principle

transTags does not inject code into other programs. It uses the native window APIs provided by each desktop system:

- On Windows, it registers global hotkeys with Win32, finds the target window, changes layered-window opacity, toggles `WS_EX_TRANSPARENT` for mouse click-through, and uses topmost window flags for pinning.
- On Ubuntu/Linux, the Qt app provides the tray and settings UI, while X11/XFixes/EWMH are used to change opacity, input shape, active-window state, and always-on-top state.

## Hotkeys

| Hotkey | Action |
| --- | --- |
| `Alt + Left` | Make the target window more transparent and enable click-through. |
| `Alt + Right` | Make the target window less transparent. |
| `Alt + Up` | Toggle click-through. If a click-through window exists, unlock it; otherwise enable click-through on the current target. |
| `Alt + Down` | Toggle always-on-top. Click-through windows are prioritized. |
| `Ctrl + Numpad 5` | Center the target window. |

## Directory Layout

```text
transTags/
|-- transTags_windows/
|   |-- source/
|   `-- release/
|-- transTags_linux/
|   |-- qt/
|   |-- legacy/
|   `-- release/
`-- docs/
```

See `docs/FILE_STRUCTURE.md` for the full structure.

Chinese documentation: `README_CN.md`.

## Build: Windows

Open the Visual Studio solution:

```text
transTags_windows/source/transTags_windows.sln
```

Then choose `Release` and build the solution.

## Build: Ubuntu/Linux

The recommended Linux version is the Qt version:

```bash
cd transTags_linux/qt
chmod +x install_qt_deps_ubuntu.sh build.sh run.sh install_desktop_entry.sh
./install_qt_deps_ubuntu.sh
./build.sh
./install_desktop_entry.sh
./run.sh
```

Full Linux window control requires an `Ubuntu on Xorg` session. Ubuntu's default Wayland session does not allow ordinary Qt programs to reliably control arbitrary native windows.

## Release Artifacts

Local release files are stored under:

- `transTags_windows/release/`
- `transTags_linux/release/`

They are ignored by git by default. Upload them through GitHub Releases instead of committing them to the source repository. The Windows release has one executable file, `transTags_windows.exe`; the zip package only bundles that executable with its default config file.

## License

This project is released under the MIT License. See `LICENSE` for details.
