"""Baut assets/icon.ico, icon.icns und icon.png aus den PNGs von `Sysline --icons <ordner>` (gleiches Logo wie im Programm).
Aufruf: QT_QPA_PLATFORM=offscreen build/sysline --icons /tmp/ic && python3 tools/make_icons.py /tmp/ic"""
import shutil
import struct
import sys

src = sys.argv[1]
def png(s):
    return open(f"{src}/icon-{s}.png", "rb").read()

sizes = [16, 24, 32, 48, 64, 128, 256]
data = [png(s) for s in sizes]
ico = struct.pack("<HHH", 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, d in zip(sizes, data):
    ico += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(d), off)
    off += len(d)
open("assets/icon.ico", "wb").write(ico + b"".join(data))
chunks = b""
for tag, sz in ((b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)):
    d = png(sz)
    chunks += tag + struct.pack(">I", len(d) + 8) + d
open("assets/icon.icns", "wb").write(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)
shutil.copy(f"{src}/icon-256.png", "assets/icon.png")
print("ok")
