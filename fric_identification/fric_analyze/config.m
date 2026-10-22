function cfg = config()
%CONFIG Settings for offline friction identification analysis.
root = fileparts(mfilename('fullpath'));
cfg.input_csv = fullfile(fileparts(fileparts(root)), 'data', 'FR0007.csv');
cfg.output_dir = fullfile(root, 'results', 'FR0007');

cfg.steady.window_s = 0.5;
cfg.steady.min_valid_duration_s = 0.4;
cfg.steady.duration_step_s = 0.025;
cfg.steady.expected_feedback_period_s = 0.0005;
% 1.5 samples allows timestamp phase; an invalid first/last sample still fails.
cfg.steady.window_boundary_tolerance_s = 0.00075;
cfg.steady.window_duration_tolerance_s = 0.001;
cfg.steady.max_abs_accel_rad_s2 = 0.3;
cfg.steady.max_speed_std_rad_s = 0.08;
% Threshold multipliers for independent steady-window evaluations.
cfg.steady.relaxed_multipliers = [2, 4];
% Require tighter dispersion before calling a low-speed interval moving/usable.
cfg.steady.low_speed_max_speed_std_rad_s = 0.04;
cfg.steady.low_speed_threshold_rad_s = 0.1;
cfg.steady.max_feedback_age_s = 0.020;
cfg.steady.max_feedback_gap_s = 0.002;
cfg.steady.max_invalid_feedback_fraction = 0.02;
cfg.steady.max_invalid_feedback_streak_s = 0.005;
% One count is about 0.0183 rad/s for the configured 75 rad/s velocity span.
cfg.motion.stationary_speed_resolution_rad_s = 0.02;
cfg.motion.stationary_position_range_rad = 0.003;
cfg.motion.moving_min_net_displacement_rad = 0.003;
cfg.motion.moving_min_position_slope_rad_s = 0.01;
cfg.motion.position_quantization_rad = (12.5 - (-12.5)) / 65535;
cfg.motion.velocity_consistency_tolerance_rad_s = 0.04;
cfg.motion.slip_bin_s = 0.02;
cfg.motion.slip_min_displacement_rad = 3 * cfg.motion.position_quantization_rad;
cfg.motion.slip_min_pause_s = 0.05;

cfg.plot.raw_velocity_torque = true;
cfg.plot.steady_velocity_torque = true;
cfg.plot.steady_interval_timeseries = false;
cfg.plot.steady_metrics = false;
cfg.plot.show_errorbars = false;
cfg.plot.show_motion_classification = true;
% Fig. 2 defaults to the MOVING subset for dynamic-friction inspection.
cfg.plot.show_moving_only = true;
% Zero-command/coasting samples remain in the CSV and diagnostics, but are
% hidden from the two standard plots unless explicitly requested.
cfg.plot.show_zero_torque = false;
cfg.plot.zero_torque_tolerance_Nm = 1e-9;
cfg.plot.steady_threshold_distributions = false;
cfg.plot.diagnostic_phase = [];
cfg.plot.diagnostic_step_index = [];
end
