# Theme format (version 1)

A theme is a directory with an 8.3 name containing `THEME.INI` and, optionally, `.BMP` files with 8.3 names. The
default theme [`themes/WIN95`](../themes/WIN95/THEME.INI) is a complete, commented example that uses
procedural drawing only.

Check a theme by rendering previews on Linux:

```sh
make preview                                   # all themes in themes/ -> build/preview/<THEME>/*.png
build/host/preview themes/MYTHEME out/         # one theme; warnings are printed
```

## 1. Reserved sections

### `[Theme]`

| Key | Meaning |
|---|---|
| `Name` | Display name |
| `Author`, `Version` | Free text |
| `Format` | Format version the theme needs. Themes with a higher number than the engine supports are refused. Current: `1`. |
| `Base` | Parent theme (directory name). Every part the theme does not define is taken from the parent. Looked up next to the theme first, then in the installed `THEMES` directory. `WIN95` is a good base for partial themes. |
| `ColorMode` | `System` (default): `Sys:` colours follow the Control Panel. `Fixed`: same, reserved for themes that set their own colours. |

### `[Metrics]`

| Key | Default | Meaning |
|---|---|---|
| `CaptionButtonWidth`, `CaptionButtonHeight` | 16, 14 | Caption button size |
| `CaptionButtonGap` | 2 | Width of a `Gap` layout token |
| `CaptionIconSize` | 16 | Caption icon size |
| `TitlePadding` | 2 | Space between the title and its neighbours |
| `Layout` | `Icon,Title,Min,Max,Gap,Close` | Caption layout. Tokens before `Title` are placed left to right from the left edge, tokens after it right to left from the right edge. Tokens: `Icon`, `Title`, `Min`, `Max`, `Close`, `Gap`. |
| `CaptionHeight`, `BorderWidth` | 0 | Reserved (plan phase 8); ignored by format 1 |

Win95 rules the engine applies regardless of the layout: min and max always appear together (the missing one
disabled), close needs a system menu, and windows with `WS_EX_DLGMODALFRAME` show no icon.

### `[Colors]`

Named colours, referenced by name anywhere a colour is expected:

```ini
[Colors]
Face=Sys:BtnFace          ; a system colour (follows the Control Panel)
Accent=0,0,128            ; a literal r,g,b
Title=Accent              ; another named colour
```

A colour value is one of:

* `Sys:<name>`: `Scrollbar`, `Background`, `ActiveCaption`, `InactiveCaption`, `Menu`, `Window`, `WindowFrame`,
  `MenuText`, `WindowText`, `CaptionText`, `ActiveBorder`, `InactiveBorder`, `AppWorkspace`, `Highlight`,
  `HighlightText`, `BtnFace`, `BtnShadow`, `GrayText`, `BtnText`, `InactiveCaptionText`, `BtnHighlight`, and the two
  Win95 colours `3DDkShadow` (black on Windows 3.x) and `3DLight` (= `BtnFace` on Windows 3.x);
* `r,g,b` (0–255 each);
* the name of a `[Colors]` entry;
* `None` (draw nothing).

### `[SysColors]`

System colours the engine sets while the theme is active, for a look whose colours differ from the Control
Panel's (`WINME`: the beige Windows ME scheme). Keys are the `Sys:` names above, values literal `r,g,b`:

```ini
[SysColors]
BtnFace=212,208,200
Background=58,110,165
3DDkShadow=64,64,64       ; the two Win95 colours only affect the theme's own drawing
```

The engine sets them with `SetSysColors` when it starts (nothing is written to `WIN.INI`), so programs that draw
with system colours follow too, and puts the old values back when it stops, except colours changed in the
meantime. With 16 colours or fewer the nearest solid colours are used; Windows would dither the others. A theme
inherits the `[SysColors]` of its base and can override single entries.

### `[Fonts]`

```ini
[Fonts]
Caption=MS Sans Serif,8,700      ; face, points, weight[, italic (0/1)]
```

The engine and `WIN95` use the names `Caption`, `Menu`, `Controls` and `IconTitle`. A `Text` op without `Font=`
uses the first font of its theme.

