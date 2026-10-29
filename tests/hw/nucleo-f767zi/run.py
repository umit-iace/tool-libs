#!/usr/bin/env python3
"""tool-libs hardware tests on a NUCLEO-F767ZI

builds the test firmware, flashes it with openocd, runs the on-target tests
over the ST-Link virtual COM port and drives the Min/UART echo test.

    ./run.py                # build, flash, run once
    ./run.py --repeat 10    # run 10 times (reset in between), e.g. for races
    ./run.py --no-build --no-flash

exit code: 0 all passed (skips allowed), 1 test failures, 2 setup/hang
"""
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("needs pyserial: pip install pyserial")

HERE = Path(__file__).resolve().parent
OPENOCD_BOARD = "board/st_nucleo_f7.cfg"
ST_VID = 0x0483

COLOR = {"PASS": "\033[32m", "FAIL": "\033[31m", "WARN": "\033[33m",
         "SKIP": "\033[36m", "END": "\033[0m"}
if not sys.stdout.isatty():
    COLOR = {k: "" for k in COLOR}


class Hang(Exception):
    pass


class Log:
    def __init__(self, path):
        self.f = open(path, "w")

    def __call__(self, line, color=None):
        self.f.write(line + "\n")
        self.f.flush()
        if color:
            line = COLOR[color] + line + COLOR["END"]
        print(line, flush=True)


# --- Min framing, mirrors comm/min.h -------------------------------------
HEADER, STUFF = 0xAA, 0x55


def min_encode(fid, payload):
    body = bytes([fid & 0x3F, len(payload)]) + bytes(payload)
    body += zlib.crc32(body).to_bytes(4, "big")
    out = bytearray([HEADER] * 3)
    seen = 0
    for b in body:
        out.append(b)
        if b == HEADER:
            seen += 1
            if seen == 2:
                out.append(STUFF)
                seen = 0
        else:
            seen = 0
    out.append(STUFF)  # EOF
    return bytes(out)


class MinDecoder:
    """returns (id, payload, end offset in chunk) for each valid frame"""

    def __init__(self):
        self.state = "sof"
        self.headers = 0

    def feed(self, chunk):
        frames = []
        for i, b in enumerate(chunk):
            if self.headers == 2:
                self.headers = 0
                if b == HEADER:
                    self.state = "id"
                    continue
                if b == STUFF:
                    continue
                self.state = "sof"
                continue
            self.headers = self.headers + 1 if b == HEADER else 0
            st = self.state
            if st == "id":
                self.id, self.crc = b & 0x3F, bytearray([b])
                self.state = "len"
            elif st == "len":
                self.n, self.payload = b, bytearray()
                self.crc.append(b)
                self.sum = bytearray()
                self.state = "payload" if b else "crc"
            elif st == "payload":
                self.payload.append(b)
                self.crc.append(b)
                if len(self.payload) == self.n:
                    self.state = "crc"
            elif st == "crc":
                self.sum.append(b)
                if len(self.sum) == 4:
                    if int.from_bytes(self.sum, "big") == zlib.crc32(self.crc):
                        frames.append((self.id, bytes(self.payload), i))
                    self.state = "sof"
        return frames


# --- board handling -----------------------------------------------------
def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def build(args):
    gen = ["-G", "Ninja"] if shutil.which("ninja") else []
    run(["cmake", "-S", HERE, "-B", args.build_dir, *gen,
         f"-DHWTEST_BAUD={args.baud}"], stdout=subprocess.DEVNULL)
    run(["cmake", "--build", args.build_dir])


