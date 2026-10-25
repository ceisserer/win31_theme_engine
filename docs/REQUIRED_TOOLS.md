# Tools, software and material needed

Companion to [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).

Split into:

* **A. What I (Claude, in the cloud session) can get myself.** Open-source or free, and reachable from this
  container. Checked on 2026-10-25: Ubuntu apt, PyPI, `git clone` from GitHub and GitHub release downloads work;
  archive.org, winworldpc.com and deb.debian.org are blocked by the network policy.
* **B. What you need to provide.** Not free (as in beer), licence-restricted, or only available from sites this
  container cannot reach.
* **C. Infrastructure decisions.**

Priority: **R** = required, **S** = strongly recommended, **O** = optional.

---

## A. I can obtain these myself

| Tool | Purpose | Source | Prio |
|---|---|---|---|
| Open Watcom C/C++ 2.0 | Win16 compiler, linker, resource compiler, `wmake`; also ships Win 3.x Spy, Heap Walker and debugger | GitHub release `Current-build` (download verified) | R |
| gcc, make, Python 3 | Host build of the portable core, unit tests, `themec` | already installed | R |
| Pillow, NumPy | Bitmap dithering, previews, screenshot diffing | PyPI | R |
| QEMU (`qemu-system-i386`) | Headless emulator for automated screenshot tests (`screendump`, `sendkey`) | apt | R |
| DOSBox-X | Fast dev loop (mounts the build directory directly), built-in debugger, no DOS licence needed | apt | R |
| mtools, dosfstools | Read/write FAT disk images without mounting | apt | R |
| ImageMagick | Screenshot comparison, image conversion | apt | O |
| NASM | Small assembly stubs if inline asm is not enough | apt | O |
| 86Box | Cycle-accurate PC emulator for real display drivers and timing | GitHub release | S |
| vbesvga.drv | Generic VBE SVGA driver for Windows 3.1: 256-colour / high-colour tests in QEMU (test images only, not redistributed) | GitHub release (download verified) | S |
| Ghidra (+ JDK) | Reading **USER.EXE/GDI.EXE** (your licensed copy) for geometry/profile research. NE format supported. | GitHub release | S |
| Wine source | Behaviour reference: `uitools.c` (`DrawEdge`, `DrawFrameControl`), `nonclient.c`, `menu.c`, `scroll.c`, `*.exe16.spec` ordinals | GitHub mirror (git) | S |
| ReactOS source | Second reference for the same | GitHub (git) | O |
| Wine (runtime) | Only for running Win16 *tools* (e.g. resource viewers); not usable as test target | apt | O |

---

## B. You need to provide these

> **Please don't commit any of these to this repository** if it is or might become public. See section C for
> how to get them into a cloud session.

### B.0 Already provided (2026-10-25)

These files are available on a private download server that the cloud environment is allowed to reach.

| File | Size | Covers |
|---|---|---|
| `Win_31.zip` | 6.3 MB | An installed `WINDOWS` directory; it is actually **WfW 3.11** (USER.EXE 3.11) |
| `Microsoft Windows 3.1 (3.10.103) (3.5).7z` | 6.5 MB | Windows 3.1, US English retail 3.10.103, 3.5" disks |
| `Microsoft Windows 3.1 [German] (3.5-1.44mb).7z` | 6.4 MB | Localised (German) Windows 3.1 |
| `Windows 3.11.iso` | 11 MB | Windows for Workgroups 3.11 |
| `Microsoft Windows For Workgroups 3.11 (OEM) [German] (3.5-1.44mb).7z` | 11 MB | WfW 3.11, German OEM |
| `Microsoft MS-DOS 6.20 [German] (3.5).7z` | 4.3 MB | MS-DOS 6.20 (German) for QEMU/86Box installs |
| `Microsoft Windows 3.1 SDK (1992) (ISO).7z` | 17 MB | Windows 3.1 SDK: debug USER/GDI/KERNEL + symbols, DBWIN, help compiler |
| `Microsoft Windows 3.1 DDK.7z` | 86 MB | Windows 3.1 DDK: display driver sources, `OBM_*` resources |
| `Microsoft Visual C++ - Version 1.52c (Microsoft) (1995).iso` | 202 MB | VC++ 1.52c: CTL3D, MFC 2.5 test apps, MS C runtime |

Windows 95 reference screenshots will be taken from online sources instead of a Windows 95 installation.
Windows 3.0 and real mode are out of scope, so no Windows 3.0 media is needed.

