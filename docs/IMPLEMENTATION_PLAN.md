# Implementation plan: an open-source, themeable Win95-look engine for Windows 3.x

Working name: **Retheme16** (placeholder, rename freely).

Goal:

1. Reproduce the Windows 95 look on Windows 3.1 and WfW 3.11 (protected mode only; Windows 3.0 and real mode are
   out of scope) as faithfully as possible:
   title bars, caption buttons, frames, 3D controls, grey dialogs, menus, scrollbars, desktop, fonts.
2. The look is **not hard-coded**. It comes from a **theme** (an INI file plus bitmaps),
   so users can make and share their own. The Win95 look is just the default theme.

---

## 1. Guiding decisions

| # | Decision | Why |
|---|---|---|
| D1 | **System-wide hooks, one-shot subclassing and GDI/USER code patches** that are armed only while USER paints something we restyle. | A subclass-only design leaves menus, USER's tracking loops and out-of-`WM_PAINT` repaints in the 3.1 look. GDI interception catches every route and keeps USER's behaviour. |
| D2 | **Split "recognising" from "rendering".** Interceptors never draw. They turn USER's GDI call into a semantic *draw request* (`part`, `state`, `rect`, `hwnd`, `hdc`), and the theme renderer draws it. | This split is the core design decision. It makes themes possible and lets the renderer be tested on Linux. |
| D3 | **The Win95 look is a theme ("Classic95"), not code.** It is built from procedural primitives (3D edges, vector glyphs, fills, text). | Proves the theme system is complete. Needs no Microsoft artwork. Scales to any button size (EGA, VGA, 8514 "large fonts"). |
| D4 | **USER geometry is data, per USER build** ("USER profile"), plus an optional run-time **self-calibration**. | Version drift (3.1 / WfW / localised builds) is the biggest correctness risk. |
| D5 | **A geometry transform layer** maps between USER's layout and the theme's layout, in both directions. | Lets themes move buttons around and, in a later phase, change caption height. |
| D6 | **Portable C core.** The parser, renderer, geometry and recogniser are plain C89 behind a small backend interface. They are built twice: Win16 (Open Watcom) and Linux (gcc) for unit tests, trace replay and theme previews. | Most bugs get caught without booting an emulator. Theme authors get a previewer. |
| D7 | **Original artwork only.** All artwork is procedural or created for this project; no artwork is copied from Microsoft. | Lets the project use a free licence safely. |
| D8 | **Fail safe.** Every interceptor falls back to "call the original", per-app and per-class opt-outs exist, `SysErrorBox` removes all patches, and unloading the engine always works. | A crash inside GDI takes down the whole system. |

---

## 2. Architecture

