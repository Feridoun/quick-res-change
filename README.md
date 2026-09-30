# Res Toggle

A tiny (~117 KB, single `.exe`, zero dependencies) Windows system-tray app that
switches the primary display between your usual resolution, a secondary one and
an optional third.

- **Left-click** the tray icon → cycle usual → secondary → (third) → usual.
- **Right-click** → Switch now, Settings…, About, or Quit.

**Settings…** has a drop-down for the **usual** and **secondary** resolutions,
plus a checkbox and drop-down for an optional **third**. The third is only
cycled through while the box is ticked. Your picks are kept even while it's
unticked.

The tray icon is **dim** while on the usual resolution and **bright** otherwise,
so you can tell the state at a glance. Settings are saved to `res_toggle.ini`
(next to the exe) and remembered across restarts.

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
2. **Right-click → Settings…**. Choose your usual and secondary resolutions,
   and optionally tick **Third resolution** and choose one.
3. **Left-click** the icon any time to cycle through them.

### Start automatically with Windows

Press `Win+R`, type `shell:startup`, and drop a shortcut to `res_toggle.exe`
into the folder that opens.

## Files

| File               | Purpose                                             |
|--------------------|-----------------------------------------------------|
| `res_toggle.cpp`   | The whole app (Win32, ~300 lines).                  |
| `resource.h`       | Icon and dialog resource ids.                       |
| `res_toggle.rc`    | Resource script: the two icons + settings dialog.   |
| `icon_base.ico`    | Dim monitor icon (shown on launch resolution).      |
| `icon_chosen.ico`  | Bright monitor icon (shown on target resolution).   |
| `build.bat`        | One-step build.                                     |
| `res_toggle.ini`   | Auto-created; stores your chosen resolutions.       |

## Notes

- Affects the **primary** display only.
- Only distinct width×height modes the driver reports are offered; the app
  keeps the current refresh rate / color depth.
- Single-instance: launching it twice just no-ops the second one.
