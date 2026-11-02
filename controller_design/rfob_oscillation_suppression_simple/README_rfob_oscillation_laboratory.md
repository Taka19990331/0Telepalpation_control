# RFOB oscillation suppression laboratory

## Purpose

This version separates three questions so their results are not conflated:

1. **Measured-log replay:** Does the detector produce a suitable extra torque on the recorded signals? The measured rotor trajectory stays fixed, so this cannot predict the changed motion.
2. **Simple-plant controller comparison:** With the same finite-duration external pulse applied to each run, how do baseline, notch, and damping variants change the simulated response and the assumed slow-motion transparency?
3. **Autonomous baseline screen:** Does the no-test-input model reproduce the measured oscillation? A failed screen remains a warning and blocks claims that the simulated controller effect predicts the measured self-oscillation.

The default pulse is a raised-cosine pulse (0.8 Nm peak, 40 ms, beginning at 0.25 s). It excites the modeled plant and lets its natural response ring down. It does not impose a continuous 12 Hz force or claim to reproduce the physical source of the measured oscillation. The optional multisine is an explicitly prescribed frequency-response experiment.

## Files

- `rfob_oscillation_analysis.m`: analysis function and local implementation.
- `run_rfob_oscillation_laboratory.m`: editable MATLAB entry point.

## Run

1. Keep `rfob_oscillation_analysis.m`, `run_rfob_oscillation_laboratory.m`, and the three CSV logs in the expected project layout, or edit `cfg.dataDir` and `cfg.files` near the configuration section in the function.
2. Open and run `run_rfob_oscillation_laboratory.m` in MATLAB R2021b or newer.
3. The function expects Signal Processing Toolbox (`pwelch`, `freqz`, and filter design functions); it does not use Simulink.

The default run expects the three logs named in `cfg.files`, because it also performs the measured-log replay and autonomous baseline screen. Edit `cfg.dataDir` and `cfg.files` when your log folder or filenames differ.

## Main controls

- Controller variant: `cfg.controller.mode = 'none'`, `'notch'`, `'damping'`, or `'compare'`.
- Damping coefficient: `cfg.controller.damping` in Nm s/rad.
- Detector band, thresholds, RMS time constant, notch, and gain smoothing: `cfg.controller.*` near the top of `rfob_oscillation_analysis.m`.
- Test input: `cfg.sim.testInput.type` is `'none'`, `'pulse'`, or `'multisine'`. Pulse fields are `start_s`, `duration_s`, and `amplitude_Nm`. Multisine uses `frequencies_Hz` and `multisineAmplitude_Nm`.
- Parameter sweep and robustness runs are off by default; enable them in the runner when desired.
- Figure visibility and saving are controlled by `cfg.visible`, `cfg.savePNG`, `cfg.saveFIG`, and `cfg.outputDir`.

## Interpretation limits

The simple-plant results depend on assumed inertia, grip stiffness/damping, friction, motor delay, and actuator model. They test controller behavior under a specified disturbance and are not a validated estimate of real-world suppression percentage. A positive damping-work metric also does not alone prove closed-loop stability. The measured-log replay is useful for detector behavior, generated command, saturation, phase, and power checks, but not for the resulting motion after the added torque changes the trajectory.
