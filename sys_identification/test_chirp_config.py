"""Offline format/config regression checks; no board or serial port required."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from convert_sd_chirp import convert, HEADER, HEADER_V2, ROW, FOOTER
from capture_chirp import extract


class ConfigTests(unittest.TestCase):
    def test_binary_versions_and_changed_conditions(self):
        for version, period, steps, f0, f1, exponent in [(1, 500, 40000, 1, 100, 1),
                                                       (2, 1000, 200, 2, 80, 2)]:
            with self.subTest(version=version), tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
                path = Path(folder) / 'run.BIN'
                header = HEADER if version == 1 else HEADER_V2
                values = [f'CHIRPSD{version}'.encode(), period, period * steps, steps + 1,
                          ROW.size, f0, f1, 1, 2.5]
                if version == 2:
                    values.append(exponent)
                rows = b''.join(ROW.pack(i, i * period, 0, 2) for i in range(steps + 1))
                path.write_bytes(header.pack(*values) + rows + FOOTER.pack(b'ENDSD001', steps + 1, 0))
                with contextlib.redirect_stdout(io.StringIO()):
                    convert(path)
                import json
                config = json.loads(path.with_suffix('.config.json').read_text())
                self.assertEqual(config['F0_Hz'], f0)
                self.assertEqual(config['AmplitudeExponent'], exponent)
                self.assertEqual(len(path.with_suffix('.csv').read_text().splitlines()), steps + 2)
                path.write_bytes(path.read_bytes()[:-1])
                with self.assertRaises(ValueError):
                    convert(path)

    def test_serial_config_controls_validation(self):
        for hz, duration, count, valid in [(1000, .002, 3, True), (2000, .002, 3, False),
                                           (3000, .001, 4, False)]:
            with self.subTest(hz=hz), tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
                path = Path(folder) / 'raw.txt'
                period = round(1e6 / hz)
                lines = ['CHIRP_BEGIN,VERSION,SD2,REASON,COMPLETE',
                         f'CONFIG,Hz,{hz},Duration_s,{duration},F0_Hz,2,F1_Hz,80,A0_Nm,1,A1_Nm,2.5,AmplitudeExponent,2',
                         'DATA,index,nominal_us,actual_us,period_us,torque_Nm,ISR_us']
                lines += [f'DATA,{i},{i*period},{i*period},{period if i else 0},0,2' for i in range(count)]
                lines += [f'SUMMARY,Count,{count}', 'CHIRP_END']
                path.write_text('\n'.join(lines))
                self.assertEqual(extract(path, path.with_suffix('.csv'))['valid'], valid)


if __name__ == '__main__':
    unittest.main()

