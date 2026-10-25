# Emulator tools

Scripts to test the engine on real Windows 3.1 / WfW 3.11 in QEMU, headless.
They need licensed installation media (see `docs/REQUIRED_TOOLS.md`); nothing
from them is stored in this repository.

| Script | Purpose |
|---|---|
| `mkdosdisk.sh DISK1.IMG OUT.IMG [MB]` | Bootable MS-DOS hard disk (FAT16, CHS 16/63) made from DOS setup disk 1, without typing: a patched copy of the floppy runs `FDISK /MBR` and `SYS C:`. |
| `mkwin31.py --dos DOS.IMG --himem HIMEM.SYS --out WIN31.IMG [--lang deu --kblayout gerdll] DISK1.IMG ... DISK6.IMG` | Unattended Windows 3.1 install (`SETUP /H` with a generated `.SHH`: VGA, PS/2 mouse, no network; US English by default). Takes about a minute. |
| `mkdebug.sh WIN31.IMG SDK/WINDEV/DEBUG OUT.IMG` | Copy with the SDK's debug KERNEL/USER/GDI and symbols. |
| `mkvbe.py --base WIN31.IMG --driver DIR --out OUT.IMG [--depth 8]` | Copy using the VBE SVGA driver ([vbesvga.drv](https://github.com/PluMGMK/vbesvga.drv) release files) for 256-colour / high-colour tests in QEMU's standard VGA. |
| `regress.sh IMG [PROFILE] [--update-golden]` | Runs every scenario against `tests/emu/golden/PROFILE/` (default `win31`; `win31de` for German Windows 3.1). |
| `run.py --base IMG --out DIR [--script FILE] [--step ...]` | Copies the image, deploys `build/win16` and `themes` to `C:\RETHEME`, starts Windows with the engine (and optionally GALLERY.EXE), runs the steps, saves screenshots and `RETHEME.LOG` (and COM1 with `--serial`). |
| `test_setup.py --base IMG [--floppy DISK.IMG] [--theme NAME]` | End-to-end test of `SETUP.EXE` from the release floppy image (`make release`): silent install, reboot (engine starts from `load=`), silent removal with the installed copy; checks files, `RETHEME.INI`, `WIN.INI` and the Program Manager group on the disk. |
| `qmon.py SOCKET CMD...` | Sends commands to the QEMU monitor. |
| `scenarios/*.txt` | Step scripts for `run.py --script`. |

Options: `--theme NAME`, `--gallery "ARGS"`, `--serial` (debug kernel output on COM1), `--trace [1|2]` (engine
logs GDI calls: 1 = in paint contexts, 2 = all), `--golden DIR [--update-golden]`, `--floppy IMG` (drive A:),
`--run "CMD"` (starts Windows with `WIN CMD`, e.g. `--run "A:\SETUP.EXE"`), `--no-deploy` (leaves the image as it
is, e.g. to test Setup), `--keep FILE` (saves the disk image after the run, for a next run with `--base FILE` or for
inspecting it with mtools).

Steps: a number waits that many seconds; `shot NAME`, `click X Y`, `move X Y`,
`drag X1 Y1 X2 Y2`, `mon <QEMU monitor command>` (e.g. `mon sendkey alt-f4`).
Mouse acceleration is switched off in WIN.INI so positions are exact. A `click`
presses and releases within ~50 ms, shorter than USER's auto-repeat delay, so
clicking a scroll arrow is exactly one step; holding a button
(`mon mouse_button 1` ... `mon mouse_button 0`) is timing-dependent wherever
USER auto-repeats.

Example (as used during development):

```sh
tools/emu/mkdosdisk.sh 144OEM1.IMG dos.img 256
tools/emu/mkwin31.py --dos dos.img --himem HIMEM.SYS --out win31.img Disk0[1-6].img
tools/emu/mkdebug.sh win31.img WINDEV/DEBUG win31dbg.img
make win16
tools/emu/run.py --base win31dbg.img --out out --serial --script tools/emu/scenarios/caption.txt
tools/emu/regress.sh win31.img
```

Notes:
* Some archived 1.44 MB floppy images are short (trailing empty sectors cut
  off); the scripts pad them, otherwise QEMU guesses the wrong geometry.
* The VM is killed at the end of a run, so the images run without SMARTDRV.
* INI files of localised Windows are not UTF-8; the scripts read and write them as Latin-1.
* Scenarios must not depend on the Windows language (e.g. menu accelerators of Program Manager); GALLERY's own
  menus are used instead (File > Unload engine).
