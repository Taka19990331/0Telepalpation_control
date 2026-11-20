"""Verify that cloning test2 preserves the controller, transport, logging and data."""
from pathlib import Path
import unittest


class PreservationTest(unittest.TestCase):
    def test_unchanged_files_and_safety(self):
        root = Path(__file__).resolve().parents[1]
        source = root.with_name('trans_ctrl_test2')
        adjusted = {
            'build_verify.ps1', 'README_ja.md', 'CAN_DIAGNOSTICS_ja.md',
            'download_trans_ctrl_test1.ps1', 'trans_ctrl_test3.ino',
            'tests/run_verify.py', 'tests/test_direction.py',
            'tests/test_preservation.py', 'control_config.h',
            'control_algorithm.h', 'save_data.h',
            'tests/core_test.cpp', 'tests/direction_test.cpp',
        }
        for path in source.rglob('*'):
            relative = path.relative_to(source)
            if (not path.is_file() or '__pycache__' in relative.parts
                    or relative.as_posix() in adjusted):
                continue
            with self.subTest(file=str(relative)):
                self.assertEqual(path.read_bytes(), (root / relative).read_bytes())
        self.assertEqual((source / 'trans_ctrl_test2.ino').read_bytes(),
                         (root / 'trans_ctrl_test3.ino').read_bytes())
