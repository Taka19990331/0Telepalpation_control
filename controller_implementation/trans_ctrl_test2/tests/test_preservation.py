"""Verify that transport, logging, safety handling and preexisting tests are untouched."""
from pathlib import Path
import unittest


class PreservationTest(unittest.TestCase):
    def test_unchanged_files_and_safety(self):
        root = Path(__file__).resolve().parents[1]
        source = root.with_name('trans_ctrl_test1')
        changed = {'control_algorithm.h', 'control_config.h', 'build_verify.ps1',
                   'trans_ctrl_test1.ino'}
        for path in source.rglob('*'):
            relative = path.relative_to(source)
            if not path.is_file() or '__pycache__' in relative.parts or str(relative) in changed:
                continue
            with self.subTest(file=str(relative)):
                self.assertEqual(path.read_bytes(), (root/relative).read_bytes())
        original = (source/'trans_ctrl_test1.ino').read_bytes()
        revised = (root/'trans_ctrl_test2.ino').read_bytes()
        self.assertEqual(original.replace(b'controller.feedback(f.velocity,f.torque,f.rxUs)',
                                         b'controller.feedback(f.velocity,f.torque,f.rxUs,f.position)'), revised)
        added_lines = (
            b'// Position is used only for low-speed friction feedforward direction.\n'
            b'constexpr uint32_t POSITION_DIRECTION_WINDOW_US=50000;\n'
            b'constexpr float POSITION_DIRECTION_THRESHOLD_RAD=3.0f*(25.0f/65535.0f);\n'
            b'constexpr float FRICTION_BLEND_LOW=0.8f, FRICTION_BLEND_HIGH=1.2f;\n')
        self.assertEqual((source/'control_config.h').read_bytes(),
                         (root/'control_config.h').read_bytes().replace(added_lines, b''))
