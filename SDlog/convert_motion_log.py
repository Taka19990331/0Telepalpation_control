"""Convert ConstVel1, StepTor1, and VelRamp1 raw SD logs to CSV (stdlib only)."""
import argparse
import csv
from pathlib import Path
import struct

HEADERS = {
    b'CONSTVL1': (60, b'ENDCV001', 'constvel',
        'timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,i_torque_command_nm,stage,error_code,feedback_valid'),
    b'STEPTOR1': (60, b'ENDST001', 'steptor',
        'timestamp_us,command_torque_nm,position_rad,velocity_rad_s,torque_estimate_nm,feedforward_torque_command_nm,stage,error_code,feedback_valid'),
    b'VELRAMP1': (68, b'ENDVR001', 'velramp',
        'timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,i_torque_command_nm,stage,error_code,feedback_valid'),
}
ROW = struct.Struct('<I5f4B')
FOOTER = struct.Struct('<8sII')


def convert(source, destination):
    source = Path(source)
    with source.open('rb') as f:
        magic = f.read(8)
        if magic not in HEADERS:
            raise ValueError(f'unsupported motion log magic: {magic!r}')
        header_size, footer_magic, name, columns = HEADERS[magic]
        f.seek(0)
        header = f.read(header_size)
        if len(header) != header_size or struct.unpack_from('<I', header, 24)[0] != ROW.size:
            raise ValueError('invalid or truncated header')
        if source.stat().st_size < header_size + FOOTER.size:
            raise ValueError('truncated log')
        f.seek(-FOOTER.size, 2)
        end_magic, rows, status = FOOTER.unpack(f.read(FOOTER.size))
        header_rows = struct.unpack_from('<I', header, 28)[0]
        if (end_magic != footer_magic or header_rows != rows or
                source.stat().st_size != header_size + rows * ROW.size + FOOTER.size):
            raise ValueError('footer or row count mismatch')
        f.seek(header_size)
        output = Path(destination)
        temp = output.with_suffix(output.suffix + '.partial')
        try:
            with temp.open('x', newline='', encoding='utf-8') as out:
                out.write(f'# source_format={name},status={status},rows={rows}\n')
                out.write('# source_header_hex=' + header.hex() + '\n')
                out.write(columns + '\n')
                writer = csv.writer(out, lineterminator='\n')
                for index in range(rows):
                    raw = f.read(ROW.size)
                    if len(raw) != ROW.size:
                        raise ValueError(f'truncated row {index}')
                    timestamp, *values = ROW.unpack(raw)
                    writer.writerow([timestamp, *values[:5], *values[5:8]])
            temp.replace(output)
        except Exception:
            temp.unlink(missing_ok=True)
            raise
    return rows, status


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('binary', type=Path)
    p.add_argument('-o', '--output', type=Path, required=True)
    args = p.parse_args()
    try:
        rows, status = convert(args.binary, args.output)
    except (OSError, ValueError, struct.error) as exc:
        p.exit(1, f'ERROR: {exc}\n')
    print(f'{args.output}: {rows} rows, status={status}')
