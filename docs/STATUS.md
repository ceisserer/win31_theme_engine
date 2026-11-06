# Status

Last update: 2026-11-04.

## Verified on Windows (QEMU)

Configurations:

| Windows | USER.EXE | Display | Golden set |
|---|---|---|---|
| Windows 3.1 (3.10.103), US English, retail and SDK debug build | 264 016 bytes | VGA 640×480×16 | `tests/emu/golden/win31` |
| Windows 3.1, German | 264 832 bytes | VGA 640×480×16 | `tests/emu/golden/win31de` |
| Windows 3.1 + VBE SVGA driver (vbesvga.drv) | 264 016 bytes | 640×480×256 | spot checks |
| Windows 3.1 + VBE SVGA driver (vbesvga.drv 1.0 beta 4), theme `WINXP` | 264 016 bytes | 800×600×65536 (16 bit) | `tests/emu/golden/win31hc-xp` |
| Windows for Workgroups 3.11 (pre-installed) | 3.11 | VGA 640×480×16 | spot checks |

All in 386 Enhanced mode. Scenarios in `tools/emu/scenarios` (`caption`, `controls`, `menus`, `menuhover`, `menukeys`, `scroll`) run
pixel-identical against
their golden screenshots (`tools/emu/regress.sh`). Verified:

* **Captions and frames:** Program Manager, GALLERY and all six GALLERY caption styles get Win95 captions (icon, bold
  left-aligned title, min/max/close following the Win95 rules, disabled buttons); activation changes, maximize,
  restore, minimize, moving by the caption, the theme's close button.
* **Dialogs:** grey background (`WM_CTLCOLOR`), Win95 push buttons (default frame, pressed, focus), check boxes and
  radio buttons in all states, embossed disabled text, etched group boxes, sunken 3D borders around edit and list
  boxes; message boxes with the System font swapped for MS Sans Serif; the bold font of dialog templates (`FONT`
  statement) replaced by a regular copy, as in Windows 95.
* **Menus:** grey Win95 menu bar and popup menus with 3D border, highlight bar, MS Sans Serif text (USER measures
  with that font too, so widths and mnemonic underlines fit), embossed disabled items, etched separators, theme
  check marks and sub-menu arrows, the system menu; unloading restores the 3.1 menus. The selection follows the
  mouse without a button held, as in Windows 95 (USER 3.1 tracks the mouse only after a button press: the first
  mouse move over an open menu is handed to it as one), also in `TrackPopupMenu` popups such as NewShell16's Start
  menu. The keyboard keeps working after that (USER ignores keys while it thinks the button is held: a key first
  releases that press on a spot where a release changes nothing, then reaches USER).
* **Scroll bars and combo boxes:** window scroll bars (top-level windows, list boxes, child windows of themed windows with and
  without a border), scroll bar controls and
  drop-down list combo boxes have Win95 arrow buttons (pressed and released), dither track and raised thumb;
  also while scrolling (`SetScrollPos`, tracking loops).
* **Safety:** the `SysErrorBox` emergency path (an empty floppy drive triggers the system error box: all patches are
  removed before it shows); unloading restores the 3.1 look and the original fonts; the debug kernel reports no
  errors for the engine.
* **Setup:** `SETUP.EXE` from the release floppy image: interactive install with theme preview, silent install
  (`/S`), autostart from `WIN.INI` `load=` after a reboot, Program Manager group, change of options and removal
  (button, `/U`, `/U /S` from the installed copy) leave no files or `WIN.INI` entries behind
  (`tools/emu/test_setup.py`).
* **Themes:** `SLATE` (bitmaps with colour key, close button on the left, theme dialog colour) works at 16 and 256
  colours. Colours outside the system palette are dithered by GDI (expected; documented for theme authors).
