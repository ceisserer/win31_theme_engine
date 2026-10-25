Retheme16 @VERSION@
==================

Retheme16 gives Windows 3.1 and Windows for Workgroups 3.11 the look of
Windows 95: title bars with the Windows 95 buttons, 3D dialog controls,
grey menus and Windows 95 scroll bars. Other looks come as themes.

Requirements: Windows 3.1 or Windows for Workgroups 3.11, Standard or
386 Enhanced mode, 16 or 256 colours.


INSTALLING

Run SETUP.EXE (File Manager, or File > Run in Program Manager). Setup
asks for the directory (default C:\RETHEME) and the theme, and can:

  - start Retheme16 with Windows (adds RETHEME.EXE to load= in WIN.INI)
  - create a "Retheme16" group in Program Manager
  - start Retheme16 right away

Silent installation (for scripts and network installs):

  SETUP /S [/D=C:\RETHEME] [/T=WIN95] [/NOLOAD] [/NOGROUP] [/NOSTART]

  /D=dir      installation directory
  /T=theme    theme directory name: WIN95, WIN98 or SLATE
  /NOLOAD     do not start Retheme16 with Windows
  /NOGROUP    no Program Manager group
  /NOSTART    do not start Retheme16 after installing

Setup writes SETUP.LOG into the installation directory.


USING RETHEME16

RETHEME.EXE loads the engine and stays as an icon. Closing the icon
unloads the engine and restores the Windows 3.1 look. To change the
theme, run "Retheme16 Setup" from the Program Manager group again, or
edit RETHEME.INI:

  [Engine]
  Theme=C:\RETHEME\THEMES\WIN95

The Theme Gallery (GALLERY.EXE) shows dialogs, menus and window styles
for trying a theme. Problems are reported in RETHEME.LOG.


REMOVING

Run "Retheme16 Setup" and click Remove, or run SETUP /U (SETUP /U /S
without questions). Setup removes the files, the WIN.INI entries and the
Program Manager group.


THEMES

  WIN95   Windows 95 (default)
  WIN98   Windows 98 (gradient title bars)
  SLATE   a dark example theme made of bitmaps

How to write themes: docs/THEME_FORMAT.md in the source code.


LICENSE

Retheme16 is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version. It comes WITHOUT ANY WARRANTY. See
COPYING.TXT for the license.


SOURCE CODE

https://github.com/ceisserer/win31_theme_engine
