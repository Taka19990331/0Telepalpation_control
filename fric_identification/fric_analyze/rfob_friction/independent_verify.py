"""Independent NumPy implementation of the FR0007 RFOB estimator.

This is a numeric fallback/audit because the current MATLAB installation can
fail with License Error 96. The production MATLAB entry point remains main.m.
"""
import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
INPUT = PROJECT / "data" / "FR0007.csv"
OUTPUT = HERE / "results" / "FR0007"
OUTPUT.mkdir(parents=True, exist_ok=True)
J = 0.0116827933542018
FC = (5.0, 10.0, 20.0)
FEEDBACK_TIMEOUT = 0.020
MAX_FILTER_GAP = 0.002
MAX_FEEDBACK_GAP = 0.002
STEP_GUARD = 0.010
MIN_SPEED = 0.10


def read_input():
    metadata = {}
    with INPUT.open(encoding="utf-8-sig", newline="") as stream:
        while True:
            line = stream.readline()
            if not line:
                raise ValueError("FR0007 data header not found")
            line = line.rstrip("\r\n")
            if line.startswith("# config = "):
                metadata["config"] = json.loads(line[len("# config = "):])
            if not line.startswith("#"):
                names = next(csv.reader([line]))
                break
        rows = list(csv.DictReader(stream, fieldnames=names))
    return metadata, rows


def num(rows, name, dtype=float):
    return np.array([dtype(row[name]) for row in rows])


meta, rows = read_input()
t_raw = num(rows, "actual_us")
row_time = (t_raw - t_raw[0]) * 1e-6
phase = num(rows, "phase", int)
step = num(rows, "step_index", int)
state = num(rows, "settling_status", int)
event = num(rows, "event", int)
target = num(rows, "target_torque_Nm")
speed = num(rows, "speed_rad_s")
torque = num(rows, "feedback_torque_estimate_Nm")
age = num(rows, "feedback_age_us") * 1e-6
sequence = num(rows, "feedback_sequence", int)
error = num(rows, "motor_error", int)
tx = num(rows, "can_tx_accepted", int)
rx_raw = num(rows, "feedback_rx_us")
rx = rx_raw * 1e-6
t = (rx_raw - rx_raw[0]) * 1e-6
n = len(rows)

valid = (np.isfinite(t) & np.isfinite(torque) & np.isfinite(speed) &
         np.isfinite(age) & (age >= 0) & (age <= FEEDBACK_TIMEOUT) &
         (sequence > 0) & (error == 0) & (tx != 0))
valid[1:] &= np.diff(sequence) != 0
timing_ok = np.ones(n, dtype=bool)
previous_valid = None
for i in np.flatnonzero(valid):
    if previous_valid is not None and (t[i] <= t[previous_valid] or
                                       t[i] - t[previous_valid] > MAX_FEEDBACK_GAP):
        timing_ok[i] = False
    previous_valid = i

step_elapsed = np.full(n, np.nan)
start_time = np.nan
active_key = None
for i in range(n):
    key = (phase[i], step[i])
    if (event[i] & 1) != 0 or key != active_key:
        start_time = row_time[i] if (event[i] & 1) != 0 else np.nan
        active_key = key
    if np.isfinite(start_time):
        step_elapsed[i] = row_time[i] - start_time

interval = (valid & np.isin(phase, [1, 3]) & np.isin(state, [1, 2]) &
            (np.abs(speed) >= MIN_SPEED) & (np.abs(target) > 0) &
            (step_elapsed >= STEP_GUARD) & (state != 4) & timing_ok)

estimates = np.full((n, len(FC)), np.nan)
segments = []
for b, fc in enumerate(FC):
    g = 2 * math.pi * fc
    y = np.nan
    previous = None
    count = 0
    for i in np.flatnonzero(valid):
        u = torque[i] + J * g * speed[i]
        if previous is None or t[i] - t[previous] > MAX_FILTER_GAP or t[i] <= t[previous]:
            y = u
            count += 1
        else:
            alpha = -math.expm1(-g * (t[i] - t[previous]))
            y = alpha * u + (1 - alpha) * y
        estimates[i, b] = y - J * g * speed[i]
        previous = i
    segments.append(count)

columns = ["Time [s]", "Phase", "Step index", "Target torque [Nm]",
           "Feedback torque [Nm]", "Speed [rad/s]",
           "Estimated friction torque [Nm]", "Q-filter cutoff [Hz]",
           "Adopted inertia J [kg*m^2]", "Valid feedback flag",
           "Analysis interval flag", "Settling status", "Feedback age [s]",
           "Feedback sequence", "Time from step start [s]", "Command row time [s]"]
