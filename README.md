# Res Toggle

A tiny (~117 KB, single `.exe`, zero dependencies) Windows system-tray app that
toggles the primary display between two resolutions.

- **Left-click** the tray icon → switch between your **target** resolution and
  the resolution that was active when the app launched.
- **Right-click** → menu to pick a target resolution, toggle, read About, or quit.

The tray icon is **dim** while on the launch resolution and **bright** while on
the target, so you can tell the state at a glance. The chosen target is saved to
`res_toggle.ini` (next to the exe) and remembered across restarts.

## Build

Requires Visual Studio 2022 with the C++ toolchain (already detected on this
machine). From a normal command prompt in this folder:

```
build.bat
```

That produces `res_toggle.exe`. The batch file sets up the MSVC environment
itself, so you don't need the "x64 Native Tools" prompt.

## Use

1. Run `res_toggle.exe`. An icon appears in the system tray (check the `^`
   overflow area if you don't see it).
2. **Right-click → Set target resolution**, pick the one you want to switch to.
3. **Left-click** the icon any time to toggle between that target and your
   normal resolution.

### Start automatically with Windows

Press `Win+R`, type `shell:startup`, and drop a shortcut to `res_toggle.exe`
into the folder that opens.

## Files

| File               | Purpose                                             |
|--------------------|-----------------------------------------------------|
| `res_toggle.cpp`   | The whole app (Win32, ~300 lines).                  |
| `resource.h`       | Icon resource ids.                                  |
| `res_toggle.rc`    | Resource script referencing the two icons.          |
| `icon_base.ico`    | Dim monitor icon (shown on launch resolution).      |
| `icon_chosen.ico`  | Bright monitor icon (shown on target resolution).   |
| `build.bat`        | One-step build.                                     |
| `res_toggle.ini`   | Auto-created; stores your chosen target resolution. |

## Notes

- Affects the **primary** display only.
- Only distinct width×height modes the driver reports are offered; the app
  keeps the current refresh rate / color depth.
- Single-instance: launching it twice just no-ops the second one.
