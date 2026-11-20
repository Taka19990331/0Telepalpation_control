from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from convert_trans_log import RECORD, HEADER, decode, convert


def record(sequence=0, end=False):
    # Unique values for every field make column reordering errors visible.
    values = [sequence, *range(10, 20), *[float(x) for x in range(20, 33)],
              True, 33.0, 34.0, 1, 7, 6, 0]
    if end:
        values[0], values[1] = 0xffffffff, sequence
    raw = RECORD.pack(*values)
    return raw[:-4] + struct.pack('<I', zlib.crc32(raw[:-4]))


class BinaryLogTest(unittest.TestCase):
    def test_legacy_columns(self):
        row = decode(record(), 0)
        self.assertEqual(len(row), 29)
        self.assertEqual(dict(zip(HEADER.split(','), row)), dict(zip(HEADER.split(','), [
            10, 20, 21, 24, 22, 23, 11, 25, 26, 27, 28, 29, 30, 31, 32,
            33, 34, 1, 1, 7, 6, 12, 13, 14, 15, 16, 17, 18, 19])))

    def test_damage_and_order(self):
        raw = bytearray(record())
        raw[65] ^= 1
        for bad in [raw, record()[:-1], record(1)]:
            with self.assertRaises(ValueError):
                decode(bad, 0)

    def test_complete_and_recovery(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'TC0000.BIN'
            prefix = b'#design,test\r\n#binary,TCBIN2,120\r\n' + record()
            path.write_bytes(prefix + record(1, end=True) + b'#rows,1\r\n#status,6\r\n')
            csvpath, rows, complete = convert(path)
            self.assertEqual((rows, complete), (1, True))
            self.assertIn('#rows,1', csvpath.read_text())
            path.write_bytes(prefix + record(1)[:50])
            with self.assertRaises(ValueError):
                convert(path)
            recovered, rows, complete = convert(path, recover=True)
            self.assertEqual((rows, complete), (1, False))
            self.assertIn('.recovered.csv', recovered.name)
            self.assertIn('#recovery,incomplete', recovered.read_text())
            path.write_bytes(prefix + record(1, end=True))
            with self.assertRaises(ValueError):
                convert(path)


if __name__ == '__main__':
    unittest.main()