Note: the only DOS provided is German. That works (DOS language doesn't affect the Windows build being tested), but
automation that types into DOS (`sendkey`) has to use the German keyboard layout.

### B.1 Operating systems

| Item | Purpose | Prio | Status |
|---|---|---|---|
| **Windows 3.1** (US English retail, 3.10) | Main target | **R** | provided |
| **MS-DOS 6.22** (or 5.0) | Needed to install/run Windows 3.1 in QEMU/86Box | **R** | provided (6.20 German) |
| **Windows for Workgroups 3.11** | Compatibility target (different USER build, 32-bit file access) | **R** for phase 9 | provided (English + German OEM) |
| **Windows 95** | Golden reference screenshots, metrics, colours | **S** | replaced by online screenshots |
| **Localised Windows 3.1** (e.g. German) | Version-drift testing of USER internals | O | provided (German) |
| Windows 3.1 OEM variants | Same | O | open |

### B.2 SDKs and debug material

| Item | Purpose | Prio | Notes |
|---|---|---|---|
| **Windows 3.1 SDK** (provided) | Debug versions of KRNL386/USER/GDI with **.SYM symbol files** (names USER's internal routines, which makes the geometry/profile work much faster); parameter validation; DBWIN; `WINDOWS.H` reference | **S** | Open Watcom has its own headers and libraries, so the build doesn't need the SDK. The debug binaries and symbols are the valuable part. |
| Windows 3.1 DDK (provided) | Display driver sources (VGA/SVGA/8514); `OBM_*` resources; `FastWindowFrame` behaviour | O | Helps with risk "display drivers" (`IMPLEMENTATION_PLAN.md`, Risks). |
| WfW 3.11 debug binaries / symbols | Same as the SDK, for WfW | O | |
| Microsoft Visual C++ 1.52 (provided, 1.52c) | CTL3D/CTL3DV2 (libraries and docs), MFC 2.5 test applications, cross-check of the MS C runtime | O | Not needed for building. |
| SoftICE for Windows 3.x, WDEB386 | System-level debugging (stepping through GDI/USER) | O | Open Watcom debugger + DOSBox-X debugger cover most needs. |
| Microsoft Help Compiler (HC31 / HCW) | Only if the docs should be a WinHelp `.HLP` | O | Plain text / HTML docs need nothing. |

### B.3 Test applications (period software)

| Item | Why this one | Prio |
|---|---|---|
| **Visual Basic 3.0** (Pro) | `Thunder*` forms, VB controls, VBX custom controls; also lets us build test apps quickly | **R** |
| **Microsoft Office 4.3** (Word 6.0, Excel 5.0) | Heavy menu/toolbar/MDI users, own desk classes (`XLDESK`, `OpusDesk`) | **R** |
| **Borland Delphi 1.0** | VCL classes (`TApplication`, `TForm`, `TComboBox`), own drawing | **S** |
| **Borland C++ 4.5** | BWCC dialogs (`BorDlg`, `BorCheck` ...), OWL apps | **S** |
| Clarion for Windows | `Cla*` classes | O |
| Lotus SmartSuite / 1-2-3 R4, Ami Pro | Different UI toolkits, common at the time | O |
| CorelDRAW 3/4, Aldus PageMaker | Owner-drawn UI, palette-heavy (256-colour testing) | O |
| Norton Desktop for Windows / PC Tools for Windows | Shell replacements, hook coexistence | O |
| Netscape Navigator 1.x–3.x (16-bit) + Trumpet Winsock | Popular, own drawing, network stack loaded | O |

### B.4 Drivers and emulator files

| Item | Purpose | Prio | Notes |
|---|---|---|---|
| Windows 3.1 display drivers for the emulated cards: **Cirrus Logic GD54xx** (QEMU), **S3 Trio64/Vision** (86Box, DOSBox-X), **Tseng ET4000** (86Box) | 256-colour and hi-colour testing | **S** | Freely downloadable vendor drivers, but from sites this container can't reach (vogons, archive.org, driver archives). |
| **86Box ROM set** | 86Box can't run without BIOS images | S (if 86Box is used) | Copyrighted BIOS dumps; please provide or confirm I may download them from the 86Box `roms` repository. |
| Sound/network drivers | Only if sound schemes in themes get tested | O | |

### B.5 Documentation / books

| Item | Why | Prio |
|---|---|---|
| *Undocumented Windows* (Schulman, Maxey, Pietrek, 1992) | USER/GDI internals, `PrestoChangoSelector`, hooks, TOOLHELP | **S** |
| *Windows Internals* (Matt Pietrek, 1993) | Pseudocode of USER/KERNEL internals, including `DefWindowProc`, NC painting, menus | **S** |
| *Programming Windows 3.1* (Petzold) | General Win16 API reference | O |
| Windows 3.1 SDK *Programmer's Reference* vol. 1–4, *Guide to Programming* | Exact API semantics, 3.0 vs 3.1 differences | S (comes with the SDK) |
| MSDN Library CD (1993–1995) | Knowledge-base articles on USER quirks, CTL3D docs | O |

PDF scans would do; they exist on archive.org, which this session cannot reach.

### B.6 Optional hardware

| Item | Purpose |
|---|---|
| Real 386/486 PC with ISA/VLB VGA, SVGA and an 8514/A-compatible or Hercules card | Final check against real drivers and timing (emulators can hide races) |

---

## C. Infrastructure decisions for you

1. **How do licensed images reach the cloud session?** Each session starts with a fresh container. The files in
   B.0 are on a private download server whose host name is allowed in the environment's network settings (done;
   winworldpc.com is allowed too). For reference, a host is allowed under Allowed domains (cloud environment menu → Edit → Network access; steps at
   https://code.claude.com/docs/en/cloud-environments#network-access) and start a new session. Alternatives:
   * a **separate private repository** (Git LFS for disk images) that you add to the session;
   * or run the emulator steps on your own machine and give me the screenshots/traces (slowest loop).
2. **A prepared disk image** saves a lot of time: MS-DOS 6.22 + Windows 3.1 installed, SVGA driver set up, `SMARTDRV`,
   `RETHEME` directory on `D:` (or a FAT host-dir drive), `WIN.INI` `load=`/`run=` hook for the test harness.
   Ideally one image each for 3.1 (English), 3.1 (German) and WfW 3.11.
3. **Self-hosted CI runner (optional)** with the images, if emulator tests should run on every push. Host-side tests
   run on plain GitHub Actions without any licensed material.
4. **Licence and name** of the project (`IMPLEMENTATION_PLAN.md`, Decisions).

---

## Minimum set to start

Provided (B.0): Windows 3.1, MS-DOS, WfW 3.11, the SDK and the DDK cover phases 0–5. What is still open:

1. test applications from B.3 (VB 3.0 and Office 4.3 first), needed from phase 4 on;
2. Windows 3.1 drivers for real SVGA cards (B.4), for the display-driver matrix; 256 colours in QEMU are already
   covered by vbesvga.drv.
