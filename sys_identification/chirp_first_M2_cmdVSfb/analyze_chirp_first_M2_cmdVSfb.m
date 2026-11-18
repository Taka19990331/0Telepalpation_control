% Simple M2 command-vs-feedback chirp amplitude analysis.
% Run this script from MATLAB. Outputs are written under ./results/<timestamp>.
root = fileparts(mfilename('fullpath'));
captureDir = fullfile(fileparts(root), 'captures');
outputRoot = fullfile(root, 'results');

% Reference settings used by chirp_analysis_multi (Motor ID 2).
referenceFile = 'chirp_20261020_102301_899188.csv';
referencePath = fullfile(captureDir, referenceFile);
assert(isfile(referencePath), 'Reference capture not found: %s', referencePath);
[~, referenceMeta] = readCaptureWithMetadata(referencePath);
ref = readConditions(referenceMeta);

files = dir(fullfile(captureDir, 'chirp_*.csv'));
[~, order] = sort({files.name});
files = files(order);
trials = {};
for i = 1:numel(files)
    path = fullfile(files(i).folder, files(i).name);
    try
        [T, M] = readCaptureWithMetadata(path);
        c = readConditions(M);
        if ~sameConditions(c, ref) || ~isHealthy(M, height(T))
            continue
        end
        % Match chirp_analysis_multi's acquisition cutoff. Both reference
        % captures and later captures with the same settings are retained.
        if files(i).datenum < datenum(2026,10,20,10,23,1) && ...
                ~strcmp(files(i).name, 'chirp_20261020_101838_880284.csv')
            continue
        end
        required = {'index','phase','command_Nm', ...
            'feedback_torque_estimate_Nm','speed_rad_s'};
        assert(all(ismember(required, T.Properties.VariableNames)), ...
            'Required telemetry columns are missing.');
        sweepRows = find(T.phase == 1);
        assert(~isempty(sweepRows), 'No sweep rows in capture.');
        sweepRows = sweepRows(1):sweepRows(end);
        assert(all(T.phase(sweepRows) == 1), 'Sweep phase is not contiguous.');
        idx0 = T.index(sweepRows(1));
        t = (T.index(sweepRows) - idx0) ./ c.fs;
        assert(numel(t) == round(c.T*c.fs), 'Unexpected sweep length.');
        trials{end+1,1} = struct('name', files(i).name, 't', t, ...
            'command', T.command_Nm(sweepRows), ...
            'feedback', T.feedback_torque_estimate_Nm(sweepRows), ...
            'speed', T.speed_rad_s(sweepRows), 'c', c); %#ok<SAGROW>
    catch ME
        warning('Skipping %s: %s', files(i).name, ME.message);
    end
end
assert(~isempty(trials), 'No valid same-condition Motor ID 2 captures found.');

% Resample onto the common nominal sweep clock, then build time-averaged waves.
fs = ref.fs;
tGrid = (0:round(ref.T*fs)-1)' ./ fs;
N = numel(trials);
command = nan(numel(tGrid),N);
feedback = command;
speed = command;
for i = 1:N
    command(:,i) = interp1(trials{i}.t, trials{i}.command, tGrid, 'linear', NaN);
    feedback(:,i) = interp1(trials{i}.t, trials{i}.feedback, tGrid, 'linear', NaN);
    speed(:,i) = interp1(trials{i}.t, trials{i}.speed, tGrid, 'linear', NaN);
end
meanCommandWave = mean(command,2,'omitnan');
meanFeedbackWave = mean(feedback,2,'omitnan');
meanSpeedWave = mean(speed,2,'omitnan');
nAtTime = sum(isfinite(command),2);

% Estimate each trial's local sinusoidal amplitude by least-squares fitting
% against the known instantaneous logarithmic chirp phase. A four-cycle
% window limits frequency smearing while retaining enough samples at low f.
nFreq = 180;
freq = logspace(log10(ref.f0), log10(ref.f1), nFreq)';
k = log(ref.f1/ref.f0)/ref.T;
phase = 2*pi*ref.f0*expm1(k*tGrid)/k;
phaseCenters = 2*pi*ref.f0*expm1(k*(log(freq/ref.f0)/k))/k;
commandAmp = nan(nFreq,N);
feedbackAmp = nan(nFreq,N);
speedAmp = nan(nFreq,N);
for i = 1:N
    commandAmp(:,i) = localAmplitude(tGrid, command(:,i), phase, phaseCenters);
    feedbackAmp(:,i) = localAmplitude(tGrid, feedback(:,i), phase, phaseCenters);
    speedAmp(:,i) = localAmplitude(tGrid, speed(:,i), phase, phaseCenters);
