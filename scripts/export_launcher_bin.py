#!/usr/bin/env python3
"""
Copy the PlatformIO app image for M5-Cardputer-Adv into a stable path
suitable for bmorcelli Launcher SD / OTA install.

This is the ESP application image (first byte 0xE9). It is NOT a merged
factory flash (bootloader + partitions + app) and will not wipe Launcher.

Usage (after a successful build):
    python3 scripts/export_launcher_bin.py
    python3 scripts/export_launcher_bin.py --env M5-Cardputer-Adv-dual

Optional:
    python3 scripts/export_launcher_bin.py --src .pio/build/M5-Cardputer-Adv/firmware.bin
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LAUNCHER_ENVS = {
    "M5-Cardputer-Adv": "NerdMiner_v2_M5-Cardputer-Adv.bin",
    "M5-Cardputer-Adv-dual": "NerdMiner_v2_M5-Cardputer-Adv-dual.bin",
}
DEFAULT_ENV = "M5-Cardputer-Adv"
DEFAULT_SRC = REPO_ROOT / ".pio" / "build" / DEFAULT_ENV / "firmware.bin"
DEFAULT_DST = REPO_ROOT / "firmware" / "launcher" / LAUNCHER_ENVS[DEFAULT_ENV]
ESP_IMAGE_MAGIC = 0xE9
# ESP32-S3 factory merges place the bootloader at 0x0 (also 0xE9) and the
# app at 0x10000. Reject those so Launcher never gets a full-flash image.
MERGED_APP_OFFSET = 0x10000


def looks_like_merged_factory(data: bytes) -> bool:
    if len(data) <= MERGED_APP_OFFSET:
        return False
    return data[MERGED_APP_OFFSET] == ESP_IMAGE_MAGIC


def export_app_bin(src: Path, dst: Path) -> int:
    if not src.is_file():
        print(f"error: app image not found: {src}", file=sys.stderr)
        print("Build first:  pio run -e M5-Cardputer-Adv  (or -e M5-Cardputer-Adv-dual)", file=sys.stderr)
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

    if looks_like_merged_factory(data):
        print(
            f"error: {src} looks like a merged factory image "
            f"(0xE9 at offset 0x{MERGED_APP_OFFSET:X}). "
            "On ESP32-S3 the bootloader itself starts with 0xE9; flashing that "
            "via Launcher can overwrite Launcher. Use firmware.bin instead.",
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
    parser.add_argument(
        "--env",
        choices=sorted(LAUNCHER_ENVS),
        default=DEFAULT_ENV,
        help="PlatformIO env whose app image should be exported (stock or dual).",
    )
    parser.add_argument("--src", type=Path, default=None)
    parser.add_argument("--dst", type=Path, default=None)
    args = parser.parse_args()
    src = args.src or (REPO_ROOT / ".pio" / "build" / args.env / "firmware.bin")
    dst = args.dst or (REPO_ROOT / "firmware" / "launcher" / LAUNCHER_ENVS[args.env])
    return export_app_bin(src, dst)


if __name__ == "__main__":
    sys.exit(main())
