# win31_theme_engine

Open-source, themeable engine that brings the classic 3D look (theme Redmond 95), a blue, rounded look (theme
RedmondExperience), a light, flat look (theme Redmond-11), the grey Platinum and the glossy Aqua look (themes
Cupertino Platinum and Cupertino Aqua) and custom, skinnable themes to Windows 3.1 and Windows for Workgroups 3.11. Working name:
Retheme16.

![Redmond 95 theme preview](host/tests/golden/red95_window.png)
![Redmond 95 dialog preview](host/tests/golden/red95_dialog.png)
![Slate theme preview](host/tests/golden/slate_window.png)
![RedmondExperience theme preview](host/tests/golden/redexp_window.png)
![RedmondExperience dialog preview](host/tests/golden/redexp_dialog.png)
![Redmond-11 theme preview](host/tests/golden/red11_window.png)
![Redmond-11 dialog preview](host/tests/golden/red11_dialog.png)
![Cupertino Platinum theme preview](host/tests/golden/cupplat_window.png)
![Cupertino Platinum dialog preview](host/tests/golden/cupplat_dialog.png)
![Cupertino Aqua theme preview](host/tests/golden/cupaqua_window.png)
![Cupertino Aqua dialog preview](host/tests/golden/cupaqua_dialog.png)

*Previews rendered on Linux by the engine's own renderer (`make preview`).*

On Windows 3.1 with a 16-bit (65536 colour) display driver, RedmondExperience and the
[NewShell16](https://github.com/ceisserer/NewShell16) shell, which switches its taskbar and Start menu to the same
look:

![RedmondExperience with NewShell16 on Windows 3.1, 800x600, 65536 colours](docs/images/winxp_newshell16.png)

Redmond-11 on Windows 3.1 (VBE driver, 800×600, 65536 colours), with the theme gallery:

![Redmond-11 on Windows 3.1, 800x600, 65536 colours](docs/images/win11_gallery.png)

## Download

Releases with the installer (`SETUP.EXE`, as ZIP and as 1.44 MB floppy image) are on the
[Releases page](https://github.com/ceisserer/win31_theme_engine/releases). Unzip, run `SETUP.EXE`, pick a theme.
The pre-release [snapshot](https://github.com/ceisserer/win31_theme_engine/releases/tag/snapshot) is rebuilt from
every push to the default branch (untested, for trying out the latest changes).

## Documents

* [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md): architecture, theme format, test strategy, phases
* [docs/STATUS.md](docs/STATUS.md): what is implemented and verified
* [docs/THEME_FORMAT.md](docs/THEME_FORMAT.md): how to write a theme
* [docs/BUILDING.md](docs/BUILDING.md): building, testing, installing
* [docs/REQUIRED_TOOLS.md](docs/REQUIRED_TOOLS.md): tools and software needed, and which of it has been provided

## Layout

```
engine/include, theme, render, geom   portable core (C, builds with gcc and Open Watcom 16-bit)
engine/win16                          RETHEME.DLL runtime: patches, hooks, GDI backend
loader/                               RETHEME.EXE
testapp/                              GALLERY.EXE test scenes
switcher/                             THEMES.EXE: switches the theme of the running engine
dist/                                 release package: SETUP.INF (installer configuration), README.TXT
external/win16setup                   SETUP.EXE installer (git submodule: Win16Setup)
host/                                 Linux backend, previewer, tests, golden images
themes/                               themes: RED95 (default), RED98, REDME, REDEXP (RedmondExperience), RED11 (Redmond-11), CUPPLAT (Cupertino Platinum), CUPAQUA (Cupertino Aqua), SLATE
tools/                                generators, emulator tests (tools/emu), release packaging (tools/release)
```

Quick start: `git submodule update --init && make test && make preview && make win16` (see [docs/BUILDING.md](docs/BUILDING.md)).

## License

Retheme16 is free software under the GNU General Public License, version 3 or (at your option) any later version;
see [LICENSE](LICENSE). The installer, [Win16Setup](https://github.com/ceisserer/Win16Setup), is GPL-3.0 as well.
