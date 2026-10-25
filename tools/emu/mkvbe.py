#!/usr/bin/env python3
"""Makes a copy of a Windows 3.1 image that uses the VBE SVGA display
driver (github.com/PluMGMK/vbesvga.drv) at a given resolution and depth,
for 256-colour and high-colour tests in QEMU's standard VGA.

  mkvbe.py --base WIN31.IMG --driver DIR --out OUT.IMG [--width 640 --height 480 --depth 8]

DIR holds VBESVGA.DRV, VBEVMDIB.3GR and VDDVBE.386 from the release zip.
"""
import argparse
import os
import subprocess
import tempfile

PART = 63 * 512


def m(*args, check=True):
    return subprocess.run(list(args), check=check, capture_output=True, encoding="latin-1")


def edit_ini(text, changes):
    """changes: {section: {key: value}}; keys are replaced or added."""
    lines = text.replace("\r\n", "\n").split("\n")
    out, section, seen = [], None, set()

    def flush(sec):
        for k, v in changes.get(sec, {}).items():
            if (sec, k.lower()) not in seen:
                out.append("%s=%s" % (k, v))
                seen.add((sec, k.lower()))

    for line in lines:
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            if section is not None:
                flush(section)
            section = s[1:-1].lower()
        elif section in changes and "=" in s:
            k = s.split("=", 1)[0].strip()
            repl = {kk.lower(): (kk, vv) for kk, vv in changes[section].items()}
            if k.lower() in repl:
                line = "%s=%s" % repl[k.lower()]
                seen.add((section, k.lower()))
        out.append(line)
    if section is not None:
        flush(section)
    for sec in changes:
        if not any(x[0] == sec for x in seen):
            out.append("[%s]" % sec)
            flush(sec)
    return "\r\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--driver", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--width", default="640")
    ap.add_argument("--height", default="480")
    ap.add_argument("--depth", default="8")
    a = ap.parse_args()

    subprocess.run(["cp", "--sparse=always", a.base, a.out], check=True)
    c = "%s@@%d" % (a.out, PART)
    for f in ("VBESVGA.DRV", "VBEVMDIB.3GR", "VDDVBE.386"):
        m("mcopy", "-o", "-i", c, os.path.join(a.driver, f), "::/WINDOWS/SYSTEM/" + f)
    ini = m("mtype", "-i", c, "::/WINDOWS/SYSTEM.INI").stdout
    ini = edit_ini(ini, {
        "boot": {"display.drv": "vbesvga.drv", "386grabber": "vbevmdib.3gr"},
        "386enh": {"display": "vddvbe.386"},
        "vbesvga.drv": {"Width": a.width, "Height": a.height, "Depth": a.depth,
                        "SwapBuffersInterval": "0"},
    })
    with tempfile.NamedTemporaryFile("w", delete=False, newline="", encoding="latin-1") as f:
        f.write(ini)
        tmp = f.name
    m("mcopy", "-o", "-i", c, tmp, "::/WINDOWS/SYSTEM.INI")
    os.unlink(tmp)
    print("VBE image: %s (%sx%sx%s)" % (a.out, a.width, a.height, a.depth))


if __name__ == "__main__":
    main()
