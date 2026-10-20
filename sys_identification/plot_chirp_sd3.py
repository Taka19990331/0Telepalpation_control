"""Plot SD3/4/5 motor/PD logs; invoked by plot_chirp.py."""
import json
import numpy as np
import matplotlib.pyplot as plt


def plot(path, data):
    data = np.atleast_1d(data)
    c = json.loads(path.with_suffix('.config.json').read_text(encoding='utf-8'))
    t = data['nominal_us']/1e6
    plt.rcParams.update({'font.family': 'Meiryo', 'font.size': 10})
    fig, axes = plt.subplots((5 if 'pi_integral_Nm' in data.dtype.names else 4), 2, figsize=(14, 17), constrained_layout=True)
    motor_label = 'AK60' if c.get('MotorProtocol',0) == 1 else 'AK80-8'
    fig.suptitle(f"{motor_label} 外部PD＋チャープ ｜ {c['F0_Hz']:g}–{c['F1_Hz']:g} Hz / {c['Duration_s']:g} s / {'非駆動テスト' if c['NoMotor'] else 'CAN駆動'}")
    ax = axes[0, 0]
    for key, label in [('command_Nm', '最終指令'), ('chirp_Nm', 'チャープ'), ('pd_Nm', 'PD保持')]:
        ax.plot(t, data[key], lw=.5, label=label)
    ax.legend(); ax.set(ylabel='トルク [N·m]', title='トルク指令成分')
    axes[0, 1].plot(t, data['position_deg'])
    axes[0, 1].axhline(c['Hold_deg'], color='red', ls='--')
    axes[0, 1].set(title='連続範囲内の位置と固定保持目標', ylabel='位置 [deg]')
    axes[1, 0].plot(t, data['speed_rad_s'],label='measured'); axes[1, 0].set(title='実測速度（テスト時は模擬）', ylabel='速度 [rad/s]')
    if 'target_velocity_rad_s' in data.dtype.names:
        axes[1,0].plot(t,data['target_velocity_rad_s'],label='target'); axes[1,0].legend()
        for key in ('pi_proportional_Nm','pi_integral_Nm','raw_torque_Nm','command_Nm'):
            axes[4,0].plot(t,data[key],lw=.5,label=key)
        axes[4,0].legend(); axes[4,0].set(ylabel='Nm',title='ZEROING PI / raw / command')
        axes[4,1].step(t,data['torque_limited'],label='torque saturation')
        axes[4,1].step(t,data['slew_limited'],label='slew limit'); axes[4,1].legend()
    axes[1, 1].plot(t, data['feedback_torque_estimate_Nm'])
    axes[1, 1].set(title='MIT換算トルク（外部トルクセンサではない）', ylabel='換算値 [N·m]')
    axes[2, 0].step(t, data['phase'], where='post')
    axes[2, 0].set(title='区間：0=前保持 / 1=加振 / 2=後保持 / 3=停止 / 4=原点整定 / 5=PI→PD移行', ylabel='区間')
    if c.get('HoldPolicy')==2:
        axes[2,0].set_title('0=zero torque / position average; 1=sweep; 2=PD hold; 3=stop')
    axes[2, 1].plot(t, data['feedback_age_us'])
    axes[2, 1].set(title='使用したフィードバックの経過時間', ylabel='時間 [µs]')
    axes[3, 0].plot(t[1:], data['period_us'][1:], lw=.5)
    axes[3, 0].set(title='ISR開始間隔', ylabel='周期 [µs]')
    axes[3, 1].plot(t, data['ISR_us'], lw=.5)
    axes[3, 1].set(title='ISR処理時間', ylabel='時間 [µs]')
    home_s = c.get('HomingRows',0)*c['Period_us']/1e6
    sweep_s = (c['HomingRows']+c['TransitionRows']+c['PreHoldRows'])*c['Period_us']/1e6 if 'TransitionRows' in c else (c['AverageRows']*c['Period_us']/1e6 if c.get('HoldPolicy')==2 else home_s+c['PreHold_s'])
    for ax in axes.flat:
        ax.set_xlabel('記録開始からの時間 [s]'); ax.grid(alpha=.2)
        ax.axvline(sweep_s, color='grey', ls=':', lw=.7)
        ax.axvline(sweep_s+c['Duration_s'], color='grey', ls=':', lw=.7)
    output = path.with_name(path.stem + '_plots.png')
    fig.savefig(output, dpi=140); fig.savefig(output.with_suffix('.pdf')); plt.close(fig)
    print(output)