with (OUTPUT / "rfob_friction_estimates.csv").open("w", newline="", encoding="utf-8-sig") as stream:
    writer = csv.writer(stream)
    writer.writerow(columns)
    for b, fc in enumerate(FC):
        for i in range(n):
            value = estimates[i, b]
            writer.writerow([f"{t[i]:.9g}", phase[i], step[i], f"{target[i]:.9g}",
                f"{torque[i]:.9g}", f"{speed[i]:.9g}",
                "" if not np.isfinite(value) else f"{value:.9g}", fc, f"{J:.16g}",
                int(valid[i]), int(interval[i]), state[i], f"{age[i]:.9g}",
                sequence[i], "" if not np.isfinite(step_elapsed[i]) else f"{step_elapsed[i]:.9g}",
                f"{row_time[i]:.9g}"])

summary = []
for b, fc in enumerate(FC):
    for phase_id in (1, 3):
        for direction in (1, -1):
            mask = (interval & (phase == phase_id) & (np.sign(speed) == direction) &
                    np.isfinite(estimates[:, b]))
            if mask.any():
                x = estimates[mask, b]
                summary.append({"CutoffHz": fc, "Phase": phase_id, "Direction": direction,
                    "SampleCount": int(mask.sum()), "MeanNm": float(x.mean()),
                    "MedianNm": float(np.median(x)), "StdNm": float(x.std(ddof=1)),
                    "MinNm": float(x.min()), "MaxNm": float(x.max())})
with (OUTPUT / "rfob_bandwidth_summary.csv").open("w", newline="", encoding="utf-8-sig") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(summary[0]))
    writer.writeheader()
    writer.writerows(summary)

feedback_dt = np.diff(rx[valid])
feedback_dt = feedback_dt[np.isfinite(feedback_dt) & (feedback_dt > 0)]
active_valid = valid & np.isin(phase, [1, 3])
target_feedback_delta = target[active_valid] - torque[active_valid]
report = {
    "calculation": "Independent NumPy reproduction of main.m; exact ZOH first-order low-pass with variable feedback_rx_us dt",
    "input": str(INPUT), "row_count": n, "valid_feedback_count": int(valid.sum()),
    "analysis_interval_count": int(interval.sum()),
    "analysis_interval_time_s": [float(t[interval].min()), float(t[interval].max())] if interval.any() else None,
    "feedback_interval_median_s": float(np.median(feedback_dt)),
    "feedback_interval_mean_s": float(feedback_dt.mean()),
    "feedback_interval_max_s": float(feedback_dt.max()),
    "nominal_actual_interval_median_s": float(np.median(np.diff(row_time))),
    "time_alignment": "feedback torque and speed share feedback_rx_us; observer dt uses feedback receive timestamps; command torque remains on actual_us row time",
    "target_feedback_difference_median_Nm": float(np.median(target_feedback_delta)),
    "target_feedback_difference_median_absolute_Nm": float(np.median(np.abs(target_feedback_delta))),
    "target_torque_change_count": int(np.count_nonzero(np.diff(target))),
    "cutoff_hz": FC, "inertia_J_kg_m2": J,
    "inertia_source": str(PROJECT / "sys_identification" / "chirp_analysis_multi" / "results" / "20261021_164640_755" / "common_parameters.csv"),
    "inertia_scope": "all-fit descriptive model, selected 13-trial chirp analysis",
    "inertia_B_Nm_s_rad": 0.0221120689229121,
    "motor": "AK80-8 Motor ID 2; CAN2; FR0007 config IS_60=0, IS_80A=0",
    "axis_convention": "AK80 standard MIT output coordinates; position rad, speed rad/s, torque Nm; no extra 8:1 conversion",
    "discretization": "exact ZOH, alpha=1-exp(-2*pi*fc*dt), dt from feedback_rx_us",
    "selection_rule": "phase 1/3; state SETTLING or ACQUIRING; fresh unique feedback; abs(speed)>=0.1 rad/s; nonzero target; 10 ms after step start; feedback gap <=2 ms; COASTING excluded",
    "settings": {"feedback_timeout_s": FEEDBACK_TIMEOUT, "max_feedback_gap_s": MAX_FEEDBACK_GAP,
                 "max_filter_gap_s": MAX_FILTER_GAP, "step_start_guard_s": STEP_GUARD,
                 "minimum_moving_speed_rad_s": MIN_SPEED},
    "filter_reinitializations_per_cutoff": segments,
    "cutoff_summary": summary,
    "friction_estimate_warning": "J is conditional on uncalibrated command torque gain; feedback torque is internal motor estimate, not external sensor torque."
}
with (OUTPUT / "independent_verification.json").open("w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2)
with (OUTPUT / "analysis_metadata.json").open("w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2)
print(json.dumps(report, indent=2))
