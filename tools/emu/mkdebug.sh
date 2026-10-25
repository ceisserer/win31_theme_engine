#!/bin/sh
# Makes a copy of a Windows 3.1 image with the SDK's debug KERNEL/USER/GDI
# and their symbol files. Debug output goes to COM1 (run.py --serial).
#
#   mkdebug.sh <win31.img> <sdk WINDEV/DEBUG directory> <out.img>
set -eu
IN=$1
DBG=$2
OUT=$3
cp --sparse=always "$IN" "$OUT"
C="$OUT@@32256"
for f in KRNL386.EXE KRNL286.EXE USER.EXE GDI.EXE KRNL386.SYM KRNL286.SYM USER.SYM GDI.SYM; do
    mcopy -o -i "$C" "$DBG/$f" "::/WINDOWS/SYSTEM/$f"
done
echo "debug image: $OUT"
