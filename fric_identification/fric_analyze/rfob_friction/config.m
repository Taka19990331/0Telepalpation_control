function cfg = config()
%CONFIG Settings for RFOB-style friction estimation on FR0007.
root = fileparts(mfilename('fullpath'));
projectRoot = fileparts(fileparts(fileparts(root)));
cfg.input_csv = fullfile(projectRoot, 'data', 'FR0007.csv');
cfg.output_dir = fullfile(root, 'results', 'FR0007');

% Adopted from the latest multi-trial chirp result (all-fit descriptive model).
cfg.inertia_J_kg_m2 = 0.0116827933542018;
cfg.inertia_B_Nm_s_rad = 0.0221120689229121; % provenance only; not used by observer
cfg.inertia_source = fullfile(projectRoot, ...
    'sys_identification', 'chirp_analysis_multi', 'results', ...
    '20261021_164640_755', 'common_parameters.csv');
cfg.motor_model = 'AK80-8, Motor ID 2 (FR0007 config: IS_60=0, IS_80A=0)';
cfg.axis_convention = 'AK80 standard MIT output coordinates; angle rad, speed rad/s, torque Nm; no extra 8:1 conversion';

cfg.rfob.cutoff_hz = [5, 10, 20];
cfg.feedback_timeout_s = 0.020;
cfg.max_feedback_gap_s = 0.002;
cfg.max_filter_gap_s = 0.002;
cfg.step_start_guard_s = 0.010;
cfg.minimum_moving_speed_rad_s = 0.10;
cfg.exclude_coasting = true;

cfg.plot.fig1 = true;
cfg.plot.fig2 = true;
cfg.plot.diag_bandwidth = false;
cfg.plot.diag_update_interval = false;
cfg.plot.diag_sync = false;
cfg.plot.diag_filter_transient = false;
end
