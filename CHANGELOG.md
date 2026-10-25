# Changelog

## 0.1.0

First release: the Windows 95 look for Windows 3.1 and Windows for Workgroups 3.11.

* Title bars and window frames: Windows 95 caption buttons (minimize, maximize/restore, close), icon, bold
  left-aligned title, active and inactive states.
* Dialogs: grey background, Windows 95 push buttons, check boxes and radio buttons, etched group boxes, sunken
  edit and list boxes, drop-down list combo boxes, message boxes with MS Sans Serif.
* Menus: grey menu bar and popup menus with 3D border, highlight bar, etched separators, system menu.
* Scroll bars: window, list box and scroll bar controls with Windows 95 arrows, thumb and track.
* Themes: `WIN95` (default), `WIN98` (gradient title bars), `SLATE` (example of a bitmap theme); themes are INI
  files, see `docs/THEME_FORMAT.md`.
* Safety: the engine removes its patches before Windows shows a system error box; closing the Retheme16 icon
  restores the Windows 3.1 look.
* Setup: graphical installer ([Win16Setup](https://github.com/ceisserer/Win16Setup)) with theme preview,
  `WIN.INI` autostart, Program Manager group, silent install (`SETUP /S`) and removal (`SETUP /U`).

Tested in QEMU on Windows 3.1 (US English, German, debug kernel), Windows for Workgroups 3.11, VGA 16 colours and
VBE SVGA 256 colours. Not yet tested: real hardware, other display drivers, large fonts, Standard mode.

Known gaps: see `docs/STATUS.md` (controls outside dialogs, simple and drop-down combo boxes, MDI children, the
desktop and icon titles keep the Windows 3.1 look).