def openocd(*cmds):
    c = []
    for cmd in cmds:
        c += ["-c", cmd]
    run(["openocd", "-f", OPENOCD_BOARD, *c],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def flash(elf):
    openocd(f"program {elf} verify reset exit")


def reset():
    openocd("init", "reset run", "shutdown")


def diagnose(elf, log):
    """attach to the hanging target and print where it is"""
    log("--- target state (openocd + gdb) ---", "WARN")
    gdb = shutil.which("arm-none-eabi-gdb") or shutil.which("gdb-multiarch")
    if not gdb:
        log("no arm-none-eabi-gdb found")
        return
    remote = (f"target extended-remote | openocd -c 'gdb_port pipe' "
              f"-c 'log_output /dev/null' -f {OPENOCD_BOARD}")
    cmd = [gdb, "-batch", "-nx", str(elf),
           "-ex", remote, "-ex", "monitor halt",
           "-ex", "bt",
           "-ex", "info registers pc lr sp",
           "-ex", "printf \"xPSR 0x%08x PRIMASK %d\\n\", $xPSR, $primask",
           "-ex", "printf \"CFSR 0x%08x HFSR 0x%08x BFAR 0x%08x\\n\", "
                  "*(unsigned*)0xE000ED28, *(unsigned*)0xE000ED2C, "
                  "*(unsigned*)0xE000ED38",
           "-ex", "detach"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        for line in (out.stdout + out.stderr).splitlines():
            log("  " + line)
    except subprocess.TimeoutExpired:
        log("  gdb timed out")


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid == ST_VID:
            return p.device
    raise Hang("no ST-Link virtual COM port found, board connected?")


# --- test session -------------------------------------------------------
class Session:
    def __init__(self, port, baud, timeout, log):
        self.ser = serial.Serial(port, baud, timeout=0.05)
        self.timeout = timeout
        self.log = log
        self.buf = b""

    def readline(self):
        deadline = time.monotonic() + self.timeout
        while b"\n" not in self.buf:
            if time.monotonic() > deadline:
                raise Hang(f"no output for {self.timeout}s")
            self.buf += self.ser.read(4096)
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode(errors="replace").rstrip("\r")

    def start(self):
        self.ser.reset_input_buffer()
        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() > deadline:
                raise Hang("firmware did not start")
            self.ser.write(b"g")
            t = time.monotonic() + 0.5
            while time.monotonic() < t:
                self.buf += self.ser.read(4096)
                while b"\n" in self.buf:
                    line, self.buf = self.buf.split(b"\n", 1)
                    line = line.decode(errors="replace").rstrip("\r")
                    if line.startswith("HWT START"):
                        self.log(line)
                        return

    def tests(self):
        """parse report lines until the echo phase, returns results"""
        results = {}
        while True:
            line = self.readline()
            if line.startswith("HWT END "):
                _, _, name, res, ms = line.split()
                results[name] = res
                self.log(f"{res:4} {name} ({ms} ms)", res)
            elif line.startswith("HWT FAIL"):
                self.log("     " + line[4:], "FAIL")
            elif line.startswith("HWT WARN"):
                self.log("     " + line[4:], "WARN")
            elif line.startswith("HWT INFO"):
                self.log("     " + line[9:])
            elif line.startswith("HWT BEGIN"):
                self.current = line.split()[2]
            elif line.startswith("HWT RESULT"):
                self.log(line)
            elif line.startswith("HWT ECHO START"):
                return results
            elif line:
                self.log("     ? " + line)

    def echo(self, frames, burst):
        """Min frames through UART::HW & Min::In/Out and back"""
        rng = random.Random(1234)
        dec = MinDecoder()
        problems = []
        sent = []
        for i in range(frames):
            n = rng.choice([0, 1, 2, 127, 128]) if i % 10 == 0 else rng.randrange(129)
            if i % 7 == 0:
                payload = bytes([HEADER] * n)  # worst case stuffing
            else:
                payload = bytes(rng.randrange(256) for _ in range(n))
            sent.append((rng.randrange(0x3E), payload))

        pending, nxt, got = [], 0, 0
        window = 4
        leftover = self.buf
        self.buf = b""
        raw = b""  # firmware text after a failure (ECHO TIMEOUT) ends up here
        deadline = time.monotonic() + self.timeout
        while got < frames:
            while nxt < frames and len(pending) < window:
                self.ser.write(min_encode(*sent[nxt]))
                pending.append(sent[nxt])
                nxt += 1
            chunk = leftover or self.ser.read(4096)
            leftover = b""
            raw += chunk
            if chunk:
                deadline = time.monotonic() + self.timeout
            elif time.monotonic() > deadline:
                problems.append(f"echo stalled after {got}/{frames} frames")
                break
            for fid, payload, _ in dec.feed(chunk):
                if not pending:
                    problems.append(f"unexpected frame id {fid}")
                    continue
                want = pending.pop(0)
                if (fid, payload) != want:
                    problems.append(f"frame {got}: got id {fid} len {len(payload)}, "
                                    f"want id {want[0]} len {len(want[1])}")
                got += 1

        # burst, nothing echoed: firmware has to keep up with the line rate
        blob, nbytes = b"", 0
        for _ in range(burst):
            n = rng.randrange(129)
            nbytes += n
            blob += min_encode(0x3E, bytes(rng.randrange(256) for _ in range(n)))
        t0 = time.monotonic()
        self.ser.write(blob)
        self.ser.flush()
        rate = len(blob) / max(time.monotonic() - t0, 1e-3)

        self.ser.write(min_encode(0x3F, b""))
        reply = None
        deadline = time.monotonic() + self.timeout
        while reply is None:
            chunk = self.ser.read(4096)
            raw += chunk
            if not chunk and time.monotonic() > deadline:
                problems.append("no reply to the end frame")
                break
            for fid, payload, end in dec.feed(chunk):
                if fid == 0x3F:
                    reply = payload
                    rest = chunk[end + 1:]
                    self.buf = rest[1:] if rest[:1] == bytes([STUFF]) else rest
        if reply is None:
            self.buf = raw
        else:
            echoed, counted, cbytes = (int.from_bytes(reply[i:i + 4], "little")
                                       for i in (0, 4, 8))
            if echoed != frames:
                problems.append(f"firmware echoed {echoed} of {frames} frames")
            if counted != burst or cbytes != nbytes:
                problems.append(f"burst: firmware got {counted}/{burst} frames, "
                                f"{cbytes}/{nbytes} bytes")
        self.log(f"     {frames} frames echoed, {burst} burst frames "
                 f"({len(blob)} bytes, host wrote {rate:.0f} B/s)")
        for p in problems:
            self.log("     " + p, "FAIL")
        res = "FAIL" if problems else "PASS"
        self.log(f"{res:4} uart_min_echo", res)
        return res

    def finish(self):
        """firmware side of the echo test, False if the UART had to abort"""
        ok = True
        while True:
            line = self.readline()
            # after a failed echo, text may follow binary garbage
            line = line[line.find("HWT "):] if "HWT " in line else line
            if line.startswith("HWT DONE"):
                return ok
            if line.startswith("HWT ECHO"):
                aborts = re.search(r"errors=(\d+).*tx_timeouts=(\d+)", line)
                bad = aborts and aborts.groups() != ("0", "0")
                ok = ok and not bad
                self.log("     " + line[4:], "FAIL" if bad else None)


def once(args, elf, log):
    port = args.port or find_port()
    s = Session(port, args.baud, args.timeout, log)
    try:
        s.start()
        results = s.tests()
        results["uart_min_echo"] = s.echo(args.frames, args.burst)
        if not s.finish():
            results["uart_min_echo"] = "FAIL"
        return results
    finally:
        s.ser.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", default=HERE / "build", type=Path)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-flash", action="store_true", help="only reset the board")
    ap.add_argument("--port", help="serial port, default: autodetect ST-Link")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=30, help="max silence [s]")
    ap.add_argument("--frames", type=int, default=300, help="Min echo frames")
    ap.add_argument("--burst", type=int, default=200, help="Min burst frames")
    ap.add_argument("--repeat", type=int, default=1)
    args = ap.parse_args()

    elf = args.build_dir / "hwtest.elf"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    logpath = args.build_dir / datetime.now().strftime("hwtest-%Y%m%d-%H%M%S.log")
    log = Log(logpath)

    try:
        if not args.no_build:
            build(args)
        if not args.no_flash:
            log(f"flashing {elf}")
            flash(elf)
    except subprocess.CalledProcessError as e:
        log(f"setup failed: {e}", "FAIL")
        return 2

    failed_runs, total = 0, {}
    for n in range(args.repeat):
        if args.repeat > 1:
            log(f"=== run {n + 1}/{args.repeat}")
        try:
            if n or args.no_flash:
                reset()
            results = once(args, elf, log)
        except Hang as e:
            log(f"HANG {e}", "FAIL")
            diagnose(elf, log)
            log(f"log: {logpath}")
            return 2
        except subprocess.CalledProcessError as e:
            log(f"reset failed: {e}", "FAIL")
            return 2
        fails = [k for k, v in results.items() if v == "FAIL"]
        for k, v in results.items():
            total.setdefault(k, []).append(v)
        failed_runs += bool(fails)
        npass = sum(v == "PASS" for v in results.values())
        nskip = sum(v == "SKIP" for v in results.values())
        log(f"--- {npass} passed, {len(fails)} failed, {nskip} skipped"
            + (f": {' '.join(fails)}" if fails else ""),
            "FAIL" if fails else "PASS")

    if args.repeat > 1:
        flaky = {k: v.count("FAIL") for k, v in total.items() if "FAIL" in v}
        log(f"=== {args.repeat - failed_runs}/{args.repeat} runs passed"
            + "".join(f"\n    {k}: failed {c}x" for k, c in flaky.items()),
            "FAIL" if failed_runs else "PASS")
    log(f"log: {logpath}")
    return 1 if failed_runs else 0


if __name__ == "__main__":
    sys.exit(main())