* **RedmondExperience (`WINXP`):** the Windows XP "Luna" look at 800×600 with 65536 colours: blue caption with
  rounded top corners (they take the desktop colour), round caption buttons with the red close button, rounded
  push buttons with the blue glow of the default button, green check marks and radio dots, light blue scroll bars
  and combo buttons, white menus with a grey border, the Luna colour scheme as system colours. Also runs at 256 and
  16 colours (nearest colours; the theme's own literal colours are dithered there). With NewShell16 as the shell the
  taskbar and Start menu follow (`[Shell] Style=XP`).
* **Redmond-11 (`WIN11`):** the light Windows 11 look at 800×600 with 65536 colours (Windows 3.1, VBE driver,
  GALLERY dialogs): light caption with dark text and rounded corners, flat caption buttons, rounded push buttons
  with the blue default button and its white label, blue check boxes and radio buttons, thin scroll bar thumbs,
  light menus with a rounded highlight, the Windows 11 light colours as system colours. No emulator golden set
  yet (spot check, `docs/images/win11_gallery.png`).
* **System colours on 256-colour displays:** `[SysColors]` are set to the nearest solid colours with 256 colours
  too (before: only with 16), so the Windows ME and XP schemes are not dithered.

## Implemented

| Area | Files | Verified |
|---|---|---|
| Theme format v1: INI parser, colours (named / `Sys:` / literal), fonts, recolouring, `Base=` inheritance, state fallback | `engine/theme/` | host tests |
| Renderer: `Fill`, `Dither`, `Gradient`, `Edge` (Win95 `DrawEdge` presets), `Border`, `Glyph`, `Bitmap` (stretch / tile / nine-grid / centre, colour key), `Text` (prefix `&`, shadow/emboss), `Focus`, `Layers` | `engine/render/` | host tests, golden images |
| Built-in glyphs (caption buttons, arrows, check marks, radio parts, size grip), original pixel art | `engine/render/glyphs.c` | golden images |
| Theme layout of the non-client area (Win95 button rules, configurable layout) | `engine/geom/nclayout.c` | host tests |
| USER 3.1 caption model (measured: bar `SM_CYSIZE` high, boxes `SM_CXSIZE+1` wide, dialog frame `SM_CXDLGFRAME` + border, menu bar `SM_CYMENU` + line) and theme→USER point mapping | `engine/geom/nclayout.c`, `engine/win16/ncpaint.c` | Windows 3.1 (US, German), WfW 3.11 |
| Default theme `WIN95` "Redmond 95" (procedural only); `WIN98` "Redmond 98" and `WINME` "Redmond ME" (derived; `WINME` sets the system colours, `[SysColors]`) and `SLATE` (original, bitmaps) | `themes/`, `tools/gen_slate.py` | previews, golden images, Windows |
| `WINXP` "RedmondExperience": Windows XP Luna look, self-contained (no `Base=`, one theme in memory), 6 24-bpp bitmaps (about 31 KB of files) drawn by a generator, `[Shell] Style=XP` for NewShell16 | `themes/WINXP`, `tools/gen_winxp.py` | previews, golden images, Windows 3.1 at 16 bit |
| `WIN11` "Redmond-11": Windows 11 light look, self-contained, 5 24-bpp bitmaps (about 34 KB of files) drawn by a generator; `PushButtonText.Default` for the white label of the accent-coloured default button | `themes/WIN11`, `tools/gen_win11.py` | previews, golden images (with the theme's `[SysColors]`), Windows 3.1 at 16 bit |
| Host tools: software backend, PNG writer, previewer, test suite | `host/` | — |
| Win16 runtime: patch engine (alias selectors, JMP FAR, arm/disarm per reason, suspend/resume, permanent patches), hooks, one-shot subclassing with post-processing of the result, per-window state, log ring buffer, trace levels 1/2 | `engine/win16/` | Windows |
| Interceptors: `PatBlt`, `BitBlt`, `StretchBlt`, `ExtTextOut`, `TextOut`, `Rectangle`, `LineTo`, `Polyline`, `SetPixel`, `FastWindowFrame`, `GetTextExtent`, `GetTextExtentPoint`, `PtInRect`, `SysErrorBox`, `SetScrollPos`, `SetScrollRange`, `EnableScrollBar` | `engine/win16/intercept.c`, `scroll.c` | Windows |
| Non-client theming (suppress and overpaint), hit-test remapping, own close button, min/max tracking | `engine/win16/ncpaint.c` | Windows |
| Controls in dialogs: push / check / radio / group boxes, static text, edit and list box borders, drop-down list combo boxes, `WM_CTLCOLOR`, font swap | `engine/win16/controls.c` | Windows |
| Menus: menu bar, popups, system menu; `GetTextExtent[Point]` patched while USER lays out menus | `engine/win16/menus.c` | Windows |
| Scroll bars: window scroll bars, list box scroll bars, scroll bar controls; `SetScrollPos`, `SetScrollRange`, `EnableScrollBar` patched (USER repaints inside them) | `engine/win16/scroll.c` | Windows |
| Loader `RETHEME.EXE`, theme switcher `THEMES.EXE`, test program `GALLERY.EXE` | `loader/`, `switcher/`, `testapp/` | Windows |
| Emulator tooling: DOS disk builder, unattended Windows 3.1 install (any language), debug-kernel image, VBE SVGA image, scripted runs, golden comparison | `tools/emu/` | used for all Windows tests |
| Installer: [Win16Setup](https://github.com/ceisserer/Win16Setup) (submodule; blue setup screen, theme preview, progress bar, `RETHEME.INI`, `WIN.INI` `load=`, Program Manager group over DDE, silent install, removal), configured by `dist/SETUP.INF` | `external/win16setup`, `dist/` | Windows 3.1 |
| Release packages: ZIP, 1.44 MB floppy image, checksums, release notes (reproducible); tag-triggered GitHub release | `tools/release/`, `.github/workflows/build.yml` | locally |
| Build: `make host/test/preview/win16/release`, GitHub Actions (rolling `snapshot` pre-release) | `Makefile`, `win16.mk`, `.github/` | locally |

## Known gaps

* Controls are themed only inside dialogs (`#32770`); controls in ordinary windows keep the 3.1 look (window
  scroll bars of themed windows and their child windows are themed).
* Combo boxes: only drop-down lists (`CBS_DROPDOWNLIST`); simple and drop-down (edit) combo boxes keep the 3.1 look.
* Not yet themed: MDI children, the desktop, icon titles, the task list.
* A disabled menu item that is selected is embossed on the highlight bar (Win95 draws it flat grey).
* Push button text always uses the theme's `Controls` font, even if the application set another font.
* Owner-draw buttons and controls of other toolkits (VB, BWCC, CTL3D-subclassed) are left alone; no class lists yet.
* Nested non-client painting of two windows at once is only handled for menu measuring (the MDI frame repainting
  its active child's caption).
* Permanent patches still missing from the plan: hook-API patches, `CallWindowProc`, `DeleteObject`, `RemoveProp`.
* Calibration (plan 4.3), trace replay on the host, `PROFILER.EXE`: not started.
* Not yet tested: WfW 3.11 German, large fonts, Standard mode, real application matrix (VB3, Office 4.3 ...).
* `WINXP`: the caption keeps the height USER gives it (Windows XP's is taller); no hot-tracking (buttons do not
  light up under the mouse); the rounded frame corners are painted in the desktop colour, so they show over a
  wallpaper or another window; it is made for 15/16/24-bit displays.
* `WIN11`: as `WINXP` (caption height, no hot-tracking, corners in the desktop colour); the frame is USER's
  thick sizing frame, not Windows 11's 1-pixel border; popup menus are square; fonts are Arial and MS Sans Serif
  (no Segoe UI on Windows 3.1). With NewShell16 as the shell the taskbar and Start menu follow (`[Shell] Style=11`).

## Next steps

1. Controls outside dialogs, the remaining combo box styles, MDI children.
2. Desktop and icon titles (plan phase 6).
3. Class lists and `CallWindowProc` superclass detection for VB / BWCC / CTL3D.
4. Application matrix once test applications are available (B.3 in `REQUIRED_TOOLS.md`).
