#!/usr/bin/env python3
"""Installs Windows 3.1 unattended onto an MS-DOS disk image.

  mkwin31.py --dos DOS.IMG --himem HIMEM.SYS --out WIN31.IMG [--lang deu --kblayout gerdll]
             DISK1.IMG ... DISK6.IMG

The installation disks are copied into C:\\W31SRC and Setup runs in batch
mode (SETUP /H with a .SHH file: VGA, PS/2 mouse, US keyboard, no network,
no tutorial), then exits to DOS. HIMEM.SYS must be an uncompressed copy (the
one on the setup disks is compressed).
"""
import argparse
import os
import shutil
import subprocess
import tempfile
import time

PART = 63 * 512

SHH = r"""[sysinfo]
showsysinfo=no

[configuration]
machine = ibm_compatible
display = vga
mouse = ps2mouse
network = nonet
keyboard = t4s0enha
language = {lang}
kblayout = {kblayout}

[windir]
c:\windows

[userinfo]
"Retheme16 Test"

[dontinstall]
readmes
screensavers

[printers]

[endinstall]
configfiles = modify
endopt = exit
"""

AUTOEXEC = "@ECHO OFF\r\nC:\r\nCD \\W31SRC\r\nSETUP /H:C:\\W31SRC\\RT.SHH\r\nECHO OK> C:\\SETUPOK.TXT\r\n"
CONFIG = "DEVICE=C:\\HIMEM.SYS\r\nFILES=40\r\nBUFFERS=30\r\n"
FINAL_CONFIG = "DEVICE=C:\\WINDOWS\\HIMEM.SYS\r\nFILES=40\r\nBUFFERS=30\r\nSTACKS=9,256\r\n"
FINAL_AUTOEXEC = "@ECHO OFF\r\nPATH C:\\WINDOWS\r\nSET TEMP=C:\\WINDOWS\\TEMP\r\nWIN\r\n"


def m(*args, check=True):
    return subprocess.run(list(args), check=check, capture_output=True, encoding="latin-1")


def put(image, text, dest):
    with tempfile.NamedTemporaryFile("w", delete=False, newline="", encoding="latin-1") as f:
        f.write(text.replace("\n", "\r\n") if "\r\n" not in text else text)
        tmp = f.name
    m("mcopy", "-o", "-i", "%s@@%d" % (image, PART), tmp, dest)
    os.unlink(tmp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dos", required=True)
    ap.add_argument("--himem", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--timeout", type=int, default=1200)
    ap.add_argument("--lang", default="enu", help="[language] key of SETUP.INF (German: deu)")
    ap.add_argument("--kblayout", default="nodll", help="[keyboard.tables] key (German: gerdll)")
    ap.add_argument("disks", nargs="+")
    a = ap.parse_args()

    out = a.out
    subprocess.run(["cp", "--sparse=always", a.dos, out], check=True)
    c = "%s@@%d" % (out, PART)
    m("mmd", "-i", c, "::/W31SRC")
    work = tempfile.mkdtemp()
    for d in a.disks:
        fl = os.path.join(work, "fl.img")
        shutil.copy(d, fl)
        os.truncate(fl, 1474560)
        files = os.path.join(work, "files")
        shutil.rmtree(files, ignore_errors=True)
        os.makedirs(files)
        m("mcopy", "-s", "-n", "-i", fl, "::/*", files)
        for f in os.listdir(files):
            m("mcopy", "-o", "-s", "-i", c, os.path.join(files, f), "::/W31SRC/")
    put(out, SHH.replace("{lang}", a.lang).replace("{kblayout}", a.kblayout), "::/W31SRC/RT.SHH")
    m("mcopy", "-o", "-i", c, a.himem, "::/HIMEM.SYS")
    put(out, CONFIG, "::/CONFIG.SYS")
    put(out, AUTOEXEC, "::/AUTOEXEC.BAT")

    sock = os.path.join(work, "mon.sock")
    q = subprocess.Popen(["qemu-system-i386", "-m", "32", "-cpu", "pentium", "-display", "none",
                          "-drive", "file=%s,if=ide,format=raw" % out,
                          "-monitor", "unix:%s,server,nowait" % sock],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    start = time.time()
    ok = False
    try:
        while time.time() - start < a.timeout:
            time.sleep(10)
            if m("mdir", "-i", c, "::/SETUPOK.TXT", check=False).returncode == 0:
                time.sleep(5)
                ok = True
                break
    finally:
        if not ok:
            subprocess.run([os.path.join(os.path.dirname(os.path.abspath(__file__)), "qmon.py"), sock,
                            "screendump %s" % os.path.join(os.path.dirname(os.path.abspath(out)), "setup-fail.ppm")])
        q.kill()
        q.wait()
    shutil.rmtree(work)
    if not ok:
        raise SystemExit("setup did not finish (screen: setup-fail.ppm)")
    # final boot configuration: start Windows. No SMARTDRV: test VMs are
    # killed, not shut down, and write-behind caching would lose data.
    m("mdel", "-i", c, "::/SETUPOK.TXT")
    m("mdel", "-i", c, "::/HIMEM.SYS")
    m("mdeltree", "-i", c, "::/W31SRC")
    put(out, FINAL_CONFIG, "::/CONFIG.SYS")
    put(out, FINAL_AUTOEXEC, "::/AUTOEXEC.BAT")
    print("Windows 3.1 installed in %s after %d s" % (out, time.time() - start))
    print(m("mtype", "-i", c, "::/CONFIG.SYS").stdout)
    print(m("mtype", "-i", c, "::/AUTOEXEC.BAT").stdout)


if __name__ == "__main__":
    main()