```
            ┌──────────────────────── THEMES.CPL (Control Panel applet) ──────────────────────┐
            │  pick / preview / apply theme, per-app opt-outs, unload                          │
            └──────────────┬───────────────────────────────────────────────────────────────────┘
                           │ exports (ThemeApply, ThemeUnload, ...)
 RETHEME.EXE (loader) ─────┤  loads DLL, keeps it resident, owns the log file, runs timers
                           ▼
 ┌──────────────────────────────── RETHEME.DLL ────────────────────────────────────────────────┐
 │                                                                                             │
 │  RUNTIME (Win16 only)                                                                       │
 │   patch.c      alias selectors, JMP FAR, unpatch/call/repatch, armed ref-counts             │
 │   hook.c       WH_CALLWNDPROC / WH_GETMESSAGE (+ hook-API patches)                          │
 │   dispatch.c   per-message logic, one-shot subclass, message rewriting                      │
 │   classify.c   class DB (built-in + INI), per-hwnd cache, superclass detection              │
 │   wstate.c     per-window state (layout cache, original font, flags)                        │
 │   fault.c      TOOLHELP fault handler, SysErrorBox emergency path, safe unload              │
 │   log.c        ring buffer in DGROUP → flushed to a file by the loader                      │
 │                                                                                             │
 │  INTERCEPTORS (Win16 only)  PatBlt BitBlt TextOut ExtTextOut StretchBlt FillRect Rectangle  │
 │   DrawText FastWindowFrame GetTextExtent[Point] SetDCOrg DefWindowProc PtInRect             │
 │   SetCapture ReleaseCapture GetWindowDC DeleteObject RemoveProp CallWindowProc MessageBox   │
 │   SysErrorBox Set/UnhookWindowsHook[Ex] EnableScrollBar SetScrollPos SetScrollRange         │
 │            │  raw GDI call + paint context                                                  │
 │            ▼                                                                                │
 │  PORTABLE CORE (Win16 + Linux)                                                              │
 │   recog/      call + context  → DrawRequest{part, state, rect, flags}  or PASSTHROUGH       │
 │   geom/       USER profiles (3.1/WfW/localised), calibration, NC layout, layout transform   │
 │   theme/      INI/T16 loader, part table, inheritance, colour/metric/font sets              │
 │   render/     ops: Fill Gradient Edge Glyph Bitmap(stretch/tile/9-grid, colour key) Text    │
 │            │                                                                                │
 │            ▼  backend interface (≈15 functions)                                             │
 │   backend/gdi16.c  (Win16 GDI)          host/backend_sw.c (software raster → PNG, Linux)    │
 └─────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 The DrawRequest contract

```c
typedef struct {
    WORD   part;     /* PART_CAPTION, PART_CAPTIONBTN_CLOSE, PART_MENUITEM, PART_PUSHBUTTON ... */
    WORD   state;    /* ST_NORMAL, ST_PRESSED, ST_DISABLED, ST_ACTIVE, ST_INACTIVE, ST_CHECKED, ... */
    WORD   flags;    /* RQ_FOCUS, RQ_DEFAULT, RQ_MAXIMIZED, RQ_MDICHILD, RQ_DIALOG ... */
    RECT   rc;       /* target rectangle, already in THEME layout */
    HWND   hwnd;
    HDC    hdc;
    LPCSTR text;  int cch;   /* for text parts */
} DRAWREQ;

/* recogniser result */
enum { RC_PASSTHROUGH, RC_REPLACED, RC_REPLACED_AND_CALL_ORIGINAL /* e.g. widened fill */ };
```

Each interceptor does: *read paint context → `recog_xxx()` → either call the original, or `render_part(&req)`*.
The recognisers hold all the knowledge of USER's drawing habits (ROP, brush colour, exact rectangle,
OBM strip offset). The renderer holds all the knowledge of how things look. Neither knows the other's internals.

### 2.2 Parts and states (theme vocabulary)

| Group | Parts | States |
|---|---|---|
| Window frame | `Frame.Top/Bottom/Left/Right`, corners; variants `Sizing`, `Dialog`, `Thin` | Active, Inactive |
| Caption | `Caption` (background), `Caption.Text`, `Caption.Icon` | Active, Inactive; flags Maximized, MDIChild, Dialog |
| Caption buttons | `Btn.Close`, `Btn.Min`, `Btn.Max`, `Btn.Restore` (+ optional `Btn.Help` later) | Normal, Pressed, Disabled, (Hot, optional) |
| Menu bar | `MenuBar`, `MenuBar.Item` | Normal, Selected, Disabled |
| Popup menu | `Menu.Border`, `Menu.Back`, `Menu.Item`, `Menu.Separator`, `Menu.Check`, `Menu.Radio`, `Menu.SubArrow` | Normal, Selected, Disabled |
| Buttons | `PushButton`, `CheckBox`, `Radio`, `GroupBox` | Normal, Pressed, Disabled, Default, Focus (overlay), Checked, Mixed |
| Edit-like | `Edit.Border`, `ListBox.Border`, `Combo.Border`, `Combo.Button` | Normal, Disabled, Pressed |
| Scrollbar | `Scroll.ArrowUp/Down/Left/Right`, `Scroll.Thumb[H/V]`, `Scroll.Track[H/V]`, `Scroll.SizeBox` | Normal, Pressed, Disabled |
| Dialog / static | `Dialog.Back`, `Static.Text` (disabled = embossed), `Static.Frame/Rect` | Normal, Disabled |
| Desktop | `Desktop` (fill/gradient), `IconTitle` | Normal, Selected |
| Misc | `MDIClient.Back`, `TaskSwitch` (Alt+Tab box), `MsgBox.Icon.*` (later) | – |

Every part has a fallback chain: *theme state → theme Normal state → `Base=` theme → built-in Classic95 → passthrough*.

---

## 3. Theme format

### 3.1 On disk

A theme is a directory with **8.3 names** (Win 3.x limitation), for example `C:\RETHEME\THEMES\WIN95\`:

```
THEME.INI        definition
*.BMP            Windows 3.0 DIBs (1/4/8/24 bpp), optional
```

For distribution, the host tool `themec` packs the directory into one **`.T16`** file: a header, a pre-parsed part
table, and bitmaps pre-dithered to 4 bpp (VGA 16 colours) and 8 bpp (20 static colours + halftone). The engine
loads either form. A directory is meant for authoring (with a "reload theme" command); `.T16` is faster to load and
needs no INI parsing at boot.

### 3.2 `THEME.INI` (draft)

> This was the first draft. The implemented format is specified in [THEME_FORMAT.md](THEME_FORMAT.md).

```ini
[Theme]
Name=Redmond 95
Author=Retheme16 project
Version=1.0
Format=1                 ; theme-format version understood by the engine
Base=                    ; optional parent theme to inherit from
ColorMode=System         ; System = follow Control Panel colours; Fixed = use [Colors] only

