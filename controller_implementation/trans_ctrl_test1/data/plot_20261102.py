from pathlib import Path
import csv
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'plots_20261102'
OUT.mkdir(exist_ok=True)
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})
files = sorted(ROOT.glob('trans_ctrl_test1_20261102_*.csv'))
assert len(files) == 3, files
signals = [('position_rad', 'Position [rad]', '#2166ac'),
           ('velocity_filtered_rad_s', 'Velocity [rad/s]', '#18856b'),
           ('external_torque_est_Nm', 'RFOB external torque [N m]', '#c35b24')]
datasets, summary = [], []
for path in files:
    with path.open(encoding='utf-8-sig') as stream:
        rows = csv.DictReader(line for line in stream if line.strip() and not line.startswith('#'))
        cols = ['timestamp_us', 'velocity_raw_rad_s'] + [s[0] for s in signals]
        values = np.array([[float(row[c]) for c in cols] for row in rows])
    assert np.isfinite(values).all(), path
    assert (np.diff(values[:, 0]) > 0).all(), path
    t = (values[:, 0] - values[0, 0]) / 1e6
    data = dict(zip(cols, values.T))
    # The encoder position wraps at +/-12.5 rad (25 rad per revolution).
    data['position_rad'] = np.unwrap(data['position_rad'] * (2 * np.pi / 25.0)) * (25.0 / (2 * np.pi))
    datasets.append((path, t, data))
    summary.append({'file': path.name, 'samples': len(t), 'duration_s': float(t[-1]),
                    'median_dt_s': float(np.median(np.diff(t))),
                    'ranges': {key: [float(data[key].min()), float(data[key].max())] for key, _, _ in signals}})

fig, axes = plt.subplots(3, 3, figsize=(18, 10), sharex='col', sharey=False)
for j, (path, t, data) in enumerate(datasets):
    for i, (key, label, color) in enumerate(signals):
        ax = axes[i, j]
        if i == 1:
            ax.plot(t, data['velocity_raw_rad_s'], color='#b3bbc2', lw=.5, alpha=.65, label='Raw')
        ax.plot(t, data[key], color=color, lw=.7, label='Filtered' if i == 1 else None)
        ax.grid(alpha=.22)
        ax.axhline(0, color='#5d6870', lw=.5, alpha=.45)
        ax.set_xlim(0, t[-1])
        if i == 0:
            lo, hi = np.percentile(data[key], [0.1, 99.9])
            pad = max((hi - lo) * .12, .01)
            ax.set_ylim(lo - pad, hi + pad)
        if j == 0:
            ax.set_ylabel(label)
        if i == 0:
            stamp = path.stem.rsplit('_', 1)[-1]
            ax.set_title(f'{stamp[:2]}:{stamp[2:4]}:{stamp[4:]}  /  {t[-1]:.2f} s', fontweight='bold')
        if i == 1 and j == 0:
            ax.legend(loc='upper right', fontsize=9)
        if i == 2:
            ax.set_xlabel('Elapsed time from first sample [s]')
fig.suptitle('trans_ctrl_test1  |  2026-11-02', fontsize=18, fontweight='bold', y=.98)
fig.text(.5, .022, 'Position is unwrapped across the +/-12.5 rad boundary (25 rad period). Position axes are zoomed independently per recording; all samples are shown.', ha='center', fontsize=10, color='#4c5862')
fig.tight_layout(rect=(0, .045, 1, .95))
fig.savefig(OUT / 'trans_ctrl_test1_20261102_overview.png', dpi=180)
fig.savefig(OUT / 'trans_ctrl_test1_20261102_overview.svg')
plt.close(fig)
for path, t, data in datasets:
    fig, axes = plt.subplots(3, 1, figsize=(14, 9), sharex=True)
    for ax, (key, label, color) in zip(axes, signals):
        if key.startswith('velocity'):
            ax.plot(t, data['velocity_raw_rad_s'], color='#b3bbc2', lw=.5, alpha=.65, label='Raw')
        ax.plot(t, data[key], color=color, lw=.7, label='Filtered' if key.startswith('velocity') else None)
        ax.set_ylabel(label)
        ax.grid(alpha=.22)
        ax.set_xlim(0, t[-1])
        if key == 'position_rad':
            lo, hi = np.percentile(data[key], [0.1, 99.9])
            pad = max((hi - lo) * .12, .01)
            ax.set_ylim(lo - pad, hi + pad)
        if key.startswith('velocity'):
            ax.legend(loc='upper right')
    axes[-1].set_xlabel('Elapsed time from first sample [s]')
    fig.suptitle(path.stem + '  |  position unwrapped, zoomed scale', fontsize=15)
    fig.tight_layout()
    fig.savefig(OUT / f'{path.stem}.png', dpi=180)
    plt.close(fig)
(OUT / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