end
meanCommandAmp = mean(commandAmp,2,'omitnan');
meanFeedbackAmp = mean(feedbackAmp,2,'omitnan');
stdFeedbackAmp = std(feedbackAmp,0,2,'omitnan');
meanSpeedAmp = mean(speedAmp,2,'omitnan');
stdSpeedAmp = std(speedAmp,0,2,'omitnan');
% Plot amplitudes from the time-averaged waveforms; trial-wise standard
% deviations show the repeat-to-repeat spread around those average responses.
meanWaveCommandAmp = localAmplitude(tGrid,meanCommandWave,phase,phaseCenters);
meanWaveFeedbackAmp = localAmplitude(tGrid,meanFeedbackWave,phase,phaseCenters);
meanWaveSpeedAmp = localAmplitude(tGrid,meanSpeedWave,phase,phaseCenters);

stamp = char(datetime('now','Format','yyyyMMdd_HHmmss'));
outDir = fullfile(outputRoot, stamp);
if ~isfolder(outDir), mkdir(outDir); end
waveTable = table(tGrid, ref.f0*exp(k*tGrid), meanCommandWave, ...
    meanFeedbackWave, meanSpeedWave, nAtTime, ...
    'VariableNames', {'time_s','frequency_Hz','mean_command_Nm', ...
    'mean_feedback_torque_Nm','mean_speed_rad_s','trial_count'});
writetable(waveTable, fullfile(outDir,'mean_waveforms.csv'));
amplitudeTable = table(freq, meanWaveCommandAmp, meanCommandAmp, ...
    meanWaveFeedbackAmp, meanFeedbackAmp, stdFeedbackAmp, ...
    meanWaveSpeedAmp, meanSpeedAmp, stdSpeedAmp, ...
    'VariableNames', {'frequency_Hz','command_amplitude_mean_wave_Nm', ...
    'command_amplitude_trial_mean_Nm','feedback_amplitude_mean_wave_Nm', ...
    'feedback_amplitude_trial_mean_Nm','feedback_amplitude_trial_std_Nm', ...
    'speed_amplitude_mean_wave_rad_s','speed_amplitude_trial_mean_rad_s', ...
    'speed_amplitude_trial_std_rad_s'});
writetable(amplitudeTable, fullfile(outDir,'amplitude_summary.csv'));
trialNames = string(cellfun(@(x)x.name,trials,'UniformOutput',false))';
trialSummary = table(trialNames, 'VariableNames', {'file'});
writetable(trialSummary, fullfile(outDir,'included_trials.csv'));

% Figure 1: command and feedback torque amplitudes. Shading shows feedback
% trial-to-trial standard deviation; center lines come from mean waveforms.
fig1 = figure('Color','w','Name','M2 torque amplitude: command vs feedback');
ax = axes(fig1); hold(ax,'on');
fill(ax, [freq; flipud(freq)], ...
    [max(0,meanWaveFeedbackAmp-stdFeedbackAmp); flipud(meanWaveFeedbackAmp+stdFeedbackAmp)], ...
    [0.80 0.87 1.00], 'EdgeColor','none','FaceAlpha',0.55, ...
    'DisplayName','Feedback +/- 1 SD');
semilogx(ax, freq, meanWaveCommandAmp, 'k-', 'LineWidth',1.8, ...
    'DisplayName','Command (mean waveform)');
semilogx(ax, freq, meanWaveFeedbackAmp, 'b-', 'LineWidth',1.8, ...
    'DisplayName','Feedback (mean waveform)');
grid(ax,'on'); box(ax,'on');
xlabel(ax,'Frequency (Hz)'); ylabel(ax,'Torque amplitude (Nm)');
title(ax,sprintf('M2 command vs feedback torque amplitude (n = %d)',N));
legend(ax,'Location','best'); xlim(ax,[ref.f0 ref.f1]);
exportgraphics(fig1, fullfile(outDir,'figure_1_torque_amplitude.png'), 'Resolution',200);
savefig(fig1, fullfile(outDir,'figure_1_torque_amplitude.fig'));

% Figure 2: feedback speed amplitude, mean and standard deviation.
fig2 = figure('Color','w','Name','M2 feedback speed amplitude');
ax = axes(fig2); hold(ax,'on');
fill(ax, [freq; flipud(freq)], ...
    [max(0,meanWaveSpeedAmp-stdSpeedAmp); flipud(meanWaveSpeedAmp+stdSpeedAmp)], ...
    [0.80 0.87 1.00], 'EdgeColor','none','FaceAlpha',0.55, ...
    'DisplayName','Feedback speed +/- 1 SD');
semilogx(ax, freq, meanWaveSpeedAmp, 'b-', 'LineWidth',1.8, ...
    'DisplayName','Feedback speed (mean waveform)');