[Metrics]                ; 0 / missing = keep USER's value
CaptionButtonWidth=16
CaptionButtonHeight=14
CaptionButtonGap=2       ; between Max and Close
CaptionIconSize=16
CaptionHeight=0          ; >0 needs phase 8 (variable metrics), otherwise ignored
BorderWidth=0            ; applied with SystemParametersInfo(SPI_SETBORDER), restored on unload
Layout=Icon,Title,Space,Min,Max,Gap,Close

[Colors]                 ; named colours. Sys:<name> refers to a COLOR_* entry
Face=Sys:BtnFace
Light=Sys:BtnHighlight
Shadow=Sys:BtnShadow
DkShadow=0,0,0
Title1=Sys:ActiveCaption
Title2=Sys:ActiveCaption ; = Title1 → plain fill, otherwise gradient (Win98 style)
ApplySysColors=0         ; 1 = call SetSysColors from [SysColors] (and restore on unload)

[Fonts]                  ; Face,PointSize,Weight[,Italic]
Caption=MS Sans Serif,8,700
Menu=MS Sans Serif,8,400
Controls=MS Sans Serif,8,400
IconTitle=MS Sans Serif,8,400
ReplaceSystemFont=1      ; swap System font for Controls font in dialogs

[Caption.Active]
Op=Gradient
From=Title1
To=Title2
Direction=Horizontal
Bands=32                 ; dithered on 16-colour displays

[Caption.Text.Active]
Op=Text
Font=Caption
Color=Sys:CaptionText
Align=Left
Offset=2,0

[Btn.Close.Normal]
Layers=BtnFace,CloseGlyph

[BtnFace]
Op=Edge
Edge=Raised              ; Raised | Sunken | Etched | Bump | Flat | RaisedOuter ...
Fill=Face

[CloseGlyph]
Op=Glyph
Glyph=Close              ; built-in vector glyph: Close Min Max Restore ArrowUp ... Check RadioDot
Color=Sys:BtnText

