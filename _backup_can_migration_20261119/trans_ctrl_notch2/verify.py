"""Replay three angle logs through the production 2 kHz detector equations."""
import csv
import json
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'controller_implementation' / 'trans_ctrl_test1' / 'data'
OUT = Path(__file__).resolve().parent / 'verification'
OUT.mkdir(exist_ok=True)
FILES = {
    'free_rotation': 'trans_ctrl_test1_20261102_122211.csv',
    'no_operation': 'trans_ctrl_test1_20261102_122519.csv',
    'grip_vibration': 'trans_ctrl_test1_20261102_123759.csv',
}
FS = 2000.0
DT = 1.0 / FS
Q = (0.541196100146197, 1.306562964876377)


class Biquad:
    def __init__(self, cutoff, q, highpass):
        w = 2*np.pi*cutoff/FS
        c, s = np.cos(w), np.sin(w)
        alpha = s/(2*q); a0 = 1+alpha
        if highpass:
            b0, b1, b2 = (1+c)/(2*a0), -(1+c)/a0, (1+c)/(2*a0)
        else:
            b0, b1, b2 = (1-c)/(2*a0), (1-c)/a0, (1-c)/(2*a0)
        self.b0, self.b1, self.b2 = b0, b1, b2
        self.a1, self.a2 = -2*c/a0, (1-alpha)/a0
        self.z1 = self.z2 = 0.0; self.initialized = False

    def update(self, x):
        if not self.initialized:
            dc = (self.b0+self.b1+self.b2)/(1+self.a1+self.a2)
            y = dc*x
            self.z1, self.z2 = y-self.b0*x, self.b2*x-self.a2*y
            self.initialized = True
        y = self.b0*x+self.z1
        self.z1 = self.b1*x-self.a1*y+self.z2
        self.z2 = self.b2*x-self.a2*y
        return y


def detector(positions, velocities):
    hp = [Biquad(8, q, True) for q in Q]
    lp = [Biquad(20, q, False) for q in Q]
    fc = np.sqrt(8*20); w = 2*np.pi*fc/FS
    z = np.exp(-1j*w)
    mag = lambda b: abs((b.b0+b.b1*z+b.b2*z*z)/(1+b.a1*z+b.a2*z*z))
    normalization = 1/np.prod([mag(b) for b in hp+lp])
    bpf = np.zeros(len(positions)); rms = np.zeros(len(positions)); speed = np.zeros(len(positions))
    prev = None; unwrapped = 0.; rms2 = mean_v = 0.
    for i, (p, v) in enumerate(zip(positions, velocities)):
        if prev is None: unwrapped = p
        else:
            d = p-prev
            if d > 12.5: d -= 25.0
            elif d < -12.5: d += 25.0
            unwrapped += d
        prev = p
        y = unwrapped
        for section in hp+lp: y = section.update(y)
        y *= normalization
        ar = np.exp(-DT/.2); rms2 = ar*rms2+(1-ar)*y*y
        mean_v = np.exp(-2*np.pi*2*DT)*mean_v+(1-np.exp(-2*np.pi*2*DT))*v
        bpf[i], rms[i], speed[i] = y, np.sqrt(max(0,rms2)), abs(mean_v)
    return bpf, rms, speed


def adaptive(rms, speed, speed_gate=True, attack=.05, release=.5, hold=.25):
    start, full = np.deg2rad(.15), np.deg2rad(.25)
    gt = np.clip((rms-start)/(full-start), 0, 1)
    gv = np.clip((.5-speed)/(.5-.3), 0, 1) if speed_gate else np.ones_like(speed)
    target = gt*gv; output = np.zeros_like(target); applied=0.; held=0.
    for i, g in enumerate(target):
        if g > applied: held=hold
        elif held>0: held=max(0,held-DT); g=max(g,applied)
        tau=attack if g>applied else release
        applied += (1-np.exp(-DT/tau))*(g-applied)
        output[i]=applied
    return gt, gv, output


def main():
    results={}
    for label, name in FILES.items():
        path=DATA/name
        with path.open(newline='',encoding='utf-8-sig') as f:
            rows=list(csv.DictReader(line for line in f if not line.startswith('#')))
        t=np.asarray([float(r['timestamp_us'])*1e-6 for r in rows])
        position=np.asarray([float(r['position_rad']) for r in rows])
        velocity=np.asarray([float(r['velocity_raw_rad_s']) for r in rows])
        valid=np.asarray([float(r['feedback_age_us'])<1e8 for r in rows])
        t,position,velocity=t[valid],position[valid],velocity[valid]
        if len(t)<2: raise ValueError(f'no usable feedback: {name}')
        grid=t[0]+np.arange(int((t[-1]-t[0])*FS)+1)*DT
        idx=np.clip(np.searchsorted(t,grid,side='right')-1,0,len(t)-1)
        p,v=position[idx],velocity[idx]
        bpf,rms,speed=detector(p,v)
        variants={}
        for gate in (False,True):
            gt,gv,g=adaptive(rms,speed,gate)
            active=np.flatnonzero(g>.5)
            requested=np.flatnonzero(gt*gv>.5)
            binary=g>.5
            latency=(float(grid[active[0]]-grid[requested[0]]) if len(active) and len(requested) else None)
            variants['angle_plus_speed' if gate else 'angle_only']={
                'active_fraction_gamma_gt_0_5':float(np.mean(g>.5)),
                'mean_gamma':float(np.mean(g)),
                'peak_angle_rms_deg':float(np.max(rms)*180/np.pi),
                'peak_mean_speed_rad_s':float(np.max(speed)),
                'latency_after_raw_target_crossing_s':latency,
                'last_gamma_above_0_5_time_s':float(grid[active[-1]]) if len(active) else None,
                'gamma_threshold_onsets':int(np.count_nonzero(binary[1:] & ~binary[:-1])),
            }
        runs=[]
        for attack,release,hold in ((.03,.2,.1),(.05,.5,.25),(.1,1.0,.5)):
            _,_,g=adaptive(rms,speed,True,attack,release,hold)
            runs.append({'attack_s':attack,'release_s':release,'hold_s':hold,
                         'mean_gamma':float(np.mean(g)),'active_fraction_gamma_gt_0_5':float(np.mean(g>.5))})
        results[label]={'source':name,'rows':len(rows),'valid_feedback_rows':int(valid.sum()),
            'interval_us_min':float(np.diff(t).min()*1e6),'interval_us_max':float(np.diff(t).max()*1e6),
            'replayed_samples_2khz':len(grid),'angle_only_and_speed_gate':variants,'attack_release_hold_candidates':runs}
        stride=10  # Keep replay output compact (200 Hz); all calculations above run at 2 kHz.
        output=tuple(a[::stride] for a in (grid,p,v,bpf,rms,rms*180/np.pi,speed,*adaptive(rms,speed,True)))
        np.savetxt(OUT/f'{label}_replay.csv',np.column_stack(output),delimiter=',',
                   header='time_s,position_wrapped_rad,velocity_rad_s,angle_bpf_rad,angle_rms_rad,angle_rms_deg,mean_speed_rad_s,gamma_angle,gamma_speed,gamma_final',comments='')
    (OUT/'offline_results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print(json.dumps(results,indent=2))


if __name__=='__main__': main()
