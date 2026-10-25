#!/usr/bin/env python3
"""Packages a Retheme16 release with Win16Setup (external/win16setup).

  mkrelease.py [--version X.Y.Z] [--out build/release]

Needs `make win16` and `make preview` first (`make release` does all).
Stages the installation files (programs, README.TXT, themes with preview
images) and the SETUP.INF from dist/SETUP.INF, then lets Win16Setup's
packer write to the output directory:

  retheme16-X.Y.Z/             the installation files (with SETUP.EXE)
  retheme16-X.Y.Z.zip          unzip, run SETUP.EXE
  retheme16-X.Y.Z-disk.img     1.44 MB floppy image with the same files
  SHA256SUMS
  RELEASE_NOTES.md             the CHANGELOG.md section of the version

The ZIP and the floppy image are reproducible: file dates come from
SOURCE_DATE_EPOCH (default: the time of the last git commit).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W16 = os.path.join(ROOT, "build", "win16")
PACK = os.path.join(ROOT, "external", "win16setup", "tools", "w16pack.py")
PROGRAMS = ["RETHEME.EXE", "RETHEME.DLL", "GALLERY.EXE"]
DEFAULT_THEME = "WIN95"


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("latin-1")


def themes():
    d = os.path.join(ROOT, "themes")
    names = sorted(t for t in os.listdir(d) if os.path.isfile(os.path.join(d, t, "THEME.INI")))
    names.remove(DEFAULT_THEME)
    return [DEFAULT_THEME] + names


def stage(version, files):
    """The installation files in files/; returns the theme names."""
    shutil.rmtree(files, ignore_errors=True)
    os.makedirs(files)
    for f in PROGRAMS:
        shutil.copy(os.path.join(W16, f), os.path.join(files, f))
    with open(os.path.join(ROOT, "dist", "README.TXT")) as f:
        readme = f.read().replace("@VERSION@", version)
    with open(os.path.join(files, "README.TXT"), "wb") as f:
        f.write(crlf(readme))
    with open(os.path.join(ROOT, "LICENSE")) as f:
        lic = f.read()
    with open(os.path.join(files, "COPYING.TXT"), "wb") as f:
        f.write(crlf(lic))
    tl = themes()
    for t in tl:
        src = os.path.join(ROOT, "themes", t)
        dst = os.path.join(files, "THEMES", t)
        shutil.copytree(src, dst)
        # the host renderer's window scene as Setup's preview
        png = os.path.join(ROOT, "build", "preview", t, "window.png")
        if not os.path.exists(png):
            sys.exit("missing %s: run 'make preview' first" % png)
        subprocess.run([sys.executable, PACK, "preview", png, os.path.join(dst, "PREVIEW.BMP")], check=True)
    return tl


def release_notes(version, path):
    notes = ""
    cl = os.path.join(ROOT, "CHANGELOG.md")
    if os.path.exists(cl):
        with open(cl) as f:
            text = f.read()
        m = re.search(r"^## \[?%s\]?.*?$(.*?)(?=^## |\Z)" % re.escape(version), text, re.M | re.S)
        if m:
            notes = m.group(1).strip() + "\n"
    with open(path, "w") as f:
        f.write(notes or "Retheme16 %s\n" % version)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "release"))
    a = ap.parse_args()
    with open(os.path.join(ROOT, "VERSION")) as f:
        file_version = f.read().strip()
    version = a.version or file_version
    if version != file_version:
        sys.exit("version %s does not match VERSION (%s)" % (version, file_version))
    if not os.path.exists(PACK):
        sys.exit("external/win16setup is empty: git submodule update --init")
    for f in PROGRAMS + ["SETUP.EXE"]:
        if not os.path.exists(os.path.join(W16, f)):
            sys.exit("missing build/win16/%s: run 'make win16' first" % f)
    shutil.rmtree(a.out, ignore_errors=True)
    os.makedirs(a.out)
    files = os.path.join(a.out, "files")
    tl = stage(version, files)

    with open(os.path.join(ROOT, "dist", "SETUP.INF")) as f:
        inf = f.read().replace("@THEMES@", "\n".join("%d=%s" % (i + 1, t) for i, t in enumerate(tl)))
    tmpl = os.path.join(a.out, "SETUP.INF")
    with open(tmpl, "w") as f:
        f.write(inf)
    env = dict(os.environ)
    if "SOURCE_DATE_EPOCH" not in env:
        r = subprocess.run(["git", "-C", ROOT, "log", "-1", "--format=%ct"], capture_output=True, text=True)
        if r.stdout.strip():
            env["SOURCE_DATE_EPOCH"] = r.stdout.strip()
    subprocess.run([sys.executable, PACK, "pack", "--inf", tmpl, "--files", files,
                    "--setup", os.path.join(W16, "SETUP.EXE"), "--out", a.out,
                    "--name", "retheme16-%s" % version, "--label", "RETHEME16", "--version", version],
                   check=True, env=env)
    shutil.rmtree(files)
    os.unlink(tmpl)
    release_notes(version, os.path.join(a.out, "RELEASE_NOTES.md"))


if __name__ == "__main__":
    main()