[PushButton.Normal]      ; a bitmap-skinned part
Op=Bitmap
Image=BUTTON.BMP
Source=0,0,24,22         ; sub-rectangle of the image (one state of a strip)
Sizing=NineGrid          ; Stretch | Tile | NineGrid | Center
Margins=4,4,4,4          ; left, top, right, bottom of the fixed border
Transparent=255,0,255    ; colour key, converted to an AND mask at load time
Recolor=System           ; placeholder colours → system colours
```

Rules:
* A section is one **op**. `Layers=` composes several ops, painted in order.
* State sections are named `<Part>.<State>`. A missing state falls back as described in 2.2.
* `Sys:<Name>` colours are re-resolved on `WM_SYSCOLORCHANGE`, so themes can follow Control Panel colours like Win95 does.
* **Placeholder recolouring**: a theme may declare `[Recolor]` key colours (e.g. `255,255,0=Face`) that are
  replaced at load time. That lets one bitmap set follow any colour scheme.
* Optional extras a theme may set (and the engine restores on unload): wallpaper (`SPI_SETDESKWALLPAPER`),
  sounds (`WIN.INI [sounds]`), border width, system colours.

### 3.3 Render ops

| Op | Parameters | Notes |
|---|---|---|
| `Fill` | colour | solid brush, shared brush cache |
| `Gradient` | from, to, direction (H, V, Concentric), bands | concentric = radial bands; integer maths, no FPU emulator |
| `Edge` | style, fill | Win95 `DrawEdge` semantics with Light / Face / Shadow / DkShadow (behaviour from Wine `uitools.c`, LGPL) |
| `Glyph` | built-in glyph name, colour, optional disabled emboss | drawn with `Polygon`/`PatBlt` at any size |
| `Bitmap` | image, source rect, sizing, margins, colour key, recolour | cached DDB + mask per colour depth |
| `Text` | font, colour, align, offset, shadow colour, emboss | used for captions, menus, disabled text |

### 3.4 Limits we document for theme authors

* **No shaped windows.** Win16 has no window regions, so rounded corners can only be painted, not made transparent.
* **Colour depth.** Most real 3.1 setups run 16 or 256 colours. Bitmaps are dithered by `themec`. The engine never
  realises its own palette (that would fight the foreground application's palette).
* **Hot (hover) states** are optional. Windows 3.x has no `WM_NCMOUSELEAVE`; hover needs a timer that polls the cursor.
* **Resources.** The GDI heap is 64 KB for the whole system. A theme gets a budget (for example at most ~40 GDI objects
  and ~150 KB of bitmap bits). `themec` warns, the engine refuses themes that go over it.
* **Metrics** other than button sizes and border width need phase 8 and are flagged experimental.

---

## 4. Key technical designs

### 4.1 Patch engine

* Install: `GetCodeHandle` → `GlobalPageLock` → `AllocSelector` + `PrestoChangoSelector` (writable alias) →
  save 5 bytes → write `EA off seg`.
* Calling the original: unpatch, call, repatch, guarded by a per-patch re-entrancy counter. If a handler is re-entered
  while unpatched, the nested call goes straight to the original.
* Arm/disarm with **reference counts per reason** (non-client, menu, control, scroll API ...), not per slot.
* Removing a patch checks the first byte is still ours (`EA`); if someone else patched on top, the engine leaves it
  and refuses to unload instead of corrupting their patch.
* All handlers and hook procedures live in **FIXED** code segments with `__loadds` (they run on the caller's DS and
  stack, `SS != DS`). Stack budget per handler: **< 512 bytes** (USER calls on the application's stack, and some have
  very small stacks). A debug build checks this with stack canaries.

### 4.2 Dispatcher and window state

* Classification is done once per window and cached as `{hwnd → class code, flags}` in a small open-addressing hash
  table in DGROUP, invalidated on `WM_NCDESTROY`.
* Per-window data (original font, layout cache, sizes) is in the same table, not in 15 window properties. We use one
  property only as a fallback for overflow. That also simplifies the `RemoveProp` protection.
* One-shot subclass: subclass, call the original procedure, remove the subclass,
  post-process. Post-processing covers `WM_CTLCOLOR` (BTNFACE brush), `WM_SETFONT` swap, caption decorations,
  `WM_NCCALCSIZE` (phase 8).

### 4.3 USER profiles and self-calibration (D4)

* A **USER profile** holds everything version-specific: caption/button/menu/scroll rectangle formulas, OBM strip
  offsets, which brush/ROP USER uses per element, which routines repaint outside `WM_PAINT`.
* Profiles are selected by `GetVersion`, USER.EXE file version, size and checksum. Unknown builds use the nearest
  profile and **calibrate**.
* **Calibration** (at startup, < 100 ms): create a window with known styles at an off-screen position, arm the
  interceptors in *record* mode, force `WM_NCPAINT`/`WM_NCACTIVATE`, and record which rectangles, brushes and ROPs
  USER uses. Compare with the profile's formulas and correct offsets. (Needs verifying that USER still issues the GDI
  calls for an off-screen window. If not, use a 1×1-pixel visible window behind the desktop icons.)
* The `profiler` test tool (section 6) dumps a new profile automatically on any Windows build we get our hands on.

### 4.4 Layout transform (D5)

For each themed window the engine keeps two layouts: **USER layout** (what USER thinks: caption rect, system box,
min/max boxes, menu bar) and **theme layout** (where the theme draws things). Functions map points and rectangles in
both directions:

* drawing: USER rect → theme rect (widened caption, moved buttons);
* input: theme point → USER point, written back into `WM_NCHITTEST`/`WM_NC*BUTTON*` messages and into `PtInRect`
  during USER's min/max tracking loop;
* buttons USER doesn't have (close) are handled by the engine's own hit code and capture loop.

Phase 8 extends this with **vertical deltas**: if a theme wants a taller caption, the engine grows the non-client area
by post-processing `WM_NCCALCSIZE`, shifts USER's menu-bar drawing and hit-testing down by the delta, and paints the
extra rows itself.

### 4.5 Fonts

MS Sans Serif variants at ¾ of the System font
height, a `WM_SETFONT` swap for controls that use the System font, and `GetTextExtent[Point]` patches so USER measures
menus, captions and icon titles with the font we draw. Dialog relayout after the font swap (only partly
understood) gets its own investigation task in phase 4. Fallback: keep the System font in dialogs (theme option
`ReplaceSystemFont=0`).

### 4.6 Logging and tracing

* Win16 file handles belong to a task, so a DLL running in some application's context can't keep a log file open.
  `log.c` writes into a ring buffer in DGROUP; the loader task flushes it to `RETHEME.LOG` on a timer.
* **Trace mode** records every intercepted call with its context and the recogniser's decision, in a compact binary
  format. Traces are the input of host-side replay tests (section 6.1).

### 4.7 Coexistence

* CTL3D/CTL3DV2, COMCTL16, BWCC, NAV3D, etc. draw their own 3D effects. We detect them by the **module that
  owns the window procedure** (TOOLHELP `GlobalEntryHandle` on the procedure's selector), not only by class name.
* Other shells (Norton Desktop, Calmira, PC Tools) and other hook/patch programs: the hook-API patches keep our hooks
  in the chain; the patch engine refuses to stack on a foreign `JMP`.
* Running under Windows 95/NT: refuse to load (`GetVersion` ≥ 3.95).

---

## 5. Repository layout

```
engine/           RETHEME.DLL (runtime + interceptors), Open Watcom wmake files
  core/           patch, hook, dispatch, classify, wstate, fault, log
  intercept/      one file per patched function
  recog/          portable: nc, menu, button, scroll, static, edit, desktop
  geom/           portable: profiles (user30, user31, wfw311, user31_de ...), calibrate, layout
  theme/          portable: ini parser, t16 reader, part table, colours, fonts
  render/         portable: ops, glyphs, gradient, ninegrid, text
  backend/        gdi16.c
