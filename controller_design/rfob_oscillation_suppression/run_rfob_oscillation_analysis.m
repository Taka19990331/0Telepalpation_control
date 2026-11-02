%% RFOB oscillation suppression laboratory
% Open in MATLAB. Save As > MATLAB Live Code (*.mlx) if an inline Live Script
% is wanted. Analysis logic resides entirely in rfob_oscillation_analysis.m.
% Requires Signal Processing Toolbox; no Simulink or private Live Editor API.
addpath(fileparts(mfilename('fullpath')));
%% Configuration overrides (defaults are in main CONFIGURATION section)
cfg = struct;
cfg.controller.mode = 'compare';
cfg.controller.damping = 0.1; % Nm s/rad, initial design point only
cfg.RUN_PARAMETER_SWEEP = false;
cfg.RUN_ROBUSTNESS_ANALYSIS = false;
cfg.figures = true(1,10);
cfg.savePNG = false;
%% Run
results = rfob_oscillation_analysis(cfg);
%% Inspect numerical results
if isfield(results,'closedSummary'), disp(results.closedSummary); end
if isfield(results,'validation'), disp(results.validation); end
% results.cfg contains the complete effective configuration after log import.
% results.friction{3}.reconstruction quantifies log reconstruction residuals.
