#!/usr/bin/env python3
"""Sanity checks for scripts/export_launcher_bin.py (no hardware required)."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from export_launcher_bin import (
    ESP_IMAGE_MAGIC,
    LAUNCHER_ENVS,
    MERGED_APP_OFFSET,
    export_app_bin,
)


class ExportLauncherBinTests(unittest.TestCase):
    def test_exports_e9_app_image(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "firmware.bin"
            dst = Path(tmp) / "out" / "app.bin"
            src.write_bytes(bytes([ESP_IMAGE_MAGIC, 0x03, 0x02, 0x20]) + b"\x00" * 32)
            self.assertEqual(export_app_bin(src, dst), 0)
            self.assertTrue(dst.is_file())
            self.assertEqual(dst.read_bytes()[0], ESP_IMAGE_MAGIC)

    def test_rejects_missing_src(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "missing.bin"
            dst = Path(tmp) / "out.bin"
            self.assertEqual(export_app_bin(src, dst), 1)
            self.assertFalse(dst.exists())

    def test_rejects_factory_or_non_e9(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "factory.bin"
            dst = Path(tmp) / "out.bin"
            src.write_bytes(b"\x00\x00\x00\x00" + b"\xff" * 64)
            self.assertEqual(export_app_bin(src, dst), 1)
            self.assertFalse(dst.exists())

    def test_rejects_s3_merged_factory_that_also_starts_e9(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "factory.bin"
            dst = Path(tmp) / "out.bin"
            # ESP32-S3 bootloader @ 0x0 is 0xE9; app is repeated at 0x10000.
            blob = bytearray(MERGED_APP_OFFSET + 4)
            blob[0] = ESP_IMAGE_MAGIC
            blob[MERGED_APP_OFFSET] = ESP_IMAGE_MAGIC
            src.write_bytes(blob)
            self.assertEqual(export_app_bin(src, dst), 1)
            self.assertFalse(dst.exists())

    def test_launcher_envs_cover_stock_and_dual(self) -> None:
        self.assertIn("M5-Cardputer-Adv", LAUNCHER_ENVS)
        self.assertIn("M5-Cardputer-Adv-dual", LAUNCHER_ENVS)
        self.assertTrue(LAUNCHER_ENVS["M5-Cardputer-Adv-dual"].endswith("-dual.bin"))


if __name__ == "__main__":
    unittest.main()