### `[Recolor]`

Placeholder colours in bitmaps that are replaced when the bitmap is loaded (only for `Bitmap` ops with `Recolor=1`),
and again whenever the system colours change:

```ini
[Recolor]
255,255,0=Sys:BtnFace     ; yellow pixels become the button face colour
0,255,0=Sys:BtnHighlight
```

At most 8 entries.

## 2. Part sections

Every other section whose name is `<Part>` or `<Part>.<State>` defines how that part looks. A section named only
`<Part>` is the `Normal` state. Sections with any other name are helpers: they are only used when a `Layers=` key
refers to them.

### Parts

| Part | Drawn for | Notes |
|---|---|---|
| `Frame`, `DlgFrame` | sizable / dialog window frames | states `Active`, `Inactive`; only the ring outside the inner rectangle is painted |
| `Caption` | caption band | `Active`, `Inactive`; the band includes the 1-pixel separator row below the bar |
| `CaptionText` | window title | `Active`, `Inactive` |
| `BtnClose`, `BtnMin`, `BtnMax`, `BtnRestore` | caption buttons | `Normal`, `Pressed`, `Disabled` |
| `MenuBar`, `MenuBarItem`, `MenuBarText` | menu bar | items: `Normal`, `Selected`, `Disabled` |
| `Menu`, `MenuItem`, `MenuText`, `MenuSeparator`, `MenuCheck`, `MenuSubArrow` | popup menus | `Normal`, `Selected`, `Disabled` |
| `PushButton`, `PushButtonText` | push buttons | `Normal`, `Pressed`, `Disabled`, `Default`, `DefaultPressed` |
| `Focus` | keyboard focus indicator | |
| `CheckBox`, `Radio` | 13×13 / 12×12 boxes | `Normal`, `Pressed`, `Disabled`, `Checked`, `CheckedPressed`, `CheckedDisabled`, `Mixed`, `MixedPressed`, `MixedDisabled` |
| `GroupBox`, `StaticText` | group frame, static text | `StaticText`: `Normal`, `Disabled` |
| `Field`, `ComboButton` | edit/list/combo border; combo drop button | `Field` paints only its border ring |
| `ScrollUp`, `ScrollDown`, `ScrollLeft`, `ScrollRight` | scroll arrows | `Normal`, `Pressed`, `Disabled` |
| `ScrollThumbV`, `ScrollThumbH`, `ScrollTrackV`, `ScrollTrackH`, `ScrollSizeBox` | scroll bar parts | tracks: `Normal`, `Pressed` |
| `Dialog`, `MDIClient`, `Desktop`, `IconTitle` | backgrounds, icon titles | if `Dialog` is a plain `Fill`, its colour is the dialog background Windows paints (`WM_CTLCOLOR` brush); otherwise the button face is used |

### State fallback

If a state is not defined, the engine tries the next state in this chain, then `Normal`, then the `Base=` theme,
then lets Windows draw the part:

| State | falls back to |
|---|---|
| `DefaultPressed` | `Pressed` |
| `CheckedPressed` | `Checked` |
| `CheckedDisabled` | `Disabled` |
| `Mixed` | `Checked` |
| `MixedPressed` | `Mixed` → `Checked` |
| `MixedDisabled` | `CheckedDisabled` → `Disabled` |
| anything else | `Normal` |

`Op=None` explicitly hands a part back to Windows.

## 3. Ops

Each part or helper section is one op. Keys common to all ops:

| Key | Meaning |
|---|---|
| `Op` | `Fill`, `Dither`, `Gradient`, `Edge`, `Border`, `Glyph`, `Bitmap`, `Text`, `Focus`, `Layers`, `None`. May be omitted when `Layers=` is present. |
| `Inset` | `l,t,r,b` or a single value: shrink the target rectangle first |

### `Layers`

