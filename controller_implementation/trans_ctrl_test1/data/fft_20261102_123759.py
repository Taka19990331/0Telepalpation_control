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
source = ROOT / 'trans_ctrl_test1_20261102_123759.csv'
with source.open(encoding='utf-8-sig') as stream:
    rows = csv.DictReader(line for line in stream if line.strip() and not line.startswith('#'))
    names = ['timestamp_us', 'position_rad', 'velocity_filtered_rad_s', 'external_torque_est_Nm']
    values = np.array([[float(row[k]) for k in names] for row in rows])
assert np.isfinite(values).all()
dt = np.median(np.diff(values[:, 0])) * 1e-6
fs = 1 / dt
signals = [
    (1, 'Position [rad]', '#2166ac'),
    (2, 'Velocity [rad/s]', '#18856b'),
    (3, 'RFOB estimated external torque [N m]', '#c35b24'),
]
fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)
summary = {'file': source.name, 'samples': len(values), 'sample_rate_hz': fs,
           'duration_s': float((len(values)-1)*dt), 'nyquist_hz': fs/2,
           'method': 'mean removed; Hann window; single-sided amplitude spectrum'}
for ax, (col, label, color) in zip(axes, signals):
    x = values[:, col] - np.mean(values[:, col])
    window = np.hanning(len(x))
    spectrum = np.fft.rfft(x * window)
    freq = np.fft.rfftfreq(len(x), d=dt)
    amp = np.abs(spectrum) * 2 / np.sum(window)
    amp[0] *= .5
    if len(x) % 2 == 0:
        amp[-1] *= .5
    ax.plot(freq, amp, color=color, lw=.8)
    ax.set_ylabel('Amplitude\n' + label)
    ax.set_xlim(0, fs/2)
    ax.grid(alpha=.25)
    valid = (freq >= 1) & (freq <= fs/2)
    peaks = np.where(valid)[0][np.argmax(amp[valid])]
    summary[label] = {'largest_peak_hz': float(freq[peaks]), 'peak_amplitude': float(amp[peaks])}
axes[-1].set_xlabel('Frequency [Hz]')
fig.suptitle('FFT | trans_ctrl_test1 2026-11-02 12:37:59', fontsize=16, fontweight='bold')
fig.text(.5, .015, f"{len(values):,} samples, fs={fs:.1f} Hz, duration={(len(values)-1)*dt:.3f} s. Mean removed and Hann window applied.", ha='center', color='#4c5862')
fig.tight_layout(rect=(0, .04, 1, .96))
fig.savefig(OUT / 'fft_20261102_123759.png', dpi=180)
fig.savefig(OUT / 'fft_20261102_123759.svg')
plt.close(fig)
(OUT / 'fft_20261102_123759_summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
