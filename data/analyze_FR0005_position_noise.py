"""Read FR0005 without modifying it; summarize stationary position quantization.

Run with the bundled Python runtime. Outputs are in FR0005_position_analysis/.
The primary windows have continuously zero logged torque, not isolated zero rows
selected from a feedback-dependent braking controller.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
import pandas as pd


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "FR0005.csv"
OUT = ROOT / "FR0005_position_analysis"
OUT.mkdir(exist_ok=True)
d = pd.read_csv(SOURCE, comment="#")
t = d.actual_us / 1e6
valid = d.feedback_sequence.gt(0) & d.motor_error.eq(0) & d.feedback_age_us.le(20000)
fresh = d.feedback_sequence.ne(d.feedback_sequence.shift()) & valid
q_rad = 25 / 65535
q_deg = np.rad2deg(q_rad)

# First run ends when torque first becomes nonzero. The second window is the
# final 1000 records of the 2 s continuous zero-torque coast, before braking.
first_nonzero = np.flatnonzero(d.target_torque_Nm.ne(0))[0]
coast_start = np.flatnonzero(d.reason.eq(4) & d.target_torque_Nm.eq(0))[0]
brake_start = coast_start + np.flatnonzero(d.target_torque_Nm.iloc[coast_start:].ne(0))[0]
initial = d.loc[valid & (d.index < first_nonzero)]
post = d.iloc[brake_start - 1000:brake_start]
assert valid.loc[post.index].all()
assert initial.target_torque_Nm.eq(0).all() and post.target_torque_Nm.eq(0).all()
braking = d.loc[d.settling_status.eq(5) & valid]
braking_zero = braking.loc[braking.target_torque_Nm.eq(0)]


def stats(name, frame):
    p = frame.position_deg.to_numpy()
    ts = frame.actual_us.to_numpy() / 1e6
    center = np.median(p)
    slope, intercept = np.polyfit(ts - ts[0], p - center, 1)
    residual = (p - center) - (slope * (ts - ts[0]) + intercept)
    values, counts = np.unique(p, return_counts=True)
    modal_index = np.argmax(counts)
    result = dict(
        window=name, n=len(p), start_s=float(ts[0]), end_s=float(ts[-1]),
        endpoint_span_s=float(ts[-1] - ts[0]), mean_deg=float(p.mean()),
        std_deg=float(p.std(ddof=1)), std_rad=float(np.deg2rad(p.std(ddof=1))),
        min_deg=float(p.min()), max_deg=float(p.max()),
        p2p_deg=float(np.ptp(p)), p2p_rad=float(np.deg2rad(np.ptp(p))),
        central_95_width_deg=float(np.quantile(p, .975) - np.quantile(p, .025)),
        abs_from_median_p95_deg=float(np.quantile(abs(p-center), .95)),
        slope_deg_s=float(slope), detrended_std_deg=float(residual.std(ddof=1)),
        first_to_last_deg=float(p[-1]-p[0]),
        mode_deg=float(values[modal_index]), mode_percent=float(100*counts[modal_index]/len(p)),
        levels=int(len(values)), mean_speed_rad_s=float(frame.speed_rad_s.mean()),
        std_speed_rad_s=float(frame.speed_rad_s.std(ddof=1)),
        unique_feedback=int(frame.feedback_sequence.nunique()),
        max_feedback_age_us=int(frame.feedback_age_us.max()),
    )
    for suffix, v in [("initial", initial), ("post_coast", post)]:
        if name == suffix:
            f = v.loc[fresh.loc[v.index]]
            result["fresh_only_std_deg"] = float(f.position_deg.std(ddof=1))
    return result


frames = [("initial", initial), ("post_coast", post),
          ("braking_all_reference", braking), ("braking_zero_rows_reference", braking_zero)]
summary = pd.DataFrame([stats(name, frame) for name, frame in frames])
summary.to_csv(OUT / "position_summary.csv", index=False, encoding="utf-8-sig")
occupancy = []
for name, frame in frames:
    for value, count in frame.position_deg.value_counts().sort_index().items():
        occupancy.append(dict(window=name, position_deg=value, n=count, percent=count/len(frame)*100))
pd.DataFrame(occupancy).to_csv(OUT / "position_levels.csv", index=False, encoding="utf-8-sig")

# Equal-duration diagnostics around stopping; no speed threshold is used to
# filter individual records, which would bias the observed position noise.
bins = []
for start in np.arange(31.3465, 32.3465, .1):
    frame = d.loc[(t >= start-1e-9) & (t < start+.1-1e-9)]
    bins.append(stats(f"{start:.4f}-{start+.1:.4f}", frame))
pd.DataFrame(bins).to_csv(OUT / "coast_100ms_windows.csv", index=False, encoding="utf-8-sig")

plot_data = {name: frame[['actual_us', 'position_deg']].to_dict(orient='list') for name, frame in frames[:2]}
(OUT / 'plot_data.json').write_text(json.dumps(plot_data), encoding='utf-8')

metadata = dict(source=str(SOURCE), sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                rows=len(d), invalid_rows=int((~valid).sum()),
                sample_period_us=sorted(d.actual_us.diff().dropna().unique().tolist()),
                position_lsb_rad=q_rad, position_lsb_deg=q_deg,
                coast_start_s=float(t.iloc[coast_start]), brake_start_s=float(t.iloc[brake_start]),
                feedback_repeated_rows=int((valid & ~fresh).sum()),
                summary=summary.to_dict(orient="records"))
(OUT / "analysis.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
print(summary.to_string(index=False))
print(f"Position LSB: {q_deg:.12g} deg = {q_rad:.12g} rad")
print(f"Outputs: {OUT}")
