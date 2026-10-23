"""Explore short-window rotation-start detection on data/FR0007.csv."""
from pathlib import Path

import numpy as np
import pandas as pd


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data" / "FR0007.csv"


def detect_rotation_start(position_deg, speed_rad_s, baseline_position_deg,
                          angle_threshold_deg=0.04, angle_samples=4,
                          speed_threshold_rad_s=0.32, speed_samples=2):
    """Return first sample index flagged by angle persistence OR speed persistence.

    Supply a stable zero-torque baseline angle (median over the stationary
    acquisition period). The rule is causal: at sample i it only uses samples
    up to i. Returns None if neither feature persists long enough.
    """
    pos = np.asarray(position_deg, dtype=float)
    vel = np.asarray(speed_rad_s, dtype=float)

    def first_persistent(mask, n):
        if n <= 0:
            raise ValueError("sample counts must be positive")
        if len(mask) < n:
            return None
        runs = np.convolve(mask.astype(int), np.ones(n, dtype=int), mode="valid")
        hit = np.flatnonzero(runs == n)
        return int(hit[0] + n - 1) if len(hit) else None

    angle_hit = first_persistent(np.abs(pos - baseline_position_deg) >= angle_threshold_deg, angle_samples)
    speed_hit = first_persistent(np.abs(vel) >= speed_threshold_rad_s, speed_samples)
    hits = [i for i in (angle_hit, speed_hit) if i is not None]
    return min(hits) if hits else None


def main():
    d = pd.read_csv(DATA, comment="#")
    # Positive torque sweep starts in phase 1. The motor is at rest through
    # step 30; step 31 (0.62 Nm) is the first observed movement.
    sweep = d[(d.phase == 1) & (d.step_index <= 31)].copy().reset_index(drop=True)
    base = sweep[sweep.step_index <= 30]
    move = sweep[sweep.step_index == 31]
    dt = np.median(np.diff(sweep.actual_us)) / 1e6
    base_pos = float(base.position_deg.median())
    # Ignore the transition row's event flag: it denotes a step boundary.
    # Estimate directional velocity from speed and signed position increments.
    dpos = sweep.position_deg.diff().fillna(0.0)
    # Encoder wrapping/glitches are excluded using physical speed plausibility.
    dpos = dpos.where(dpos.abs() < 1.0, 0.0)
    posrate = dpos / (dt * 360.0) * (2 * np.pi)
    speed = sweep.speed_rad_s.to_numpy()
    posrate = posrate.to_numpy()
    t = sweep.actual_us.to_numpy()

    # Baseline is the complete stationary positive-torque sweep through 0.60 Nm.
    v0 = np.abs(base.speed_rad_s.to_numpy())
    p0 = np.abs(base.position_deg.to_numpy() - base_pos)
    print(f"rows={len(d)} dt={dt*1000:.3f} ms baseline_position={base_pos:.6f} deg")
    print(f"stationary |speed| q99={np.quantile(v0,.99):.4f}, q99.9={np.quantile(v0,.999):.4f}, max={v0.max():.4f} rad/s")
    print(f"stationary position deviation q99={np.quantile(p0,.99):.5f}, max={p0.max():.5f} deg")
    print(f"step31 torque=.62 Nm n={len(move)} speed abs max={move.speed_rad_s.abs().max():.4f}; pos range={move.position_deg.min():.5f}..{move.position_deg.max():.5f}")

    # Per-sample operating points: threshold crossings on stationary segment
    # versus first crossing on the movement segment.
    for feature, x0, x1 in [
        ("speed", np.abs(base.speed_rad_s.to_numpy()), np.abs(move.speed_rad_s.to_numpy())),
        ("angle excursion", p0, np.abs(move.position_deg.to_numpy() - base_pos)),
        ("angle step", np.abs(np.diff(base.position_deg.to_numpy(), prepend=base.position_deg.iloc[0])), np.abs(np.diff(move.position_deg.to_numpy(), prepend=move.position_deg.iloc[0]))),
    ]:
        thresholds = np.unique(np.r_[np.quantile(x0, [.99, .999, .9999]), x0.max(), np.quantile(x1, [.5,.75,.9]), x1.max()])
        print(f"\n{feature}: threshold | stationary exceed / {len(x0)} | first step31 crossing (ms from step start)")
        for th in thresholds:
            idx = np.flatnonzero(x1 >= th)
            print(f"{th:.5f} | {(x0 >= th).sum()} | {(t[len(base)+idx[0]]-t[len(base)]) / 1000:.2f}" if len(idx) else f"{th:.5f} | {(x0 >= th).sum()} | none")

    # Test simple N-consecutive speed and angle-excursion rules over *all*
    # stationary samples, then find earliest trigger in step 31.
    print("\nRule sweep (consecutive samples; any direction; stationary false runs count):")
    candidates = [
        ("speed", float(np.quantile(v0,.999)), np.abs(sweep.speed_rad_s.to_numpy())),
        ("speed", float(np.quantile(v0,.9999)), np.abs(sweep.speed_rad_s.to_numpy())),
        ("angle excursion", .02, np.abs(sweep.position_deg.to_numpy()-base_pos)),
        ("angle excursion", .04, np.abs(sweep.position_deg.to_numpy()-base_pos)),
    ]
    stat_end = len(base)
    for name, th, x in candidates:
        for n in range(1, 6):
            mask = x >= th
            run = np.convolve(mask.astype(int), np.ones(n, dtype=int), mode="valid") == n
            false = int(run[:max(0,stat_end-n+1)].sum())
            onset_idx = np.flatnonzero(run[stat_end-n+1:])
            delay = ((stat_end-n+1+onset_idx[0]+n-1) if len(onset_idx) else None)
            delay_ms = (t[delay]-t[stat_end])/1000 if delay is not None and delay < len(t) else None
            print(f"{name:16s} th={th:.5f} N={n} stationary_trigger_windows={false} step31_delay_ms={delay_ms}")

    hit = detect_rotation_start(
        sweep.position_deg, sweep.speed_rad_s, base_pos,
        angle_threshold_deg=.04, angle_samples=4,
        speed_threshold_rad_s=.32, speed_samples=2,
    )
    print(f"\nRecommended OR rule: angle >=0.04 deg for 4 samples OR |speed| >=0.32 rad/s for 2 samples")
    if hit is not None:
        print(f"first trigger index={hit}, t={t[hit]/1000:.2f} ms, torque={sweep.target_torque_Nm.iloc[hit]:.2f} Nm, step={sweep.step_index.iloc[hit]}")


if __name__ == "__main__":
    main()