loader/           RETHEME.EXE
cpl/              THEMES.CPL (Control Panel applet)
testapp/          GALLERY.EXE: every control/window type in fixed, scriptable states
                  PROFILER.EXE: dumps USER geometry, OBM sizes, metrics
host/             Linux build of the portable core: unit tests, trace replay, previewer
tools/            themec (Python): lint, pack, dither, preview
                  emu/: QEMU / DOSBox-X automation, screenshot diff
themes/           win95 (default), win98 (gradient caption), and original themes
docs/             this plan, theme-authoring guide, profile format, test matrix
```

Build: Open Watcom C/C++ 2.0 on Linux (`wcc -bt=windows -bd -ms/-ml -zu`, `wlink system windows_dll`), plus gcc for
`host/`. GitHub Actions builds both and runs the host tests on every push.

---

## 6. Test strategy

### 6.1 Level 1: host tests (Linux, CI, no Windows needed)

* Theme parser and fallback chains.
* Renderer: render every part/state of every bundled theme to PNG with the software backend and compare with golden
  images (pixel-exact).
* Geometry: layout formulas and transforms (property tests: `theme→user→theme` round trips, hit-test coverage).
* **Trace replay**: traces recorded in the emulator are fed to the recognisers; assertions check that each USER call is
  classified correctly. A regression in recognition shows up without booting Windows.

### 6.2 Level 2: emulator integration (local or self-hosted runner, because it needs licensed images)

* A prepared disk image boots DOS → Windows → `GALLERY.EXE /script:<scene>`, which creates the scene and writes a
  ready marker.
* The host takes screenshots (QEMU monitor `screendump`, DOSBox-X capture), injects input (`sendkey`, mouse events) and
  diffs against golden screenshots (3.1 + engine vs. real Win95 references, reviewed by a human once, then frozen).
* Build/deploy loop: DOSBox-X mounts the build directory directly; QEMU uses `-drive file=fat:rw:<dir>`; 86Box images
  are updated with `mtools`.

### 6.3 Level 3: stress and compatibility

* Soak: open/close applications in a loop, menus, drags, theme switching, unload/reload 1000×; watch
  `GetFreeSystemResources` for leaks and the debug kernel for parameter-validation errors.
* Application matrix (VB3, Word 6, Excel 5, Delphi 1, BC++ 4.5/OWL, MFC 2.5, Clarion, Program/File Manager, Write,
  Paintbrush, Control Panel, a Winsock browser).
* Display matrix: VGA 16, SVGA 256 (S3, Cirrus, ET4000), hi-colour, 8514/A large fonts, EGA, Hercules/CGA (best effort).
* Windows matrix: 3.1 and WfW 3.11 (Standard and 386 Enhanced mode), localised 3.1 and WfW (German).

---

## 7. Phases, deliverables and exit criteria

Estimates are for **one experienced Win16 developer**. Phases 2 and 7 (portable code) can run in parallel with the
emulator-bound phases.

| Phase | Content | Exit criteria | Effort |
|---|---|---|---|
| **0. Environment** | Toolchain on Linux; emulator images (3.1, German 3.1, WfW 3.11); Win95 reference screenshots from online sources; debug kernel; build/deploy loop; `GALLERY.EXE` and `PROFILER.EXE` v0; a reference screenshot set for every gallery scene. | One command builds, deploys and screenshots a gallery scene. | 3–4 wk |
| **1. Runtime skeleton** | Loader, DLL, hooks, dispatcher, classification cache, window state, patch engine with arm/disarm, fault handler, `SysErrorBox` emergency path, logging/tracing, safe unload. | Trace of a full Program Manager session; 1000× load/unload with no resource leak and no crash. | 6–8 wk |
| **2. Theme model + renderer** (portable) | INI/`.T16` loader, part table, fallbacks, all render ops, vector glyphs, GDI and software backends, `themec lint/pack/preview`, Classic95 theme v0. | Previewer renders a complete Win95 window mock from Classic95; host tests green in CI. | 4–6 wk |
| **3. Non-client area** | USER 3.1 profile + calibration; recognisers for caption fill, title text, OBM glyph blits, `FastWindowFrame`; layout transform; caption icon; Win95 button rules; hit-test rewriting; min/max tracking; own close button; MDI children and maximised MDI. | Every gallery window type matches the Win95 look; dragging, sizing, min/max/close, system menu, MDI all behave as on plain 3.1. | 8–12 wk |
| **4. Controls** | `WM_CTLCOLOR` grey; font swap + dialog relayout; push buttons (incl. `BM_SETSTATE` repaints); check/radio; group boxes; statics + embossed disabled text; edit/list/combo borders; combo button; scrollbars incl. `SetScrollPos`/`SetScrollRange`/`EnableScrollBar`; VB/BWCC/Clarion classes; CTL3D coexistence. | Gallery dialogs and the VB3/Delphi/BC++ test apps match the Win95 look; no visual "flicker back" to 3.1 on clicks/keys. | 6–10 wk |
| **5. Menus** | Menu bar and popups (`#32768`), selection bar, separators, check marks, sub-menu arrows, disabled emboss, `GetTextExtent` patch, keyboard and mouse menu tracking, system menu items for the engine. | Menus in Program Manager, Word 6 and Excel 5 match the Win95 look; item widths correct. | 4–6 wk |
| **6. Desktop & misc** | Desktop gradient + `WM_ICONERASEBKGND`, icon titles (`SetDCOrg`), Alt+Tab switcher, task list, message boxes. | **Milestone 1.0-preview: "Win95 look on 3.1"** implemented on 3.1 (VGA + SVGA 256). | 3–4 wk |
| **7. Theme engine 1.0** | All parts skinnable, bitmap 9-grid/tile/colour key/recolour, colour-depth variants, live theme switching (`RedrawWindow` with `RDW_FRAME`/`RDW_ALLCHILDREN` for every top-level window), restore of colours/wallpaper/sounds/border, `THEMES.CPL` with preview, resource budget checks, authoring guide, 3–4 sample themes. | Theme switching 100× without leaks; a third party can build a theme from the guide alone. | 6–10 wk |
| **8. Variable metrics** (experimental) | Caption height, button size/position from theme; `WM_NCCALCSIZE` post-processing; vertical layout deltas for menu bar and hit-testing; per-app opt-out for applications that compute non-client sizes themselves. | Sample theme with a 24 px caption works in the app matrix, or is disabled per app. | 6–10 wk |
| **9. Compatibility** | WfW 3.11 and localised profiles; display driver matrix; application matrix. | Test matrix (6.3) green or documented. | 6–12 wk |
| **10. Release** | Installer, uninstaller, documentation (text/HTML; WinHelp optional), theme gallery, bug tracker triage. | 1.0 release. | 3–4 wk |
| | **Total** | | **55–86 wk ≈ 13–20 months** |

