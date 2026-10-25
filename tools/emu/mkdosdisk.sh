#!/bin/sh
# Creates a bootable MS-DOS hard-disk image for QEMU without any typing:
# a copy of DOS setup disk 1 boots with an AUTOEXEC.BAT that runs
# "FDISK /MBR" and "SYS C:" on a FAT16 partition prepared on the host.
#
#   mkdosdisk.sh <dos-disk1.img> <out.img> [size-MB]
#
# Needs: qemu-system-i386, mtools, dosfstools, sfdisk. Size <= 500 MB
# (MS-DOS 6 uses CHS: 16 heads, 63 sectors).
set -eu
DISK1=$1
OUT=$2
MB=${3:-256}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

SECTORS=$((MB * 1024 * 2))
PART_START=63
PART_SECTORS=$((SECTORS - PART_START))

truncate -s 0 "$OUT"
truncate -s $((SECTORS * 512)) "$OUT"
printf 'start=%d, size=%d, type=6, bootable\n' $PART_START $PART_SECTORS | sfdisk -q --no-reread "$OUT"

truncate -s $((PART_SECTORS * 512)) "$WORK/part.img"
mkfs.fat -F 16 -g 16/63 -h $PART_START -n RETHEME16 "$WORK/part.img" >/dev/null
dd if="$WORK/part.img" of="$OUT" bs=512 seek=$PART_START conv=notrunc status=none

cp "$DISK1" "$WORK/floppy.img"
# Some archived 1.44 MB images lack their empty trailing sectors; QEMU
# then guesses the wrong floppy geometry and the disk does not boot.
truncate -s 1474560 "$WORK/floppy.img"
printf '@ECHO OFF\r\nFDISK /MBR\r\nSYS C:\r\nECHO OK> C:\\SYSDONE.TXT\r\n' > "$WORK/autoexec.bat"
mcopy -o -i "$WORK/floppy.img" "$WORK/autoexec.bat" ::/AUTOEXEC.BAT

timeout 120 qemu-system-i386 -m 16 -display none -no-reboot \
    -drive file="$WORK/floppy.img",if=floppy,format=raw \
    -drive file="$OUT",if=ide,format=raw -boot a &
QPID=$!
# wait until SYS has written its marker
i=0
while [ $i -lt 100 ]; do
    sleep 1
    if mdir -i "$OUT@@$((PART_START * 512))" ::/SYSDONE.TXT >/dev/null 2>&1; then
        sleep 2
        break
    fi
    i=$((i + 1))
done
kill $QPID 2>/dev/null || true
wait $QPID 2>/dev/null || true
mdir -i "$OUT@@$((PART_START * 512))" ::/ 
mdel -i "$OUT@@$((PART_START * 512))" ::/SYSDONE.TXT
