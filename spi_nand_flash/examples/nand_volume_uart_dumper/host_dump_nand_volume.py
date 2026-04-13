#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Receive a logical NAND volume dump from the nand_volume_uart_dumper firmware.

1. Same baud as EXAMPLE_DUMP_UART_BAUD (default 921600 in Kconfig; match with -b).
2. Wait for WAIT\\n then READY\\n.
3. Send DMP\\n.
4. Header: **NDLV** + raw stream (legacy), or **NDS1** + sparse SKIP/DATA records (default firmware).
   Sparse mode still produces a **full disk_size** image (skipped ranges filled with 0xFF) for fatfsparse.py.
"""

from __future__ import annotations

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError as e:
    print("This script requires pyserial: pip install pyserial", file=sys.stderr)
    raise SystemExit(1) from e

HANDSHAKE = b"DMP\n"
WAIT_LINE = b"WAIT\n"
READY_LINE = b"READY\n"
HEADER_RAW = b"NDLV"
HEADER_SPA = b"NDS1"

# Reused slab for sparse SKIP writes — avoids allocating ``ln`` bytes per record (very slow for large skips).
_FF_SLAB = bytes([0xFF]) * (256 * 1024)


def _noise_hint(buf: bytes) -> str:
    if len(buf) < 32:
        return ""
    tail = buf[-256:]
    hi = sum(1 for b in tail if b & 0x80)
    ratio = hi / len(tail)
    if 0.25 < ratio < 0.75:
        return (
            " RX looks like wrong-baud noise or a floating RX — use the **dump UART TX** GPIO from "
            "menuconfig, and match baud (-b) to EXAMPLE_DUMP_UART_BAUD (default 921600)."
        )
    return ""


def wait_line(ser: serial.Serial, token: bytes, timeout: float, label: str) -> None:
    deadline = time.monotonic() + timeout
    buf = b""
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk
        if b"FAIL" in buf:
            tail = buf[-min(400, len(buf)) :]
            raise OSError(f"Device reported failure on dump UART. Snippet: {tail!r}")
        if token in buf:
            return
        if not chunk:
            time.sleep(0.02)
    tail = buf[-min(200, len(buf)) :] if buf else b""
    hint = _noise_hint(buf)
    raise OSError(
        f"No {token!r} within {timeout}s ({label}). "
        f"Connect USB-serial **RX** to firmware **TX**, common **GND**, same baud as firmware.{hint} "
        f"Last RX ({len(buf)} bytes): {tail!r}"
    )


def read_header_resync(ser: serial.Serial, max_scan: int = 65536):
    """
    Find NDLV (raw) or NDS1 (sparse) and return:
      ('raw', disk_size, page_size, prefix) or ('sparse', disk_size, page_size, prefix)
    """
    buf = b""
    while len(buf) < max_scan:
        chunk = ser.read(min(1024, max_scan - len(buf)))
        if not chunk:
            raise OSError(
                f"Timeout waiting for header (got {len(buf)} bytes). Tail (hex): {buf[-48:].hex()}"
            )
        buf += chunk
        ir = buf.find(HEADER_RAW)
        is_ = buf.find(HEADER_SPA)
        candidates = []
        if ir >= 0:
            candidates.append((ir, "raw", HEADER_RAW, 12))
        if is_ >= 0:
            candidates.append((is_, "sparse", HEADER_SPA, 16))
        if not candidates:
            if len(buf) > 128:
                buf = buf[-64:]
            continue
        idx, mode, magic, need = min(candidates, key=lambda x: x[0])
        tail = buf[idx + 4 :]
        while len(tail) < need:
            more = ser.read(need - len(tail))
            if not more:
                raise OSError("Short header after magic")
            tail += more
        rest = tail[need:]
        if mode == "raw":
            disk_size, page_size = struct.unpack("<QI", tail[:12])
            return "raw", disk_size, page_size, rest
        _ver, disk_size, page_size = struct.unpack("<IQI", tail[:16])
        return "sparse", disk_size, page_size, rest


def receive_raw(ser, out, disk_size, page_size, prefix: bytes, timeout: float) -> None:
    ser.timeout = timeout
    remaining = disk_size
    block = min(1024 * 1024, max(65536, page_size))
    if prefix:
        if len(prefix) > remaining:
            raise OSError("payload_prefix longer than disk_size")
        out.write(prefix)
        remaining -= len(prefix)
    while remaining > 0:
        chunk = min(block, remaining)
        data = ser.read(chunk)
        if len(data) != chunk:
            raise OSError(f"Incomplete read: need {chunk} got {len(data)}; remaining {remaining}")
        out.write(data)
        remaining -= chunk


def receive_sparse(ser, out, disk_size, page_size, prefix: bytes, timeout: float) -> None:
    """Reassemble SKIP/DATA stream into a full disk_size file (0xFF for SKIP)."""
    ser.timeout = timeout
    buf = prefix
    pos = 0
    read_floor = 64 * 1024

    def ensure(n: int) -> None:
        nonlocal buf
        while len(buf) < n:
            need = n - len(buf)
            chunk = ser.read(max(read_floor, min(1024 * 1024, need)))
            if not chunk:
                raise OSError(f"Sparse timeout at file pos {pos}, need {n} buf bytes have {len(buf)}")
            buf += chunk

    def write_skip_at(off: int, ln: int) -> None:
        if out.tell() != off:
            out.seek(off)
        end = off + ln
        cur = off
        slab = _FF_SLAB
        slab_len = len(slab)
        while cur < end:
            n = min(end - cur, slab_len)
            if n == slab_len:
                out.write(slab)
            else:
                out.write(slab[:n])
            cur += n

    while pos < disk_size:
        ensure(4)
        tag = buf[:4]
        buf = buf[4:]
        payload = b""
        if tag == b"SKIP":
            ensure(12)
            off, ln = struct.unpack("<QI", buf[:12])
            buf = buf[12:]
        elif tag == b"DATA":
            ensure(12)
            off, ln = struct.unpack("<QI", buf[:12])
            buf = buf[12:]
            ensure(ln)
            payload = buf[:ln]
            buf = buf[ln:]
        else:
            raise OSError(f"Bad sparse tag {tag!r} at volume pos {pos}")

        if off != pos:
            raise OSError(f"Sparse sync error: expected offset {pos}, got {off}")
        if ln == 0 or pos + ln > disk_size:
            raise OSError(f"Bad sparse length ln={ln} pos={pos} disk_size={disk_size}")

        if tag == b"SKIP":
            write_skip_at(off, ln)
        else:
            if out.tell() != off:
                out.seek(off)
            out.write(payload)
        pos = off + ln


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("-p", "--port", required=True, help="Serial port (dump UART)")
    p.add_argument("-b", "--baud", type=int, default=115200, help="Baud (match EXAMPLE_DUMP_UART_BAUD)")
    p.add_argument("-o", "--output", required=True, help="Output image path")
    p.add_argument("--wait-timeout", type=float, default=30.0, help="Seconds to wait for WAIT")
    p.add_argument("--ready-timeout", type=float, default=180.0, help="Seconds to wait for READY")
    p.add_argument("--timeout", type=float, default=300.0, help="Timeout while receiving payload")
    args = p.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=2.0)
    ser.reset_input_buffer()

    try:
        wait_line(ser, WAIT_LINE, args.wait_timeout, "UART init")
        wait_line(ser, READY_LINE, args.ready_timeout, "NAND init")
    except OSError as e:
        print(str(e), file=sys.stderr)
        return 1

    ser.reset_input_buffer()
    ser.write(HANDSHAKE)
    ser.flush()
    time.sleep(0.05)

    ser.timeout = 10.0
    try:
        mode, disk_size, page_size, prefix = read_header_resync(ser)
    except OSError as e:
        print(str(e), file=sys.stderr)
        return 1

    print(f"mode={mode} disk_size={disk_size} page_size={page_size} -> {args.output}")

    try:
        with open(args.output, "w+b") as out:
            out.seek(disk_size - 1)
            out.write(b"\x00")
            out.seek(0)
            if mode == "raw":
                receive_raw(ser, out, disk_size, page_size, prefix, args.timeout)
            else:
                receive_sparse(ser, out, disk_size, page_size, prefix, args.timeout)
    except OSError as e:
        print(str(e), file=sys.stderr)
        return 1

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