Estimate: the faithful Win95 clone alone is 9–18 months; the theme layer, the portable
core and variable metrics add about 3–4 months, and the host test infrastructure pays some of that back by
shortening phases 3–5 and 9.

### 7.1 Order of risk retirement

The riskiest unknowns go first, as **spikes inside phase 0/1**, before investing in the rest:

1. Patch `PatBlt` on 3.1, recognise the caption fill, paint a gradient: proves D1/D2 (1 week).
2. Min/max tracking with `PtInRect` remapping and our own close button: proves D5 (1–2 weeks).
3. Calibration on an off-screen window: proves D4 (1 week).
4. Menu-bar text with the `GetTextExtent` patch: proves the menu approach (1 week).

If spike 3 fails, profiles are hand-written per build (more work in phase 9, no change elsewhere).

---

## 8. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Re-entrancy in unpatch/call/repatch | System crash | Per-patch nesting counter; handlers never yield; short arm windows; stress tests |
| Version drift of USER internals | Wrong recognition on some builds | Profiles + calibration + trace replay; testers with localised builds |
| False positives (app draws like USER) | Corrupted app drawing | Recognition requires paint context + exact rect + brush + ROP; per-app/per-class opt-outs |
| GDI/USER heap exhaustion (64 KB each) | "Out of memory" in other apps | Resource budget per theme; shared brushes; one memory DC; leak tests |
| Small application stacks | Stack overflow inside GDI | < 512 B per handler, no recursion, canaries in debug build |
| Display drivers with odd OBM sizes / accelerated `FastWindowFrame` | Missing or misplaced decorations | Calibration; driver matrix; passthrough fallback |
| Dialog relayout after font swap (only partly understood) | Clipped dialog text | Separate investigation; theme option to keep the System font |
| Variable metrics breaking apps | Layout glitches | Experimental flag, per-app opt-out, off by default |
| Legal (Microsoft artwork) | Project takedown | Original artwork only (D7); procedural Classic95 |

---

## 9. Decisions

| # | Question | Decision |
|---|---|---|
| 1 | Licence | open |
| 2 | Windows 3.0 / real mode on 8088 | **Dropped.** Targets are Windows 3.1 and WfW 3.11 in Standard and 386 Enhanced mode. |
| 3 | Variable metrics (phase 8) in 1.0? | open |
| 4 | Project name | open (working name Retheme16) |
| 5 | Win95 reference material | Screenshots from online sources, no Windows 95 installation. |

---

## 10. Status

See [STATUS.md](STATUS.md) for what is implemented and what was verified where.
