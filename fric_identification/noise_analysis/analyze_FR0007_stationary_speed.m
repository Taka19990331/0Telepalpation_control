%% Analyze speed noise and quantization while FR0007 is stationary.
% The stationary condition mirrors StopWindow in friction_core.h:
%   * 2001 samples (1.000 s at PERIOD_US = 500 us)
%   * unwrapped position range < 0.003 rad
%   * absolute window-mean speed < 0.03 rad/s
%
% Run from any current folder. Results are written beside this script.

scriptDir = fileparts(mfilename('fullpath'));
repoRoot = fileparts(fileparts(scriptDir));
addpath(fullfile(repoRoot, 'fric_identification', 'fric_analyze'));
inputFile = fullfile(repoRoot, 'data', 'FR0007.csv');
outputCsv = fullfile(scriptDir, 'FR0007_stationary_speed_histogram.csv');
outputPng = fullfile(scriptDir, 'FR0007_stationary_speed_histogram.png');

% AK80 feedback speed quantization: +/-37.5 rad/s over 4095 intervals.
deltaOmega = 75 / 4095;
samplePeriodUs = 500;
windowUs = 1000000;
windowSamples = windowUs / samplePeriodUs + 1; % Both endpoints included.
positionRangeLimitRad = 0.003;
meanSpeedLimitRadS = 0.03;

if ~isfile(inputFile)
    error('Input CSV not found: %s', inputFile);
end

[T, meta] = load_friction_csv(inputFile);
required = {'actual_us', 'position_deg', 'speed_rad_s'};
missing = required(~ismember(required, T.Properties.VariableNames));
if ~isempty(missing)
    error('Input CSV is missing required columns: %s', strjoin(missing, ', '));
end

timeUs = T.actual_us;
positionRad = deg2rad(T.position_deg);
speed = T.speed_rad_s;
valid = isfinite(timeUs) & isfinite(positionRad) & isfinite(speed);
if any(~valid)
    warning('Ignoring %d rows with invalid time, position, or speed.', nnz(~valid));
    timeUs = timeUs(valid);
    positionRad = positionRad(valid);
    speed = speed(valid);
end
if numel(speed) < windowSamples
    error('Need at least %d valid samples; found %d.', windowSamples, numel(speed));
end
if any(diff(timeUs) ~= samplePeriodUs)
    error('Valid samples must be contiguous and spaced by %d us.', samplePeriodUs);
end

% Match the Arduino's correction for the motor's +/-12.5 rad position wrap.
unwrappedPosition = zeros(size(positionRad));
unwrappedPosition(1) = positionRad(1);
for k = 2:numel(positionRad)
    delta = positionRad(k) - positionRad(k-1);
    if delta > 12.5
        delta = delta - 25;
    elseif delta < -12.5
        delta = delta + 25;
    end
    unwrappedPosition(k) = unwrappedPosition(k-1) + delta;
end

% Each endpoint's trailing window matches StopWindow::observe().
meanSpeed = movmean(speed, [windowSamples-1, 0], 'Endpoints', 'shrink');
positionMax = movmax(unwrappedPosition, [windowSamples-1, 0], 'Endpoints', 'shrink');
positionMin = movmin(unwrappedPosition, [windowSamples-1, 0], 'Endpoints', 'shrink');
positionRange = positionMax - positionMin;
windowReady = (1:numel(speed))' >= windowSamples;
stationaryWindowEnd = windowReady & positionRange < positionRangeLimitRad & ...
    abs(meanSpeed) < meanSpeedLimitRadS;

% Include every sample belonging to at least one qualifying 1 s window.
stationarySample = movmax(double(stationaryWindowEnd), [0, windowSamples-1]) > 0;
stationarySpeeds = speed(stationarySample);
if isempty(stationarySpeeds)
    error(['No FR0007 samples met the Arduino stationary condition ' ...
        '(position range < %.4g rad and |mean speed| < %.4g rad/s).'], ...
        positionRangeLimitRad, meanSpeedLimitRadS);
end

% Quantize samples to integer feedback counts, with count zero at 0 rad/s.
countValues = round(stationarySpeeds / deltaOmega);
maxAbsCount = max(abs(countValues));
if maxAbsCount == 0
    maxAbsCount = 1;
