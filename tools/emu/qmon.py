#!/usr/bin/env python3
"""Sends commands to a QEMU human monitor on a UNIX socket.

  qmon.py <socket> "screendump /tmp/x.ppm" ["sendkey ret" ...]
"""
import socket
import sys
import time


def run(path, cmds, delay=0.2):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(path)
    s.settimeout(2)
    out = b""

    def drain():
        nonlocal out
        try:
            while True:
                d = s.recv(65536)
                if not d:
                    break
                out += d
                if out.endswith(b"(qemu) "):
                    break
        except socket.timeout:
            pass

    drain()
    for c in cmds:
        s.sendall(c.encode() + b"\n")
        time.sleep(delay)
        drain()
    s.close()
    return out.decode(errors="replace")


if __name__ == "__main__":
    print(run(sys.argv[1], sys.argv[2:]))
