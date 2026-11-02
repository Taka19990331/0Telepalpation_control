%% RFOB oscillation suppression laboratory
% Run from this folder in MATLAB. Save As > MATLAB Live Code (*.mlx) if desired.
% The analysis logic is in rfob_oscillation_analysis.m.
addpath(fileparts(mfilename('fullpath')));

%% Controller settings
cfg = struct;
cfg.controller.mode = 'compare'; % none / notch / damping / compare
cfg.controller.damping = 0.1;    % Nm s/rad; starting point, not optimized

%% Optional runtime switches
cfg.RUN_PARAMETER_SWEEP = false;
cfg.RUN_ROBUSTNESS_ANALYSIS = false;
cfg.figures = true(1,10);
cfg.savePNG = false;

%% Optional simple-plant test input
% This pulse excites the assumed plant and its natural ring-down. It is a
% design-test input, not a reconstruction of the measured self-oscillation.
cfg.sim.testInput.type = 'pulse'; % none / pulse / multisine
cfg.sim.testInput.start_s = 0.25;
cfg.sim.testInput.duration_s = 0.04;
cfg.sim.testInput.amplitude_Nm = 0.8;

%% Run and inspect
results = rfob_oscillation_analysis(cfg);
if isfield(results,'closedSummary'), disp(results.closedSummary); end
if isfield(results,'transparencySummary'), disp(results.transparencySummary); end
if isfield(results,'validation'), disp(results.validation); end
% results.replay summarizes fixed-trajectory replay on measured logs.
% results.autonomousBaseline is the separate no-input baseline screen.
