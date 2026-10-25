#!/usr/bin/env python3
"""End-to-end test of SETUP.EXE in QEMU, from the release floppy image:

  1. silent install from A: (SETUP /S /T=theme): files, RETHEME.INI,
     WIN.INI load= and [Retheme16], Program Manager group, SETUP.LOG
  2. reboot: the engine starts from load= (RETHEME.LOG)
  3. silent removal with the installed copy (SETUP /U /S): directory,
     WIN.INI entries and group are gone

  test_setup.py --base IMG [--floppy build/release/retheme16-X.Y.Z-disk.img]
                [--out DIR] [--theme WIN98]

Screenshots of every stage go to DIR (default build/emu/setup). Exit
code 1 if a check fails. Needs `make release` (tools/release/mkrelease.py).
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
PART = 63 * 512
failed = 0


def check(cond, what):
    global failed
    print("%s  %s" % ("ok  " if cond else "FAIL", what))
    failed += not cond


def mt(image, *args):
    r = subprocess.run([args[0], "-i", "%s@@%d" % (image, PART)] + list(args[1:]), capture_output=True,
                       encoding="latin-1")
    return r.stdout if r.returncode == 0 else None


def ini_value(text, section, key):
    sec = ""
    for line in (text or "").replace("\r\n", "\n").split("\n"):
        s = line.strip()
        if s.startswith("["):
            sec = s[1:-1].lower()
        elif "=" in s and sec == section.lower() and s.split("=", 1)[0].strip().lower() == key.lower():
            return s.split("=", 1)[1].strip()
    return None


def run(base, out, keep, *extra, steps=()):
    cmd = [sys.executable, os.path.join(HERE, "run.py"), "--base", base, "--out", out, "--no-deploy",
           "--keep", keep] + list(extra)
    for s in steps:
        cmd += ["--step", s]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--floppy")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "emu", "setup"))
    ap.add_argument("--theme", default="WIN98")
    a = ap.parse_args()
    floppy = a.floppy or (sorted(glob.glob(os.path.join(ROOT, "build", "release", "retheme16-*-disk.img"))) or
                          [None])[-1]
    if not floppy:
        sys.exit("no floppy image: run 'make release' first")
    os.makedirs(a.out, exist_ok=True)
    i1, i2, i3 = (os.path.join(a.out, n) for n in ("1-installed.img", "2-rebooted.img", "3-removed.img"))

    print("== silent install from %s" % os.path.basename(floppy))
    run(a.base, a.out, i1, "--floppy", floppy, "--run", "A:\\SETUP.EXE /S /T=%s" % a.theme, "--boot", "45",
        steps=["20", "shot 1-installed"])
    files = mt(i1, "mdir", "-b", "-/", "::/RETHEME") or ""
    for f in ("RETHEME.EXE", "RETHEME.DLL", "GALLERY.EXE", "SETUP.EXE", "SETUP.INF", "README.TXT",
              "THEMES/%s/THEME.INI" % a.theme):
        check("::/RETHEME/" + f in files, "installed %s" % f)
    ini = mt(i1, "mtype", "::/RETHEME/RETHEME.INI")
    check(ini_value(ini, "Engine", "Theme") == "C:\\RETHEME\\THEMES\\" + a.theme, "RETHEME.INI Theme=")
    win = mt(i1, "mtype", "::/WINDOWS/WIN.INI")
    check("C:\\RETHEME\\RETHEME.EXE" in (ini_value(win, "windows", "load") or ""), "WIN.INI load=")
    check(ini_value(win, "Retheme16", "InstallDir") == "C:\\RETHEME", "WIN.INI [Retheme16] InstallDir=")
    pm = mt(i1, "mtype", "::/WINDOWS/PROGMAN.INI") or ""
    check("RETHEME" in pm.upper(), "Program Manager group")
    log = mt(i1, "mtype", "::/RETHEME/SETUP.LOG") or ""
    check("has been installed" in log, "SETUP.LOG")
    print(log.strip())
    rlog = mt(i1, "mtype", "::/RETHEME/RETHEME.LOG") or ""
    check("RETHEME start" in rlog, "engine started after installing")

    print("== reboot")
    run(i1, a.out, i2, "--boot", "45", steps=["shot 2-rebooted"])
    rlog = mt(i2, "mtype", "::/RETHEME/RETHEME.LOG") or ""
    check(rlog.count("RETHEME start") == 2, "engine started from load=")

    print("== silent removal with the installed copy")
    run(i2, a.out, i3, "--run", "C:\\RETHEME\\SETUP.EXE /U /S", "--boot", "60", steps=["shot 3-removed"])
    check(mt(i3, "mdir", "::/RETHEME") is None, "C:\\RETHEME removed")
    win = mt(i3, "mtype", "::/WINDOWS/WIN.INI")
    check("RETHEME" not in (ini_value(win, "windows", "load") or "").upper(), "WIN.INI load= cleaned")
    check("[retheme16]" not in (win or "").lower(), "WIN.INI [Retheme16] removed")
    pm = mt(i3, "mtype", "::/WINDOWS/PROGMAN.INI") or ""
    check("RETHEME" not in pm.upper(), "Program Manager group removed")
    print((mt(i3, "mtype", "::/WINDOWS/TEMP/W16UNINS.LOG") or "").strip())

    for f in (i1, i2, i3):
        os.unlink(f)
    print("all checks passed" if not failed else "%d check(s) failed" % failed)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
