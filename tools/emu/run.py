#!/usr/bin/env python3
"""Boots a Windows 3.x disk image in QEMU with the engine deployed, runs a
scripted session and collects screenshots and RETHEME.LOG.

  run.py --base IMG --out DIR [--no-engine] [--gallery ARGS] [--theme NAME]
         [--serial] [--script FILE] [--step STEP] ...

Steps run in order after boot:
  <seconds>        wait
  shot NAME        screendump to DIR/NAME.png
  mon CMD          raw QEMU monitor command (sendkey, mouse_move ...)
  move X Y         move the mouse to absolute screen position X, Y
  click [X Y]      left click (at X, Y if given)
  drag X1 Y1 X2 Y2 press at X1,Y1, move to X2,Y2, release

--floppy IMG inserts a disk image as drive A:, --run "CMD" starts Windows
with "WIN CMD" (e.g. --run "A:\\SETUP.EXE /S"), --no-deploy leaves the
engine out of the image and --keep FILE saves the disk image after the run
(for a second run with --base FILE, or for inspecting it with mtools).

With --golden DIR every screenshot is compared with DIR/NAME.png (exit code
1 if more than --tolerance pixels differ; a diff image is written next to
the screenshot); --update-golden writes the screenshots into DIR instead.

The base image is copied (sparse) for every run, so the base stays clean.
Files are deployed to C:\\RETHEME; WIN.INI gets load=/run= entries.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qmon  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PART = 63 * 512


def mtools(*args, check=True):
    return subprocess.run(list(args), check=check, capture_output=True, encoding="latin-1")


def img(path):
    return "%s@@%d" % (path, PART)


def set_win_ini(image, load, run):
    """Rewrites load= and run= in C:\\WINDOWS\\WIN.INI and turns mouse
    acceleration off (QEMU merges motion events, which would otherwise be
    accelerated unpredictably)."""
    text = mtools("mtype", "-i", img(image), "::/WINDOWS/WIN.INI").stdout
    out, done = [], set()
    section = ""
    for line in text.replace("\r\n", "\n").split("\n"):
        s = line.strip()
        if s.startswith("["):
            section = s.lower()
        key = s.split("=", 1)[0].strip().lower()
        if section == "[windows]" and key in ("load", "run", "mousespeed"):
            vals = {"load": load, "run": run, "mousespeed": "0"}
            line = "%s=%s" % (key, vals[key])
            done.add(key)
        out.append(line)
    if len(done) < 3:
        i = next(i for i, l in enumerate(out) if l.strip().lower() == "[windows]")
        for key, val in (("load", load), ("run", run), ("MouseSpeed", "0")):
            if key.lower() not in done:
                out.insert(i + 1, "%s=%s" % (key, val))
    with tempfile.NamedTemporaryFile("w", delete=False, suffix=".ini", newline="\r\n",
                                     encoding="latin-1") as f:
        f.write("\n".join(out))
        tmp = f.name
    mtools("mcopy", "-o", "-i", img(image), tmp, "::/WINDOWS/WIN.INI")
    os.unlink(tmp)


def set_win_ini_keep(image):
    """Only turns mouse acceleration off; load= and run= stay as they are."""
    text = mtools("mtype", "-i", img(image), "::/WINDOWS/WIN.INI").stdout.replace("\r\n", "\n")
    load = run = ""
    section = ""
    for line in text.split("\n"):
        s = line.strip()
        if s.startswith("["):
            section = s.lower()
        elif section == "[windows]" and "=" in s:
            k, v = s.split("=", 1)
            if k.strip().lower() == "load":
                load = v.strip()
            elif k.strip().lower() == "run":
                run = v.strip()
    set_win_ini(image, load, run)


def set_autoexec(image, win_args):
    """AUTOEXEC.BAT that starts Windows, optionally with a program and
    arguments (WIN.INI run= cannot pass arguments)."""
    cmd = "WIN"
    if win_args:
        cmd += " " + win_args
    text = "@ECHO OFF\r\nPATH C:\\WINDOWS\r\nSET TEMP=C:\\WINDOWS\\TEMP\r\n%s\r\n" % cmd
    with tempfile.NamedTemporaryFile("w", delete=False, newline="", encoding="latin-1") as f:
        f.write(text)
        tmp = f.name
    mtools("mcopy", "-o", "-i", img(image), tmp, "::/AUTOEXEC.BAT")
    os.unlink(tmp)


def deploy(image, theme, engine, gallery, trace=False):
    mtools("mmd", "-i", img(image), "::/RETHEME", check=False)
    for f in ("RETHEME.DLL", "RETHEME.EXE", "GALLERY.EXE"):
        mtools("mcopy", "-o", "-i", img(image), os.path.join(ROOT, "build/win16", f), "::/RETHEME/" + f)
    mtools("mcopy", "-o", "-s", "-i", img(image), os.path.join(ROOT, "themes"), "::/RETHEME/")
    ini = "[Engine]\r\nTheme=C:\\RETHEME\\THEMES\\%s\r\nTrace=%d\r\n" % (theme, trace)
    with tempfile.NamedTemporaryFile("w", delete=False) as f:
        f.write(ini)
        tmp = f.name
    mtools("mcopy", "-o", "-i", img(image), tmp, "::/RETHEME/RETHEME.INI")
    os.unlink(tmp)
    load = "C:\\RETHEME\\RETHEME.EXE" if engine else ""
    run = "C:\\RETHEME\\GALLERY.EXE" if gallery is True else ""
    set_win_ini(image, load, run)
    if isinstance(gallery, str):
        set_autoexec(image, "C:\\RETHEME\\GALLERY.EXE " + gallery)


def compare(shot, gold, diff_path):
    """Returns the number of differing pixels; writes a diff image (changed
    pixels red over a faded copy of the golden image)."""
    from PIL import Image, ImageChops
    a = Image.open(shot).convert("RGB")
    b = Image.open(gold).convert("RGB")
    if a.size != b.size:
        return a.size[0] * a.size[1]
    mask = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v else 0)
    n = sum(1 for v in mask.getdata() if v)
    if n:
        faded = Image.blend(b, Image.new("RGB", b.size, (255, 255, 255)), 0.6)
        faded.paste((255, 0, 0), mask=mask)
        faded.save(diff_path)
    return n


class Mouse:
    """Relative PS/2 mouse: home to the top-left corner, then move.
    Windows 3.x applies acceleration above a threshold, so moves are
    split into small steps (WIN.INI MouseSpeed=0 avoids it entirely)."""

    def __init__(self, sock):
        self.sock = sock
        self.x = self.y = None

    def home(self):
        cmds = ["mouse_move -40 -40"] * 30
        qmon.run(self.sock, cmds, delay=0.03)
        self.x = self.y = 0

    def move(self, x, y):
        if self.x is None:
            self.home()
        cmds = []
        dx, dy = x - self.x, y - self.y
        while dx or dy:
            sx = max(-20, min(20, dx))
            sy = max(-20, min(20, dy))
            cmds.append("mouse_move %d %d" % (sx, sy))
            dx -= sx
            dy -= sy
        qmon.run(self.sock, cmds, delay=0.03)
        self.x, self.y = x, y


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--theme", default="WIN95")
    ap.add_argument("--no-engine", action="store_true")
    ap.add_argument("--no-gallery", action="store_true")
    ap.add_argument("--gallery", help="start GALLERY.EXE with these arguments")
    ap.add_argument("--boot", type=float, default=40, help="seconds to wait for Windows")
    ap.add_argument("--step", action="append", default=[])
    ap.add_argument("--script", help="file with one step per line ('#' comments); options in a"
                    " leading '#!' line are added to the command line")
    ap.add_argument("--memory", default="32")
    ap.add_argument("--serial", action="store_true", help="log COM1 (debug kernel output) to DIR/serial.log")
    ap.add_argument("--trace", type=int, nargs="?", const=1, default=0,
                    help="engine trace level: 1 = GDI calls in paint contexts, 2 = all GDI calls")
    ap.add_argument("--golden", help="directory with golden screenshots")
    ap.add_argument("--update-golden", action="store_true")
    ap.add_argument("--tolerance", type=int, default=0, help="differing pixels allowed per screenshot")
    ap.add_argument("--floppy", help="disk image for drive A:")
    ap.add_argument("--run", help="start Windows with this command line (WIN <run>)")
    ap.add_argument("--no-deploy", action="store_true", help="do not copy the engine into the image")
    ap.add_argument("--keep", help="save the disk image after the run to this file")
    a = ap.parse_args()
    if a.script:
        steps = []
        with open(a.script) as f:
            for line in f:
                line = line.strip()
                if line.startswith("#!"):
                    a = ap.parse_args(sys.argv[1:] + line[2:].split())
                elif line and not line.startswith("#"):
                    steps.append(line)
        a.step = steps + a.step

    os.makedirs(a.out, exist_ok=True)
    work = tempfile.mkdtemp()
    image = os.path.join(work, "run.img")
    subprocess.run(["cp", "--sparse=always", a.base, image], check=True)
    if a.no_deploy:
        set_win_ini_keep(image)
        set_autoexec(image, a.run)
    else:
        gallery = a.gallery if a.gallery is not None else (not a.no_gallery)
        deploy(image, a.theme, not a.no_engine, gallery, a.trace)
        if a.run:
            set_autoexec(image, a.run)
    # UNIX socket paths are limited to ~100 characters: keep it short
    sockdir = tempfile.mkdtemp(prefix="rt", dir="/tmp")
    sock = os.path.join(sockdir, "mon")
    q = subprocess.Popen(["qemu-system-i386", "-m", a.memory, "-cpu", "pentium", "-display", "none",
                          "-drive", "file=%s,if=ide,format=raw" % image,
                          "-monitor", "unix:%s,server,nowait" % sock]
                         + (["-serial", "file:%s" % os.path.join(a.out, "serial.log")] if a.serial else [])
                         + (["-drive", "file=%s,if=floppy,format=raw,readonly=on" % os.path.abspath(a.floppy)]
                            if a.floppy else []),
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(a.boot)
        mouse = Mouse(sock)
        for step in a.step:
            parts = step.split()
            if parts[0] == "shot":
                ppm = os.path.join(work, "s.ppm")
                qmon.run(sock, ["screendump " + ppm])
                time.sleep(0.5)
                subprocess.run(["convert", ppm, os.path.join(a.out, parts[1] + ".png")], check=True)
            elif parts[0] == "mon":
                qmon.run(sock, [" ".join(parts[1:])])
            elif parts[0] == "move":
                mouse.move(int(parts[1]), int(parts[2]))
            elif parts[0] == "drag":
                mouse.move(int(parts[1]), int(parts[2]))
                qmon.run(sock, ["mouse_button 1"])
                time.sleep(0.3)
                mouse.move(int(parts[3]), int(parts[4]))
                time.sleep(0.3)
                qmon.run(sock, ["mouse_button 0"])
            elif parts[0] == "click":
                if len(parts) == 3:
                    mouse.move(int(parts[1]), int(parts[2]))
                # press and release within ~50 ms: shorter than USER's
                # auto-repeat delay, so a click is exactly one step
                qmon.run(sock, ["mouse_button 1", "mouse_button 0"], delay=0.05)
            else:
                time.sleep(float(parts[0]))
    finally:
        q.kill()
        q.wait()
    if a.keep:
        subprocess.run(["cp", "--sparse=always", image, a.keep], check=True)
    log = mtools("mtype", "-i", img(image), "::/RETHEME/RETHEME.LOG", check=False)
    with open(os.path.join(a.out, "RETHEME.LOG"), "w") as f:
        f.write(log.stdout)
    print(log.stdout or "(no RETHEME.LOG)")
    shutil.rmtree(work)
    shutil.rmtree(sockdir, ignore_errors=True)

    failed = 0
    if a.golden:
        os.makedirs(a.golden, exist_ok=True)
        for step in a.step:
            parts = step.split()
            if parts[0] != "shot":
                continue
            shot = os.path.join(a.out, parts[1] + ".png")
            gold = os.path.join(a.golden, parts[1] + ".png")
            if a.update_golden:
                shutil.copy(shot, gold)
                print("golden updated: %s" % gold)
            elif not os.path.exists(gold):
                print("NO GOLDEN  %s" % parts[1])
                failed += 1
            else:
                n = compare(shot, gold, os.path.join(a.out, parts[1] + ".diff.png"))
                ok = n <= a.tolerance
                failed += not ok
                print("%s  %s (%d pixels differ)" % ("ok  " if ok else "FAIL", parts[1], n))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
