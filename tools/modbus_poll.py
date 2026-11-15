#!/usr/bin/env python3
"""Modbus RTU polling tool for the STM32F407 environmental sensing node.

STM32F407 环境数据采集节点 · Modbus RTU 上位机轮询工具

Examples / 用法:
    python modbus_poll.py --selftest
    python modbus_poll.py --port COM5
    python modbus_poll.py --port COM5 --interval 1 -n 10
    python modbus_poll.py --port /dev/ttyUSB0 --raw

Register map (FC03, slave 0x01, 115200 8N1):
    0x0000  PM2.5  [ug/m3]
    0x0001  PM10   [ug/m3]
    0x0002  Lux    [lx]
    0x0003  status bitfield
"""

from __future__ import annotations

import argparse
import sys
import time

SLAVE_DEFAULT = 0x01
BAUD_DEFAULT = 115200
REG_COUNT = 4

STATUS_BITS = [
    (0x0001, "PM2.5 error"),
    (0x0002, "PM10 error"),
    (0x0004, "BH1750 error"),
    (0x0008, "data not ready"),
    (0x0010, "sensor timeout"),
]

# Frames generated and verified against the sample vectors in docs/modbus-protocol.md.
SELFTEST_VECTORS = [
    # (payload without CRC, expected full frame)
    ("01 03 00 00 00 04", "01 03 00 00 00 04 44 09"),
    ("01 03 08 00 1A 00 2B 01 2C 00 00", "01 03 08 00 1A 00 2B 01 2C 00 00 CA 25"),
    ("01 83 02", "01 83 02 C0 F1"),
    ("01 83 03", "01 83 03 01 31"),
    ("01 84 01", "01 84 01 82 C0"),
]


def crc16(data: bytes) -> int:
    """Modbus CRC16 (poly 0xA001, init 0xFFFF). Sent low byte first."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 0x0001 else crc >> 1
    return crc


def with_crc(payload: bytes) -> bytes:
    c = crc16(payload)
    return payload + bytes((c & 0xFF, c >> 8))


def hexs(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def build_request(slave: int, start: int, count: int) -> bytes:
    return with_crc(
        bytes((slave, 0x03, start >> 8, start & 0xFF, count >> 8, count & 0xFF))
    )


def decode_response(frame: bytes, count: int,
                    slave: int = SLAVE_DEFAULT) -> tuple[list[int] | None, str | None]:
    """Return (register values, error message)."""
    if len(frame) < 5:
        return None, f"frame too short ({len(frame)} bytes): {hexs(frame)}"
    if frame[0] != slave:
        return None, f"unexpected slave address 0x{frame[0]:02X}"
    if frame[1] & 0x80:
        codes = {
            0x01: "illegal function",
            0x02: "illegal address",
            0x03: "illegal data value",
            0x04: "slave device failure",
        }
        return None, f"exception 0x{frame[2]:02X} ({codes.get(frame[2], 'unknown')})"
    if frame[1] != 0x03:
        return None, f"unexpected function code 0x{frame[1]:02X}"
    if crc16(frame[:-2]) != (frame[-2] | (frame[-1] << 8)):
        return None, f"CRC mismatch: {hexs(frame)}"

    byte_count = frame[2]
    if byte_count != count * 2 or len(frame) != 5 + byte_count:
        return None, f"bad byte count {byte_count} for {count} registers"

    values = [(frame[3 + i * 2] << 8) | frame[4 + i * 2] for i in range(count)]
    return values, None


def describe_status(status: int) -> str:
    if status == 0:
        return "0x0000 OK"
    active = [name for bit, name in STATUS_BITS if status & bit]
    unknown = status & ~sum(bit for bit, _ in STATUS_BITS)
    if unknown:
        active.append(f"unknown bits 0x{unknown:04X}")
    return f"0x{status:04X} " + " | ".join(active)


def print_reading(index: int, values: list[int]) -> None:
    pm25, pm10, lux, status = values
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    print(
        f"#{index:<3} {stamp}  PM2.5={pm25:>3} ug/m3  PM10={pm10:>3} ug/m3  "
        f"Lux={lux:>5}  Status={describe_status(status)}",
        flush=True,
    )


def selftest() -> int:
    print("Modbus CRC / frame self-test")
    failures = 0
    for payload_hex, expected_hex in SELFTEST_VECTORS:
        got = hexs(with_crc(bytes.fromhex(payload_hex)))
        ok = got == expected_hex
        print(f"  [{'PASS' if ok else 'FAIL'}] {payload_hex:<36} -> {got}")
        failures += 0 if ok else 1

    values, err = decode_response(bytes.fromhex(SELFTEST_VECTORS[1][1]), REG_COUNT)
    ok = err is None and values == [26, 43, 300, 0]
    print(f"  [{'PASS' if ok else 'FAIL'}] decode response -> {values} {err or ''}")
    failures += 0 if ok else 1

    values, err = decode_response(bytes.fromhex(SELFTEST_VECTORS[2][1]), REG_COUNT)
    ok = err is not None and "exception" in err
    print(f"  [{'PASS' if ok else 'FAIL'}] detect exception -> {err}")
    failures += 0 if ok else 1

    print("ALL PASS" if failures == 0 else f"{failures} check(s) FAILED")
    return 0 if failures == 0 else 1


def poll(port: str, slave: int, baud: int, count: int, interval: float,
         limit: int, raw: bool) -> int:
    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial is required: pip install pyserial", file=sys.stderr)
        return 2

    try:
        link = serial.Serial(port, baud, bytesize=8, parity="N", stopbits=1,
                             timeout=0.3)
    except serial.SerialException as exc:
        print(f"cannot open {port}: {exc}", file=sys.stderr)
        return 2

    request = build_request(slave, 0x0000, count)
    expected = 5 + count * 2
    print(f"{port} @ {baud} 8N1, slave 0x{slave:02X}, polling {count} register(s)")

    index = 0
    try:
        while limit <= 0 or index < limit:
            index += 1
            link.reset_input_buffer()
            link.write(request)
            frame = link.read(expected)
            if raw:
                print(f"  TX {hexs(request)}\n  RX {hexs(frame) if frame else '<none>'}")
            if len(frame) < expected:
                print(f"#{index:<3} timeout, no (complete) response", flush=True)
            else:
                values, err = decode_response(frame, count)
                if err:
                    print(f"#{index:<3} {err}", flush=True)
                else:
                    print_reading(index, values)
            if limit <= 0 or index < limit:
                time.sleep(interval)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        link.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Poll the STM32F407 Modbus RTU environment node.")
    parser.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=BAUD_DEFAULT,
                        help=f"baud rate (default {BAUD_DEFAULT})")
    parser.add_argument("--slave", type=lambda s: int(s, 0), default=SLAVE_DEFAULT,
                        help="slave address (default 1)")
    parser.add_argument("--count", type=int, default=REG_COUNT,
                        help="registers to read, 1-4 (default 4)")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="seconds between polls (default 1.0)")
    parser.add_argument("-n", "--number", type=int, default=0,
                        help="stop after N polls (0 = forever)")
    parser.add_argument("--raw", action="store_true", help="dump raw TX/RX bytes")
    parser.add_argument("--selftest", action="store_true",
                        help="run CRC/frame self-test without hardware")
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()
    if not args.port:
        parser.error("--port is required (or use --selftest)")
    if not 1 <= args.count <= REG_COUNT:
        parser.error(f"--count must be 1..{REG_COUNT}")
    return poll(args.port, args.slave, args.baud, args.count,
                args.interval, args.number, args.raw)


if __name__ == "__main__":
    raise SystemExit(main())
