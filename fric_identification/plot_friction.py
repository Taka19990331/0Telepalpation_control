"""Plot corresponding samples, keeping stationary points out of dynamic friction curves."""
import argparse
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd


def plot(path):
    path = Path(path)
    data = pd.read_csv(path, comment='#')
    steps = pd.read_csv(path.with_suffix('.steps.csv'))
    fig, axes = plt.subplots(5, 1, figsize=(12, 19), constrained_layout=True)
    t = data.actual_us/1e6
    axes[0].plot(t, data.target_torque_Nm, label='Requested output torque')
    axes[0].plot(t, data.feedback_torque_estimate_Nm, alpha=.65, label='Internal torque estimate')
    axes[0].set(xlabel='Time [s]', ylabel='Torque [Nm]')
    axes[1].plot(t, data.speed_rad_s)
    axes[1].set(xlabel='Time [s]', ylabel='Speed [rad/s]')
    for phase, label, color in [(1,'Positive increasing','C0'), (2,'Positive decreasing','C1'),
                                 (3,'Negative increasing','C2'), (4,'Negative decreasing','C3')]:
        s = steps[steps.phase == phase]
        valid = s.loc[s.classification == 'STEADY', 'step_index']
        # Same-row pairs only; never combine independent time averages.
        d = data[(data.phase == phase) & data.step_index.isin(valid) & (data.settling_status == 2)]
        d = d.drop_duplicates('feedback_sequence')
        axes[2].scatter(d.speed_rad_s, d.target_torque_Nm, s=2, color=color, label=label)
        axes[3].scatter(d.speed_rad_s, d.feedback_torque_estimate_Nm, s=2, color=color, label=label)
        axes[4].scatter(s.step_index, s.settling_time_s, s=12, color=color, label=label)
        stationary = s[s.classification == 'STATIONARY']
        axes[4].scatter(stationary.step_index, stationary.settling_time_s, marker='x', color=color)
    axes[2].set(xlabel='Speed [rad/s]', ylabel='Requested torque [Nm]')
    axes[3].set(xlabel='Speed [rad/s]', ylabel='Torque estimate [Nm]')
    axes[4].set(xlabel='Step index (x = stationary)', ylabel='Settling time [s]')
    for ax in axes:
        ax.grid(alpha=.3)
        if ax.get_legend_handles_labels()[0]:
            ax.legend(loc='best')
    fig.suptitle('Friction identification — latest feedback snapshots; braking excluded from curves')
    out = path.with_suffix('.plots.png')
    fig.savefig(out, dpi=150)
    plt.close(fig)
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    print(plot(parser.parse_args().csv))