end
countLevels = (-maxAbsCount:maxAbsCount)';
levelIndex = countValues + maxAbsCount + 1;
speedLevels = countLevels * deltaOmega;
counts = accumarray(levelIndex, 1, [numel(countLevels), 1]);
percent = counts * (100 / numel(stationarySpeeds));
result = table(countLevels, speedLevels, counts, percent, ...
    'VariableNames', {'speed_count', 'speed_rad_s', 'count', 'percent_of_stationary_samples'});
writetable(result, outputCsv);

fig = figure('Color', 'w', 'Position', [100, 100, 1400, 760]);
mainPosition = [0.10, 0.25, 0.86, 0.62];
ax = axes('Parent', fig, 'Position', mainPosition);
stemHandle = stem(ax, speedLevels, percent, 'Color', [0.12, 0.43, 0.68], ...
    'LineWidth', 3, 'Marker', 'none');
stemHandle.BaseLine.Visible = 'off';
hold(ax, 'on');
grid(ax, 'on');
ax.FontSize = 11;
occupiedCounts = countLevels(counts > 0);
minOccupiedCount = min(occupiedCounts);
maxOccupiedCount = max(occupiedCounts);
xlim(ax, [minOccupiedCount-0.5, maxOccupiedCount+0.5] * deltaOmega);
physicalTicks = (minOccupiedCount:maxOccupiedCount) * deltaOmega;
xticks(ax, physicalTicks);
xtickformat(ax, '%.5f');
ylim(ax, [0, max(percent) * 1.18]);
ylabel(ax, 'Share of stationary samples [%]');
title(ax, sprintf('FR0007 stationary speed distribution (%d samples)', ...
    numel(stationarySpeeds)));
subtitle(ax, sprintf(['Arduino stop window: %d samples (%.3f s), position range < %.3f rad, ' ...
    '|mean speed| < %.3f rad/s; 1 ct = %.5f rad/s'], windowSamples, windowUs / 1e6, ...
    positionRangeLimitRad, meanSpeedLimitRadS, deltaOmega));
for k = 1:numel(speedLevels)
    text(ax, speedLevels(k), percent(k), sprintf('%.2f%%', percent(k)), ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom', ...
        'FontSize', 8, 'Clipping', 'off');
end

% A second, aligned bottom axis reports integer feedback counts.
countTicks = minOccupiedCount:maxOccupiedCount;
countLabels = strings(size(countTicks));
countLabels(countTicks == 0) = "0";
countLabels(countTicks > 0) = compose('+%d ct', countTicks(countTicks > 0));
countLabels(countTicks < 0) = compose('%d ct', countTicks(countTicks < 0));
mainPosition = ax.Position;
xlabel(ax, 'Reported speed [rad/s]');
ax2 = axes('Parent', fig, 'Position', ...
    [mainPosition(1), mainPosition(2)-0.105, mainPosition(3), mainPosition(4)], ...
    'Color', 'none', 'XAxisLocation', 'bottom', 'YAxisLocation', 'right', ...
    'YTick', [], 'YColor', 'none', 'Box', 'off', 'XLim', ax.XLim, ...
    'XTick', countTicks * deltaOmega, 'XTickLabel', countLabels, ...
    'FontSize', 10, 'TickLength', [0.008, 0.008]);
xlabel(ax2, 'Feedback speed count');
ax2.XLabel.Position(2) = -0.065;
exportgraphics(fig, outputPng, 'Resolution', 180);

fprintf('Input: %s\n', inputFile);
fprintf('FR0007 stop confirmed in metadata: %s\n', ...
    string(isfield(meta.result, 'stop_confirmed') && meta.result.stop_confirmed));
fprintf('Speed quantum: %.10f rad/s per count\n', deltaOmega);
fprintf('Qualifying 1 s windows: %d\n', nnz(stationaryWindowEnd));
fprintf('Stationary samples: %d / %d (%.2f%%)\n', ...
    numel(stationarySpeeds), numel(speed), 100 * numel(stationarySpeeds) / numel(speed));
fprintf('Histogram CSV: %s\n', outputCsv);
fprintf('Histogram PNG: %s\n', outputPng);

close(fig);
