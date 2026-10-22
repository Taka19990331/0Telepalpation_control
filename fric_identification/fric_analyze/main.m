% MAIN Run the offline AK80 friction data analysis.
clear; clc;
cfg = config();
if ~isfile(cfg.input_csv), error('Input CSV not found: %s', cfg.input_csv); end
if ~isfolder(cfg.output_dir), mkdir(cfg.output_dir); end
[samples, meta] = load_friction_csv(cfg.input_csv);

% Report what is present in the source before evaluating any steady windows.
report_phase_coverage(samples);

criteria = ["Strict", "Relaxed 1", "Relaxed 2"];
multipliers = [1, cfg.steady.relaxed_multipliers(:)'];
if numel(multipliers) ~= numel(criteria) || any(~isfinite(multipliers)) || any(multipliers < 1)
    error('cfg.steady.relaxed_multipliers must contain two finite values >= 1.');
end

steadyByCriterion = cell(1, numel(criteria));
diagnosticsByCriterion = cell(1, numel(criteria));
for c = 1:numel(criteria)
    criterionCfg = cfg;
    criterionCfg.steady.max_abs_accel_rad_s2 = cfg.steady.max_abs_accel_rad_s2 * multipliers(c);
    criterionCfg.steady.max_speed_std_rad_s = cfg.steady.max_speed_std_rad_s * multipliers(c);
    [steadyByCriterion{c}, diagnosticsByCriterion{c}] = extract_steady_state(samples, meta, criterionCfg);
end

% Preserve the established files as the Strict result for backward compatibility.
steadyStrict = steadyByCriterion{1};
diagnosticsStrict = diagnosticsByCriterion{1};
writetable(steadyStrict, fullfile(cfg.output_dir,'steady_state_data.csv'));
writetable(diagnosticsStrict, fullfile(cfg.output_dir,'steady_state_diagnostics.csv'));

comparison = make_comparison_points(steadyByCriterion{1}, criteria(1));
comparisonDiagnostics = diagnosticsByCriterion{1};
comparisonDiagnostics.Criterion = repmat(criteria(1),height(comparisonDiagnostics),1);
summaryRows = cell(numel(criteria), 1);
strictKeys = [steadyStrict.Phase, steadyStrict.StepIndex];
summaryRows{1} = summarize_criterion(steadyStrict, criteria(1), multipliers(1), cfg, strictKeys);
for c = 2:numel(criteria)
    points = make_comparison_points(steadyByCriterion{c}, criteria(c));
    comparison = [comparison; points]; %#ok<AGROW>
    criterionDiagnostics = diagnosticsByCriterion{c};
    criterionDiagnostics.Criterion = repmat(criteria(c),height(criterionDiagnostics),1);
    comparisonDiagnostics = [comparisonDiagnostics; criterionDiagnostics]; %#ok<AGROW>
    summaryRows{c} = summarize_criterion(steadyByCriterion{c}, criteria(c), ...
        multipliers(c), cfg, strictKeys);
end
comparisonDiagnostics = movevars(comparisonDiagnostics,'Criterion','Before',1);
writetable(comparisonDiagnostics, fullfile(cfg.output_dir,'steady_state_comparison_diagnostics.csv'));
writetable(comparison, fullfile(cfg.output_dir,'steady_state_comparison.csv'));
summary = vertcat(summaryRows{:});
writetable(summary, fullfile(cfg.output_dir,'steady_state_comparison_summary.csv'));

plot_friction_results(samples, steadyStrict, diagnosticsStrict, comparison, cfg, cfg.output_dir);

fprintf('Input rows: %d\n',height(samples));
fprintf('Strict thresholds: |acceleration| <= %.6g rad/s^2; speed std <= %.6g rad/s\n', ...
    cfg.steady.max_abs_accel_rad_s2, cfg.steady.max_speed_std_rad_s);
fprintf('Relaxed multipliers: %.6g, %.6g (only the two thresholds above are scaled)\n', ...
    multipliers(2), multipliers(3));
fprintf('\nSteady-point comparison (all commanded-torque steps; plots may hide zero-command steps):\n');
fprintf('%-12s %8s %8s %12s %12s %12s %15s\n', ...
    'Criterion','Accepted','MOVING','STATIONARY','UNCERTAIN','New vs Strict','Max |v| MOVING');
plotRanges = NaN(height(summary),2);
for c = 1:height(summary)
    plotRanges(c,:) = visible_speed_range(comparison,summary.Criterion(c),cfg);
end
strictMin = plotRanges(1,1); strictMax = plotRanges(1,2);
for c = 1:height(summary)
    fprintf('%-12s %8d %8d %12d %12d %12d %15.6g\n', ...
        char(summary.Criterion(c)), summary.AcceptedStepCount(c), summary.MovingCount(c), ...
        summary.StationaryCount(c), summary.UncertainCount(c), summary.NewStepsVsStrict(c), ...
        summary.MovingMaxAbsSpeedRadS(c));
    fprintf('  MOVING positive/negative speed: %d / %d; threshold limits: %.6g rad/s^2, %.6g rad/s\n', ...
        summary.MovingPositiveSpeedCount(c), summary.MovingNegativeSpeedCount(c), ...
        summary.AccelerationThresholdRadS2(c), summary.SpeedStdThresholdRadS(c));
    fprintf('  Fig. 2 MOVING speed range: %s; extension beyond Strict (low/high): %s / %s rad/s\n', ...
        format_numeric_range(plotRanges(c,:)), ...
        format_extension(strictMin,plotRanges(c,1),true), ...
        format_extension(strictMax,plotRanges(c,2),false));
end
fprintf('Results: %s\n',cfg.output_dir);

function report_phase_coverage(T)
fprintf('\nSource phase coverage (from FR0007.csv):\n');
for ph = 1:4
    ix = double(T.phase) == ph;
    if ~any(ix)
        fprintf('Phase %d: absent from source CSV; no phase data were synthesized.\n', ph);
        continue
    end
    steps = unique(double(T.step_index(ix)));
    target = double(T.target_torque_Nm(ix));
    speed = double(T.speed_rad_s(ix));
    target = target(isfinite(target)); speed = speed(isfinite(speed));
    if isempty(target), targetRange = [NaN NaN]; else, targetRange = [min(target), max(target)]; end
    if isempty(speed), speedRange = [NaN NaN]; else, speedRange = [min(speed), max(speed)]; end
    fprintf(['Phase %d: samples=%d; unique steps=%d; command torque=[%.6g, %.6g] Nm; ' ...
        'speed=[%.6g, %.6g] rad/s\n'], ph, sum(ix), numel(steps), ...
        targetRange(1), targetRange(2), speedRange(1), speedRange(2));
end
end

function P = make_comparison_points(S, criterion)
P = table(S.Phase, S.StepIndex, S.TargetTorqueNm, repmat(string(criterion),height(S),1), ...
    S.SelectedWindowStartTimeS, S.SelectedWindowEndTimeS, S.NumValidFeedbackSamples, ...
    S.MeanSpeedRadS, S.MeanFeedbackTorqueNm, S.EstimatedAccelerationRadS2, ...
    S.SpeedStdRadS, S.MotionClassification, S.DynamicFrictionUsable, ...
    'VariableNames', {'phase','step_index','target_torque_Nm','criterion', ...
    'window_start_time_s','window_end_time_s','valid_sample_count','mean_speed_rad_s', ...
    'mean_feedback_torque_Nm','estimated_acceleration_rad_s2','speed_std_rad_s', ...
    'classification','dynamic_friction_usable'});
end

function row = summarize_criterion(S, criterion, multiplier, cfg, strictKeys)
moving = S.MotionClassification == "MOVING";
stationary = S.MotionClassification == "STATIONARY";
uncertain = S.MotionClassification == "UNCERTAIN";
positive = moving & S.MeanSpeedRadS > 0;
negative = moving & S.MeanSpeedRadS < 0;
if any(moving), maxAbsSpeed = max(abs(S.MeanSpeedRadS(moving))); else, maxAbsSpeed = NaN; end
if any(moving), minSpeed = min(S.MeanSpeedRadS(moving)); maxSpeed = max(S.MeanSpeedRadS(moving));
else, minSpeed = NaN; maxSpeed = NaN; end
keys = [S.Phase, S.StepIndex];
if isempty(keys), newVsStrict = 0; else, newVsStrict = sum(~ismember(keys, strictKeys, 'rows')); end
row = table(string(criterion), multiplier, height(S), sum(moving), sum(stationary), ...
    sum(uncertain), sum(positive), sum(negative), minSpeed, maxSpeed, maxAbsSpeed, newVsStrict, ...
    cfg.steady.max_abs_accel_rad_s2*multiplier, cfg.steady.max_speed_std_rad_s*multiplier, ...
    'VariableNames', {'Criterion','Multiplier','AcceptedStepCount','MovingCount', ...
    'StationaryCount','UncertainCount','MovingPositiveSpeedCount', ...
    'MovingNegativeSpeedCount','MovingMinSpeedRadS','MovingMaxSpeedRadS', ...
    'MovingMaxAbsSpeedRadS','NewStepsVsStrict', ...
    'AccelerationThresholdRadS2','SpeedStdThresholdRadS'});
end

function range = visible_speed_range(P,criterion,cfg)
ix = P.criterion==criterion & P.classification=="MOVING" & ...
    isfinite(P.mean_speed_rad_s) & isfinite(P.mean_feedback_torque_Nm);
if ~cfg.plot.show_zero_torque
    ix = ix & abs(P.target_torque_Nm)>cfg.plot.zero_torque_tolerance_Nm;
end
if any(ix)
    range = [min(P.mean_speed_rad_s(ix)),max(P.mean_speed_rad_s(ix))];
else
    range = [NaN NaN];
end
end

function out = format_numeric_range(range)
if any(~isfinite(range))
    out = '<no MOVING points visible>';
else
    out = sprintf('[%.6g, %.6g]',range(1),range(2));
end
end

function out = format_extension(strictEdge,criterionEdge,isLowEdge)
if ~isfinite(strictEdge) || ~isfinite(criterionEdge)
    out = 'n/a';
elseif isLowEdge
    out = sprintf('%.6g',max(0,strictEdge-criterionEdge));
else
    out = sprintf('%.6g',max(0,criterionEdge-strictEdge));
end
end