`Layers=A,B,C` draws the sections `A`, `B`, `C` in that order into the same rectangle (after this op's `Inset`).
At most 6 layers, nesting up to 4 levels.

### `Fill`, `Dither`

`Color=`; `Dither` also `Color2=` (50 % checkerboard, as used for Win95 scroll bar tracks).

### `Gradient`

`From=`, `To=`, `Direction=Horizontal|Vertical|Concentric`, `Bands=` (default 32). `From` = `To` gives a plain fill.
`Concentric` draws nested rectangles from the outside (`From`) to the centre (`To`).

### `Edge`

Win95 `DrawEdge`-style 3D borders. `Edge=` picks a preset; `Outer=TL/BR` and `Inner=TL/BR` (two colours separated
by `/`) override its colours; `Fill=` fills the inside.

| Preset | Outer (top-left / bottom-right) | Inner | Used for |
|---|---|---|---|
| `Raised` | 3DLight / 3DDkShadow | BtnHighlight / BtnShadow | window frames, scroll arrows |
| `RaisedSoft` | BtnHighlight / 3DDkShadow | 3DLight / BtnShadow | push and caption buttons |
| `Sunken` | BtnShadow / BtnHighlight | 3DDkShadow / 3DLight | fields, check boxes |
| `SunkenSoft` | 3DDkShadow / BtnHighlight | BtnShadow / 3DLight | pressed caption buttons |
| `Etched` | BtnShadow / BtnHighlight | BtnHighlight / BtnShadow | group boxes |
| `Bump` | 3DLight / 3DDkShadow | BtnShadow / BtnHighlight | |
| `RaisedOuter`, `RaisedInner`, `SunkenOuter`, `SunkenInner` | single 1-pixel ring | – | separators |
| `Flat` | BtnShadow / BtnShadow | – | pressed scroll arrows |

### `Border`

`Color=`, `Width=` (default 1): a solid frame.

### `Glyph`

A built-in shape centred in the rectangle: `Glyph=` one of `Close`, `Min`, `Max`, `Restore`, `ArrowUp`, `ArrowDown`,
`ArrowLeft`, `ArrowRight`, `Check`, `MenuCheck`, `SubArrow`, `RadioDot`, `RadioFill`, `RadioOuter`, `RadioInner`,
`SizeGrip`.

`Color=`; `Color2=` (second colour: bottom-right half of `RadioOuter`/`RadioInner`, dark lines of `SizeGrip`);
`Shadow=` (drawn first, 1 pixel down and right: with `Color=BtnShadow` and `Shadow=BtnHighlight` this is the Win95
disabled look); `Size=w,h` (scaled, default natural size); `Offset=x,y`.

### `Bitmap`

| Key | Meaning |
|---|---|
| `Image` | BMP file in the theme directory: Windows 3.x DIB, uncompressed, 1/4/8/24 bpp, bottom-up, at most 60 000 bytes |
| `Source` | `x,y,w,h` in the image (default: the whole image); lets one image hold many states |
| `Sizing` | `Stretch` (default), `Tile`, `NineGrid`, `Center` |
| `Margins` | `l,t,r,b` for `NineGrid`: these borders keep their size, the rest is stretched |
| `Transparent` | `r,g,b` colour key: pixels of this colour are not drawn |
| `Recolor` | `1`: apply `[Recolor]` |

Use 16-colour bitmaps where possible: most Windows 3.x systems run at 16 or 256 colours, and GDI memory is limited
(at most 16 images per theme).

### `Text`

`Font=`, `Color=`, `Shadow=` (drawn 1 pixel down and right first), `Align=Auto|Left|Center|Right` (`Auto`: as
Windows would align it), `Offset=x,y`. The text itself comes from Windows (window title, menu item, button label).
Text is centred vertically in the rectangle.

### `Focus`

The dotted focus rectangle (XOR), usually with an `Inset`.

## 4. Limits

| Item | Limit |
|---|---|
| ops (sections used) | 320 |
| colours (named + literals) | 64 |
| fonts | 8 |
| images | 16 |
| `[Recolor]` entries | 8 |
| file names | 8.3 |

Warnings (unknown keys, colours, fonts, glyphs, missing files) are printed by the previewer and written to
`RETHEME.LOG` by the engine. The theme still loads; the affected op draws nothing or is skipped.
