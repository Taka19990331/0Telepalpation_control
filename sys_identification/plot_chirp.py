"""Plot a CSV produced by capture_chirp.py (requires matplotlib and numpy)."""
import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('csv', type=Path)
args = parser.parse_args()
with args.csv.open(encoding='utf-8') as source:
    # names=True otherwise treats the first metadata comment as column names.
    data = np.genfromtxt((line for line in source if not line.startswith('#')),
                         delimiter=',', names=True)
if 'command_Nm' in data.dtype.names:
    from plot_chirp_sd3 import plot
    plot(args.csv, data)
    raise SystemExit(0)

t = data['nominal_us'] / 1e6
torque = data['torque_Nm']
config_path = args.csv.with_suffix('.config.json')
if not config_path.exists():
    raise ValueError('Missing .config.json: regenerate CSV with capture_chirp.py or convert_sd_chirp.py')
config = json.loads(config_path.read_text(encoding='utf-8'))
duration, fs = config['Duration_s'], config['Hz']
f0, f1 = config['F0_Hz'], config['F1_Hz']
k = np.log(f1 / f0) / duration
frequency = f0 * np.exp(k * t)
amplitude = config['A0_Nm'] + (config['A1_Nm'] - config['A0_Nm']) * np.clip(
    (frequency - f0) / (f1 - f0), 0, 1) ** config['AmplitudeExponent']
phase = 2 * np.pi * f0 * np.expm1(k * t) / k
reference = amplitude * np.sin(phase)
reference[t >= duration] = 0
plt.rcParams.update({'font.family': 'Meiryo', 'font.size': 10, 'axes.spines.top': False,
                     'axes.spines.right': False})
fig, axes = plt.subplots(3, 2, figsize=(14, 11), constrained_layout=True)
fig.suptitle(f'マイコン上のチャープ生成結果 ｜ {f0:g} → {f1:g} Hz・{duration:g}秒・{fs:g} Hz', fontsize=17)
ax = axes[0, 0]
ax.plot(t, torque, lw=.5, color='#2166ac', label='記録したトルク指令')
ax.plot(t, amplitude, '--', color='#d6604d', lw=1, label='設計上の振幅 ±A(f)')
ax.plot(t, -amplitude, '--', color='#d6604d', lw=1)
ax.set(title='全体波形', ylabel='トルク指令 [N·m]', xlim=(0, duration))
ax.legend(fontsize=9)
ax = axes[0, 1]
ax.plot(t, frequency, color='#238b45')
ax.set(title='設計上の瞬時周波数（計算式から算出）', ylabel='周波数 [Hz]', xlim=(0, duration))
for ax, lo, hi, title in [(axes[1, 0], 0, min(3, duration), '開始付近'),
                          (axes[1, 1], max(0, duration - .1), duration, '終了付近（終了時にゼロへ切替）')]:
    mask = (t >= lo) & (t <= hi)
    ax.plot(t[mask], torque[mask], '.-', lw=1, ms=2, color='#2166ac')
    ax.set(title=title, ylabel='トルク指令 [N·m]', xlim=(lo, hi))
axes[2, 0].plot(t[1:], data['period_us'][1:], color='#756bb1', lw=1)
axes[2, 0].set(title='実測ISR開始間隔（先頭行を除外）', ylabel='周期 [µs]')
axes[2, 1].plot(t, data['ISR_us'], color='#e08214', lw=.7)
axes[2, 1].set(title='ISR内の計算・記録時間（micros、分解能1 µs）', ylabel='処理時間 [µs]', ylim=(-.1, max(2, data['ISR_us'].max()+.5)))
for ax in axes.flat:
    ax.set_xlabel('経過時間 [s]')
    ax.grid(True, alpha=.2)
output = args.csv.with_name(args.csv.stem + '_plots.png')
fig.savefig(output, dpi=160)
fig.savefig(output.with_suffix('.pdf'))
print(output)
print(f'Rows: {len(t)}; period: {data["period_us"][1:].min():.0f}..{data["period_us"][1:].max():.0f} us')
print(f'ISR max: {data["ISR_us"].max():.0f} us; max |actual-nominal|: {np.max(np.abs(data["actual_us"]-data["nominal_us"])):.0f} us')
print(f'Max torque error against formula: {np.max(np.abs(torque-reference)):.3g} Nm')
