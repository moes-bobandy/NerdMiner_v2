#!/usr/bin/env python3
"""Sanity checks for scripts/export_launcher_bin.py (no hardware required)."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from export_launcher_bin import ESP_IMAGE_MAGIC, export_app_bin


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
            # Merged factory images start with bootloader bytes, not 0xE9.
            src.write_bytes(b"\x00\x00\x00\x00" + b"\xff" * 64)
            self.assertEqual(export_app_bin(src, dst), 1)
            self.assertFalse(dst.exists())


if __name__ == "__main__":
    unittest.main()