grid(ax,'on'); box(ax,'on');
xlabel(ax,'Frequency (Hz)'); ylabel(ax,'Speed amplitude (rad/s)');
title(ax,sprintf('M2 feedback speed amplitude (n = %d)',N));
legend(ax,'Location','best'); xlim(ax,[ref.f0 ref.f1]);
exportgraphics(fig2, fullfile(outDir,'figure_2_feedback_speed.png'), 'Resolution',200);
savefig(fig2, fullfile(outDir,'figure_2_feedback_speed.fig'));

fprintf('Included %d same-condition Motor ID 2 trials.\n', N);
fprintf('Results: %s\n', outDir);

function [T,M] = readCaptureWithMetadata(path)
lines = splitlines(string(fileread(path)));
commentMask = startsWith(strtrim(lines),'#');
comments = lines(commentMask);
keys = strings(0,1); values = strings(0,1);
for j = 1:numel(comments)
    s = char(comments(j)); eq = find(s=='=',1);
    if ~isempty(eq)
        keys(end+1,1) = strtrim(s(2:eq-1)); %#ok<AGROW>
        values(end+1,1) = strtrim(s(eq+1:end)); %#ok<AGROW>
    end
end
M = table(keys,values,'VariableNames',{'Key','Value'});
header = find(~commentMask & strlength(strtrim(lines))>0,1);
assert(~isempty(header), 'CSV data header not found.');
opts = detectImportOptions(path,'NumHeaderLines',header-1, ...
    'VariableNamingRule','preserve');
T = readtable(path,opts);
end

function c = readConditions(M)
c.format = getMeta(M,'FORMAT');
c.status = getMeta(M,'RESULT.status_name');
c.valid = getMeta(M,'RESULT.valid');
c.complete = getMeta(M,'RESULT.complete');
c.f0 = metaNumber(M,'START_FREQUENCY_HZ (f0)');
c.f1 = metaNumber(M,'END_FREQUENCY_HZ (f1)');
c.T = metaNumber(M,'SWEEP_DURATION_S (T)');
c.A0 = metaNumber(M,'START_AMPLITUDE_NM (A0)');
c.A1 = metaNumber(M,'END_AMPLITUDE_NM (A1)');
c.a = metaNumber(M,'AMPLITUDE_EXPONENT (a)');
c.fs = metaNumber(M,'SAMPLING_FREQUENCY_HZ (fs)');
c.Kp = metaNumber(M,'KP_PD (Kp) [Nm/deg]');
c.Kd = metaNumber(M,'KD_PD (Kd) [Nm/(rad/s)]');
c.motorId = metaNumber(M,'CONFIG.MotorID');
c.limit = metaNumber(M,'TORQUE_LIMIT_NM (tau_limit)');
end

function yes = sameConditions(a,b)
names = {'f0','f1','T','A0','A1','a','fs','Kp','Kd','motorId','limit'};
yes = strcmp(a.format,'CHIRPSI1_CSV_WITH_METADATA_V1');
for j=1:numel(names)
    yes = yes && isfinite(a.(names{j})) && ...
        abs(a.(names{j})-b.(names{j})) <= 1e-6*max(1,abs(b.(names{j})));
end
yes = yes && a.motorId == 2;
end

function yes = isHealthy(M,nRows)
yes = strcmp(getMeta(M,'RESULT.valid'),'True') && ...
    strcmp(getMeta(M,'RESULT.complete'),'True') && ...
    strcmp(getMeta(M,'RESULT.status_name'),'COMPLETE') && ...
    strcmp(getMeta(M,'RESULT.errors'),'[]') && ...
    metaNumber(M,'RESULT.rows') == nRows;
end

function value = getMeta(M,key)
idx = find(M.Key==key,1);
assert(~isempty(idx), 'Missing metadata key: %s', key);
value = char(M.Value(idx));
end

function value = metaNumber(M,key)
value = str2double(getMeta(M,key));
assert(isfinite(value), 'Invalid numeric metadata: %s', key);
end

function amp = localAmplitude(t,x,phase,centers)
% Fit x = a*sin(phase) + b*cos(phase) + offset + linear trend.
amp = nan(size(centers));
halfWindow = 4*pi; % two cycles on either side of the center
for j=1:numel(centers)
    use = abs(phase-centers(j)) <= halfWindow & isfinite(x);
    if nnz(use) < 20, continue; end
    tt = t(use); pp = phase(use); xx = x(use);
    tc = tt - mean(tt);
    A = [sin(pp), cos(pp), ones(size(pp)), tc];
    beta = A \ xx;
    amp(j) = hypot(beta(1),beta(2));
end
end
