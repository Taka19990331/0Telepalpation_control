import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import convert_friction as c


def row(i, state, event=0, reason=0, seq=None):
    return c.ROW.pack(i, i*500, 1, 0, state, reason, event,
                      .2, 0, 1, .19, 0, 0, i*500, 0, seq or i+1, 0, 0, 1, 500)


class ConversionTests(unittest.TestCase):
    def write(self, path, rows, reason=0):
        config = json.dumps({'NO_MOTOR_TEST': 1}).encode()
        path.write_bytes(c.HEADER.pack(b'FRIC001\0', 80, 500, len(config))+config+b''.join(rows)+
                         c.FOOTER.pack(b'ENDFR001', len(rows), reason, reason == 0, 0))

    def test_valid_and_duplicate_feedback(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'test.BIN'
            self.write(p, [row(0, 1, 1), row(1, 2, seq=2), row(2, 2, 2, seq=2)])
            self.assertTrue(c.convert(p)['complete'])
            with p.with_suffix('.steps.csv').open() as f:
                s = next(csv.DictReader(f))
            self.assertEqual(s['valid_samples'], '1')
            self.assertEqual(s['classification'], 'STEADY')

    def test_reacceleration_invalidates_all_acquisition(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'test.BIN'
            self.write(p, [row(0, 1, 1), row(1, 2), row(2, 2, 2, 2)])
            c.convert(p)
            with p.with_suffix('.steps.csv').open() as f:
                s = next(csv.DictReader(f))
            self.assertEqual(s['valid_samples'], '0')
            self.assertEqual(s['end_reason'], 'UNSTABLE')
            self.assertEqual(s['mean_speed_rad_s'], '')

    def test_stationary_and_abort_prefix(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'test.BIN'
            self.write(p, [row(0, 1, 1), row(1, 3), row(2, 3, 2)])
            c.convert(p)
            with p.with_suffix('.steps.csv').open() as f:
                self.assertEqual(next(csv.DictReader(f))['classification'], 'STATIONARY')
            self.write(p, [row(0, 1, 1), row(1, 2)], 8)
            self.assertFalse(c.convert(p)['complete'])
            with p.with_suffix('.steps.csv').open() as f:
                s = next(csv.DictReader(f))
            self.assertEqual(s['valid_samples'], '0')
            self.assertEqual(s['end_reason'], 'RING_FULL')

    def test_truncation_and_invalid_index_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'test.BIN'
            self.write(p, [row(0, 1, 1)])
            p.write_bytes(p.read_bytes()[:-1])
            with self.assertRaises(ValueError): c.convert(p)
            self.write(p, [row(3, 1, 1)])
            with self.assertRaises(ValueError): c.convert(p)
            self.assertFalse(p.with_suffix('.csv.tmp').exists())


if __name__ == '__main__':
    unittest.main()
