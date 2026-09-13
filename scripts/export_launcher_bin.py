#!/usr/bin/env python3
"""
Copy the PlatformIO app image for M5-Cardputer-Adv into a stable path
suitable for bmorcelli Launcher SD / OTA install.

This is the ESP application image (first byte 0xE9). It is NOT a merged
factory flash (bootloader + partitions + app) and will not wipe Launcher.

Usage (after a successful build):
    python3 scripts/export_launcher_bin.py

Optional:
    python3 scripts/export_launcher_bin.py --src .pio/build/M5-Cardputer-Adv/firmware.bin
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SRC = REPO_ROOT / ".pio" / "build" / "M5-Cardputer-Adv" / "firmware.bin"
DEFAULT_DST = REPO_ROOT / "firmware" / "launcher" / "NerdMiner_v2_M5-Cardputer-Adv.bin"
ESP_IMAGE_MAGIC = 0xE9


def export_app_bin(src: Path, dst: Path) -> int:
    if not src.is_file():
        print(f"error: app image not found: {src}", file=sys.stderr)
        print("Build first:  pio run -e M5-Cardputer-Adv", file=sys.stderr)
        return 1

    data = src.read_bytes()
    if not data:
        print(f"error: empty file: {src}", file=sys.stderr)
        return 1

    magic = data[0]
    if magic != ESP_IMAGE_MAGIC:
        print(
            f"error: {src} starts with 0x{magic:02X}, not ESP app magic 0xE9. "
            "Refusing to export a merged factory / bootloader image.",
            file=sys.stderr,
        )
        return 1

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(data)
    digest = hashlib.sha256(data).hexdigest()
    print(f"Launcher app-only bin: {dst}")
    print(f"  size:   {len(data)} bytes")
    print(f"  magic:  0x{magic:02X}")
    print(f"  sha256: {digest}")
    print()
    print("Install: copy this file to a FAT32 SD card, boot Cardputer Adv into")
    print("bmorcelli Launcher (m5stack-cardputer-adv), open SD, select the bin, Install.")
    print("Do not flash firmware/*_factory.bin through Launcher if you want to keep it.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC)
    parser.add_argument("--dst", type=Path, default=DEFAULT_DST)
    args = parser.parse_args()
    return export_app_bin(args.src, args.dst)


if __name__ == "__main__":
    sys.exit(main())
