from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from convert_trans_log import RECORD_V2, RECORD_V3, HEADER, decode, convert


def make_record(version, sequence=0, end=False):
    if version == 2:
        values = [sequence, *range(10, 20), *[float(x) for x in range(20, 33)],
                  True, 33.0, 34.0, 1, 7, 6, 0]
        record = RECORD_V2
    else:
        values = [sequence, *range(10, 20), *[float(x) for x in range(20, 24)],
                  *[float(x) for x in range(24, 41)], 2, True, 41.0, 42.0, 1, 7, 6, 0]
        record = RECORD_V3
    if end:
        values[0], values[1] = 0xffffffff, sequence
    raw = record.pack(*values)
    return raw[:-4] + struct.pack('<I', zlib.crc32(raw[:-4]))


class BinaryLogTest(unittest.TestCase):
    def test_legacy_and_current_records(self):
        old = decode(make_record(2), 0, 2)
        self.assertEqual(len(old), 29)
        self.assertEqual(old[:7], [10, 20.0, 21.0, 24.0, 22.0, 23.0, 11])
        current = decode(make_record(3), 0, 3)
        self.assertEqual(len(current), 39)
        self.assertEqual(current[29:38], [33.0, 34.0, 35.0, 36.0, 37.0, 38.0, 39.0, 40.0, 2])

    def test_damage_sequence_and_conversion(self):
        raw = bytearray(make_record(3))
        raw[65] ^= 1
        with self.assertRaises(ValueError): decode(raw, 0, 3)
        scratch = Path(__file__).resolve().parent / 'tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            for version, record, label in [(2, RECORD_V2, 'TCBIN2,120'), (3, RECORD_V3, 'TCBIN3,152')]:
                path = Path(folder) / f'TC{version:04}.BIN'
                prefix = f'#design,test\r\n#binary,{label}\r\n'.encode() + make_record(version)
                path.write_bytes(prefix + make_record(version, 1, True) + b'#rows,1\r\n#status,6\r\n')
                csvpath, rows, complete = convert(path)
                self.assertEqual((rows, complete), (1, True))
                text = csvpath.read_text()
                self.assertIn('#rows,1', text)
                self.assertEqual(len(text.splitlines()[2].split(',')), 29 if version == 2 else 39)


if __name__ == '__main__':
    unittest.main()
