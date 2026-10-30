"""
prints the bandwidth summary of the periodic frames, see comm/telemetry.h

usage: telemetry.py OBJCOPY OBJECT
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SECTION = '.rodata.telemetrySummary'


def read(objcopy, obj):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'summary.bin'
        subprocess.run([objcopy, '-O', 'binary', f'--only-section={SECTION}', obj, out], check=True)
        return out.read_bytes()


def text(raw):
    return raw.rstrip(b'\0').decode()


def main(objcopy, obj):
    data = read(objcopy, obj)
    name, capacity, overhead, maxLoad, count = struct.unpack_from('<16sIIII', data)
    print(f"telemetry over {text(name)}: {capacity} B/s, {overhead} B per frame")
    total = 0
    for i in range(count):
        fname, fid, period, size = struct.unpack_from('<12sIII', data, 32 + 24 * i)
        rate = size * 1000 / period
        total += rate
        print(f"  {text(fname):<12} id {fid:2}  every {period:4} ms  {size:4} B"
              f"  {rate:7.0f} B/s  {100 * rate / capacity:5.1f} %")
    print(f"  total {total:.0f} B/s = {100 * total / capacity:.1f} % of the link (limit {maxLoad / 10:.0f} %)")


if __name__ == '__main__':
    main(*sys.argv[1:])
