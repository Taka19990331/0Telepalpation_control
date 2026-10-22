"""Plot measured motor speed against the driver's torque estimate from a VelRamp CSV.

Requires numpy and Pillow. With no CSV path, the newest data/VelRamp_*.csv is used.
"""
import argparse
import csv
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FIELDS = [
    "timestamp_us", "command_rad_s", "position_rad", "velocity_rad_s",
    "torque_estimate_nm", "i_torque_command_nm", "stage", "error_code",
    "feedback_valid",
]
GROUPS = [
    ("Positive acceleration", (0, 1), "#0072B2"),
    ("Positive return to zero", (2, 3), "#D55E00"),
    ("Negative acceleration", (4, 5), "#009E73"),
    ("Negative return to zero", (6, 7), "#CC79A7"),
]


def load_csv(path):
    metadata = {}
    rows = []
    with path.open(encoding="utf-8", newline="") as source:
        for line in source:
            if line.startswith("#"):
                key, _, value = line[1:].rstrip().partition(",")
                metadata[key] = value
            elif line.startswith("timestamp_us,"):
                break
        for row in csv.DictReader(source, fieldnames=FIELDS):
            if row["timestamp_us"]:
                rows.append((
                    float(row["velocity_rad_s"]),
                    float(row["torque_estimate_nm"]),
                    int(row["stage"]),
                ))
    if not rows:
        raise ValueError(f"No sample rows found in {path}")
    return metadata, np.asarray(rows, dtype=np.float64)


def load_font(size, bold=False):
    names = ("arialbd.ttf", "Arial Bold.ttf") if bold else ("arial.ttf", "Arial.ttf")
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def make_plot(csv_path, output_path):
    metadata, data = load_csv(csv_path)
    speed = data[:, 0]
    torque = data[:, 1]
    stage = data[:, 2].astype(int)

    width, height = 1600, 1120
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    title_font = load_font(32, bold=True)
    subtitle_font = load_font(18)
    axis_font = load_font(21, bold=True)
    tick_font = load_font(17)
    legend_font = load_font(16)

    draw.text((145, 28), "VelRamp: measured speed vs motor torque estimate",
              fill="#182230", font=title_font)
    subtitle = (
        f"AK80 | ID {metadata.get('motor_id', '?')} | "
        f"{metadata.get('sampling_hz', '?')} Hz | {len(data):,} samples | "
        f"result code {metadata.get('result_code', '?')}"
    )
    draw.text((148, 76), subtitle, fill="#506070", font=subtitle_font)

    legend_x, legend_y = 155, 125
    masks = []
    for label, stages, color in GROUPS:
        mask = np.isin(stage, stages)
        masks.append(mask)
        draw.ellipse((legend_x, legend_y + 5, legend_x + 13, legend_y + 18), fill=color)
        draw.text((legend_x + 21, legend_y), label, fill="#263442", font=legend_font)
        legend_x += 330

    left, right, top, bottom = 145, 1530, 180, 930
    x_min, x_max = -11.5, 11.5
    y_min, y_max = -0.9, 0.9

    def to_pixel(x, y):
        px = left + (x - x_min) / (x_max - x_min) * (right - left)
        py = bottom - (y - y_min) / (y_max - y_min) * (bottom - top)
        return px, py

    for x in np.arange(-10, 10.01, 2.5):
        px, _ = to_pixel(x, 0)
        color = "#94A0AC" if abs(x) < 1e-9 else "#E2E7EC"
        draw.line((px, top, px, bottom), fill=color, width=2 if abs(x) < 1e-9 else 1)
        label = f"{x:g}"
        box = draw.textbbox((0, 0), label, font=tick_font)
        draw.text((px - (box[2] - box[0]) / 2, bottom + 14), label,
                  fill="#354454", font=tick_font)

    for y in np.arange(-0.8, 0.801, 0.2):
        _, py = to_pixel(0, y)
        color = "#94A0AC" if abs(y) < 1e-9 else "#E2E7EC"
        draw.line((left, py, right, py), fill=color, width=2 if abs(y) < 1e-9 else 1)
        label = f"{y:.1f}"
        box = draw.textbbox((0, 0), label, font=tick_font)
        draw.text((left - 16 - (box[2] - box[0]), py - (box[3] - box[1]) / 2),
                  label, fill="#354454", font=tick_font)

    # Translucent observations sampled every eighth row, with 0.25 rad/s bin means.
    for (_, _, color), mask in zip(GROUPS, masks):
        indices = np.flatnonzero(mask & np.isfinite(speed) & np.isfinite(torque))
        layer = Image.new("RGBA", (width, height), (255, 255, 255, 0))
        layer_draw = ImageDraw.Draw(layer)
        rgb = tuple(int(color[index:index + 2], 16) for index in (1, 3, 5))
        for index in indices[::8]:
            px, py = to_pixel(speed[index], torque[index])
            px, py = int(round(px)), int(round(py))
            if left <= px <= right and top <= py <= bottom:
                layer_draw.ellipse((px - 1, py - 1, px + 1, py + 1),
                                   fill=(*rgb, 48))
        image = Image.alpha_composite(image.convert("RGBA"), layer).convert("RGB")
        draw = ImageDraw.Draw(image)

        edges = np.arange(x_min, x_max + 0.25001, 0.25)
        bins = np.digitize(speed[indices], edges) - 1
        means = []
        for bin_index in range(len(edges) - 1):
            selected = bins == bin_index
            if selected.sum() >= 20:
                means.append((float(np.mean(speed[indices][selected])),
                              float(np.mean(torque[indices][selected]))))
        if len(means) > 1:
            draw.line([to_pixel(x, y) for x, y in means], fill=color, width=3)

    draw.rectangle((left, top, right, bottom), outline="#647280", width=2)
    x_label = "Measured motor speed (rad/s)"
    box = draw.textbbox((0, 0), x_label, font=axis_font)
    draw.text(((left + right - (box[2] - box[0])) / 2, bottom + 56), x_label,
              fill="#182230", font=axis_font)

    y_label = Image.new("RGBA", (500, 48), (255, 255, 255, 0))
    ImageDraw.Draw(y_label).text((0, 0), "Motor torque estimate (Nm)",
                                 fill="#182230", font=axis_font)
    y_label = y_label.rotate(90, expand=True)
    image.paste(y_label, (28, int((top + bottom - y_label.height) / 2)), y_label)
    draw = ImageDraw.Draw(image)
    draw.text((left, 1018),
              "Dots: sampled observations (every 8th row) | Lines: mean torque in 0.25 rad/s speed bins",
              fill="#506070", font=subtitle_font)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path, dpi=(180, 180), optimize=True)
    return len(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", type=Path,
                        help="VelRamp CSV; defaults to the newest CSV in data/")
    parser.add_argument("-o", "--output", type=Path,
                        help="PNG output path; defaults beside the CSV")
    args = parser.parse_args()

    data_dir = Path(__file__).resolve().parent / "data"
    csv_path = args.csv
    if csv_path is None:
        candidates = list(data_dir.glob("VelRamp_*.csv"))
        if not candidates:
            parser.error(f"No VelRamp CSV files found in {data_dir}")
        csv_path = max(candidates, key=lambda path: path.stat().st_mtime)
    output_path = args.output or csv_path.with_name(csv_path.stem + "_torque_vs_speed.png")
    count = make_plot(csv_path, output_path)
    print(f"Plotted {count:,} samples: {output_path}")


if __name__ == "__main__":
    main()
