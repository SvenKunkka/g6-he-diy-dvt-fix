#!/usr/bin/env python3
"""Verify properties that must hold before the standalone image is handed off."""

from __future__ import annotations

import hashlib
import os
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("G6_BUILD_DIR", ROOT / "build/zephyr"))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"build contract verification: FAIL: {message}")


def parse_ihex(path: Path) -> tuple[int, int]:
    upper = 0
    addresses: list[int] = []
    for line in path.read_text(encoding="ascii").splitlines():
        require(line.startswith(":"), f"invalid Intel HEX line in {path}")
        record = bytes.fromhex(line[1:])
        length = record[0]
        offset = int.from_bytes(record[1:3], "big")
        record_type = record[3]
        data = record[4 : 4 + length]
        require(sum(record) & 0xFF == 0, f"bad Intel HEX checksum in {path}")
        if record_type == 0:
            addresses.extend(range(upper + offset, upper + offset + length))
        elif record_type == 2:
            upper = int.from_bytes(data, "big") << 4
        elif record_type == 4:
            upper = int.from_bytes(data, "big") << 16
    require(bool(addresses), "Intel HEX has no data records")
    return min(addresses), max(addresses)


def main() -> None:
    binary_path = BUILD / "zephyr.bin"
    hex_path = BUILD / "zephyr.hex"
    elf_path = BUILD / "zephyr.elf"
    dts_path = BUILD / "zephyr.dts"
    for path in (binary_path, hex_path, elf_path, dts_path):
        require(path.is_file(), f"missing build output {path}")

    binary = binary_path.read_bytes()
    require(len(binary) < 2_036 * 1024, "binary exceeds application RRAM")
    initial_sp, reset_vector = struct.unpack_from("<II", binary)
    require(0x2000_0000 <= initial_sp < 0x2010_0000, "invalid initial stack")
    require(reset_vector & 1 == 1, "reset vector is not Thumb code")
    require(reset_vector < len(binary), "reset vector lies outside image")
    require(b"G6 HE DIY DVT Fix v0.2" in binary,
            "missing development USB product")
    require(b"G6 DIY Lab" in binary, "missing development manufacturer")

    low, high = parse_ihex(hex_path)
    require(low == 0, f"standalone image starts at 0x{low:x}, expected 0")
    require(high < 0x20_0000, "image extends outside nRF54LM20A RRAM")

    overlay = (ROOT / "app.overlay").read_text(encoding="utf-8")
    for pin in (
        "NRF_PSEL(SPIM_SCK, 2, 1)",
        "NRF_PSEL(SPIM_MOSI, 2, 2)",
        "NRF_PSEL(SPIM_MISO, 2, 4)",
    ):
        require(pin in overlay, f"missing confirmed sensor pin {pin}")
    require("&spi00" in overlay, "confirmed SPI00 controller is not enabled")
    require("&wdt31" in overlay, "watchdog alias target is not enabled")

    main_source = (ROOT / "src/main.c").read_text(encoding="utf-8")
    initialize = main_source[main_source.index("static int initialize(void)") :]
    set_poll = initialize.index("hid_device_set_in_polling")
    usb_init = initialize.index("g6_usb_init")
    usb_enable = initialize.index("usbd_enable")
    require(set_poll < usb_init < usb_enable, "initial polling is set too late")

    restart = main_source[
        main_source.index("static void usb_restart_handler") :
        main_source.index("static void update_health_locked")
    ]
    calls = [
        restart.index("usbd_disable"),
        restart.index("usbd_shutdown"),
        restart.index("hid_device_set_in_polling"),
        restart.index("usbd_init"),
        restart.index("usbd_enable"),
    ]
    require(calls == sorted(calls), "USB polling restart sequence is out of order")
    require(
        re.search(r"k_work_reschedule\s*\(", main_source) is not None,
        "polling changes do not defer USB restart",
    )

    print("build contract verification: PASS")
    print(f"bin bytes: {len(binary)}")
    print(f"bin sha256: {hashlib.sha256(binary).hexdigest()}")
    print(f"hex address span: 0x{low:08x}-0x{high:08x}")
    print(f"initial SP/reset: 0x{initial_sp:08x}/0x{reset_vector:08x}")


if __name__ == "__main__":
    main()
