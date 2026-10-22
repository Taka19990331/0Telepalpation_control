%% VelRamp_steady_analysis.m
% VelRamp raw CSV -> (1) original-like speed/torque plot,
% (2) steady-only speed/torque plot, (3) diagnostic plot,
% (4) steady samples CSV, (5) per-command-step summary CSV.
%
% Place this MATLAB file in fric_VelRamp. The newest unprocessed
% VelRamp_*.csv in fric_VelRamp\data is used by default.
% No toolboxes are required (tested for MATLAB R2024b syntax; not run on actual CSV).

clear; clc;

%% ======================= USER SETTINGS ===============================
P.csvFile = '';                  % Empty: newest data/VelRamp_*.csv; or absolute CSV path
P.shortWindowSamples = 10;       % 10 samples = 5 ms at 2 kHz
P.maxShortRangeRadS = 0.50;      % max(v)-min(v) <= 0.50 rad/s (Python sweep recommendation)
P.excludeAfterStepSec = 0.030;  % reject early response after a command step
P.minContinuousSec = 0.030;     % consecutive valid interval (30 ms)

% A 10-sample / 5-ms test ALONE accepts a slowly changing ramp.
% Reject ramps by comparing 10-sample-smoothed speeds 60 ms apart.
P.enableLongTrendCheck = true;
P.trendLookbackSec = 0.060;      % 60 ms
P.maxSpeedSlopeRadS2 = 5.0;     % |delta smoothed velocity / dt| <= 5 rad/s^2
P.maxCommandSlopeRadS2 = 5.0;   % also exclude continuously changing commands

P.commandStepJumpRadS = 0.050; % |delta command| >= this -> new step
P.requireFreshFeedback = false; % false: held-last CAN values are allowed
P.rawPlotEveryN = 8;            % Python equivalent: every 8th raw point
P.speedBinWidthRadS = 0.25;     % Python equivalent: speed bin width
P.minRawBinSamples = 20;        % Python equivalent: >= 20 points per bin
P.minSteadyBinSamples = 10;
P.xLimits = [-11.5, 11.5];      % Python equivalent axis limits
P.yLimits = [-0.9, 0.9];
P.showFigures = true;
%% =====================================================================

analysisDir = fileparts(mfilename('fullpath'));
dataDir = fullfile(analysisDir, 'data');
% Also support keeping this script in an optional child folder such as
% analysis_steady, with data/ alongside that child folder.
if ~isfolder(dataDir)
    parentDataDir = fullfile(fileparts(analysisDir), 'data');
    if isfolder(parentDataDir), dataDir = parentDataDir; end
end
outDir = fullfile(analysisDir, 'results');
if ~isfolder(outDir), mkdir(outDir); end

if isempty(P.csvFile)
    files = dir(fullfile(dataDir, 'VelRamp_*.csv'));
    files = files(~contains({files.name}, '_steady') & ...
                  ~contains({files.name}, '_summary'));
    if isempty(files)
        error('VelRamp:NoCSV', 'No raw VelRamp_*.csv found in %s', dataDir);
    end
    [~, newest] = max([files.datenum]);
    csvFile = fullfile(files(newest).folder, files(newest).name);
else
    csvFile = char(P.csvFile);
end
fprintf('Input CSV: %s\n', csvFile);

[data, names, meta] = readVelRampNumericCSV(csvFile);
getColumn = @(key) getNumericColumn(data, names, key);
t_us = getColumn('timestamp_us');
u = getColumn('command_rad_s');
v = getColumn('velocity_rad_s');
tau = getColumn('torque_estimate_nm');
stage = getColumn('stage');
feedbackValid = getColumn('feedback_valid');

n = numel(v);
t = (t_us - t_us(1)) * 1e-6;
dtAll = diff(t);
dtValid = dtAll(isfinite(dtAll) & dtAll > 0 & dtAll < 0.1);
if isempty(dtValid), error('VelRamp:Time', 'No valid positive time intervals.'); end
dt = median(dtValid);
fs = 1/dt;
fprintf('Samples: %d | median fs: %.2f Hz | duration: %.3f s\n', n, fs, t(end));

validRaw = isfinite(t) & isfinite(v) & isfinite(tau) & ...
           isfinite(u) & isfinite(stage);
if P.requireFreshFeedback
    validRaw = validRaw & feedbackValid == 1;
end
validStage = ismember(stage, 0:7);
validRaw = validRaw & validStage;

% Four groups retain the Python code's exact stage mapping and colors.
groupNames = {'Positive acceleration', 'Positive return to zero', ...
              'Negative acceleration', 'Negative return to zero'};
groupStages = {[0,1], [2,3], [4,5], [6,7]};
colors = [0 114 178; 213 94 0; 0 158 115; 204 121 167]/255;
edges = P.xLimits(1):P.speedBinWidthRadS:P.xLimits(2);

% A new step begins on a discrete >= 0.05 rad/s command jump, a stage
% change, or invalid/discontinuous timestamps. A continuous ramp (which has
% smaller per-sample changes) is handled by the slope checks below.
isBreak = [true; diff(stage) ~= 0 | ...
                  abs(diff(u)) >= P.commandStepJumpRadS | ...
                  ~isfinite(diff(u)) | ...
                  ~isfinite(diff(stage)) | ...
                  ~isfinite(diff(t)) | diff(t) <= 0 | diff(t) > 0.05];
segmentId = cumsum(isBreak);
segmentStarts = find(isBreak);
segmentEnds = [segmentStarts(2:end)-1; n];
numSegments = numel(segmentStarts);

steady = false(n,1);
shortRange = NaN(n,1);
speedSlope = NaN(n,1);
commandSlope = NaN(n,1);

for q = 1:numSegments
    idx = (segmentStarts(q):segmentEnds(q))';
    if numel(idx) < P.shortWindowSamples || ~all(validStage(idx))
        continue;
    end
    vs = v(idx);
    us = u(idx);
    ts = t(idx);
    good = validRaw(idx);
    len = numel(idx);
    shortN = P.shortWindowSamples;

    % All short-window samples must be finite and inside the same segment.
    localRange = movmax(vs, [shortN-1, 0]) - movmin(vs, [shortN-1, 0]);
    goodCount = movsum(double(good), [shortN-1, 0]);
    localRange(1:shortN-1) = NaN;
    shortRange(idx) = localRange;
    candidate = isfinite(localRange) & (localRange <= P.maxShortRangeRadS) & ...
                (goodCount == shortN) & ...
                (ts - ts(1) >= P.excludeAfterStepSec);

    if P.enableLongTrendCheck
        lag = max(1, round(P.trendLookbackSec / dt));
        if len <= lag
            candidate(:) = false;
        else
            % Smoothing with the SAME 10-sample window reduces CAN speed
            % quantization before evaluating longer-term velocity drift.
            vAvg = movmean(vs, [shortN-1, 0], 'omitnan');
            uAvg = movmean(us, [shortN-1, 0], 'omitnan');
            localVSlope = NaN(len,1);
            localUSlope = NaN(len,1);
            dtLag = ts(lag+1:end)-ts(1:end-lag);
            okTime = dtLag > 0 & isfinite(dtLag);
            numeratorV = abs(vAvg(lag+1:end)-vAvg(1:end-lag));
            numeratorU = abs(uAvg(lag+1:end)-uAvg(1:end-lag));
            tmpV = NaN(numel(dtLag),1);
            tmpU = tmpV;
            tmpV(okTime) = numeratorV(okTime)./dtLag(okTime);
            tmpU(okTime) = numeratorU(okTime)./dtLag(okTime);
            localVSlope(lag+1:end) = tmpV;
            localUSlope(lag+1:end) = tmpU;
            speedSlope(idx) = localVSlope;
            commandSlope(idx) = localUSlope;
            candidate = candidate & ...
                        localVSlope <= P.maxSpeedSlopeRadS2 & ...
                        localUSlope <= P.maxCommandSlopeRadS2;
        end
    end

    % Keep entire continuous valid intervals ONLY when they last long enough.
    changes = diff([false; candidate(:); false]);
    runStarts = find(changes == 1);
    runEnds = find(changes == -1)-1;
    for k = 1:numel(runStarts)
        spanSec = ts(runEnds(k)) - ts(runStarts(k)) + dt;
        if spanSec >= P.minContinuousSec
            steady(idx(runStarts(k):runEnds(k))) = true;
        end
    end
end

fprintf('Steady samples: %d / %d (%.2f%%)\n', nnz(steady), n, 100*nnz(steady)/n);
if ~any(steady)
    warning('VelRamp:NoSteady', ['No steady samples detected. Check the ', ...
        'diagnostic PNG, especially the configured range and slope limits.']);
end

[~, stem, ~] = fileparts(csvFile);

% Export raw-column-complete steady samples with their original timestamps.
rawT = array2table(data, 'VariableNames', names);
rawT.segment_id = segmentId;
steadyCsv = fullfile(outDir, [stem '_steady.csv']);
writetable(rawT(steady,:), steadyCsv);

% One summary row PER COMMAND STEP (accel/decel are NOT combined).
stepNumber = (1:numSegments)';
stageCol = NaN(numSegments,1);
commandCol = NaN(numSegments,1);
meanSpeed = NaN(numSegments,1);
stdSpeed = NaN(numSegments,1);
meanTorque = NaN(numSegments,1);
stdTorque = NaN(numSegments,1);
sampleCount = zeros(numSegments,1);
segmentSampleCount = zeros(numSegments,1);
steadySeconds = zeros(numSegments,1);
startSeconds = NaN(numSegments,1);
for q = 1:numSegments
    idx = (segmentStarts(q):segmentEnds(q))';
    sel = idx(steady(idx));
    stageCol(q) = stage(idx(1));
    commandCol(q) = mean(u(idx), 'omitnan');
    segmentSampleCount(q) = numel(idx);
    startSeconds(q) = t(idx(1));
    sampleCount(q) = numel(sel);
    steadySeconds(q) = numel(sel) * dt;
    if ~isempty(sel)
        meanSpeed(q) = mean(v(sel), 'omitnan');
        stdSpeed(q) = std(v(sel), 0, 'omitnan');
        meanTorque(q) = mean(tau(sel), 'omitnan');
        stdTorque(q) = std(tau(sel), 0, 'omitnan');
    end
end
stepSummary = table(stepNumber, stageCol, commandCol, startSeconds, ...
    segmentSampleCount, sampleCount, steadySeconds, meanSpeed, stdSpeed, ...
    meanTorque, stdTorque, sampleCount > 0, ...
    'VariableNames', {'segment_id','stage','command_rad_s', ...
    'start_time_s','segment_samples','steady_samples','steady_duration_s', ...
    'mean_velocity_rad_s','std_velocity_rad_s', ...
    'mean_torque_nm','std_torque_nm','has_steady'});
summaryCsv = fullfile(outDir, [stem '_steady_summary.csv']);
writetable(stepSummary, summaryCsv);

visibility = 'off';
if P.showFigures, visibility = 'on'; end

%% Figure 1: EXACT Python-style raw scatter + speed-bin mean curves
f1 = figure('Name','VelRamp RAW: torque vs speed', 'Color','w', ...
    'Position',[80 80 1430 850], 'Visible',visibility);
hold on;
legendHandles = gobjects(4,1);
for g = 1:4
    idx = find(validRaw & ismember(stage,groupStages{g}));
    idxDraw = idx(1:P.rawPlotEveryN:end);
    if ~isempty(idxDraw)
        scatter(v(idxDraw), tau(idxDraw), 8, colors(g,:), 'filled', ...
            'MarkerFaceAlpha',0.18,'MarkerEdgeColor','none', ...
            'HandleVisibility','off');
    end
    [bx,by] = speedBinMeans(v(idx),tau(idx),edges,P.minRawBinSamples);
    if ~isempty(bx)
        legendHandles(g) = plot(bx,by,'-', 'Color',colors(g,:), ...
            'LineWidth',2.5,'DisplayName',groupNames{g});
    else
        legendHandles(g) = plot(NaN,NaN,'-', 'Color',colors(g,:), ...
            'LineWidth',2.5,'DisplayName',groupNames{g});
    end
end
formatTorqueAxes(P);
legend(legendHandles,groupNames,'Location','northoutside','NumColumns',4);
title(sprintf('VelRamp RAW: measured speed vs motor torque estimate (N=%d)',n), ...
    'Interpreter','none');
subtitle(sprintf('Motor ID %s | %.1f Hz | dots: every %dth sample | mean bin: %.2f rad/s', ...
    getMeta(meta,'motor_id','?'),fs,P.rawPlotEveryN,P.speedBinWidthRadS));
rawPng = fullfile(outDir,[stem '_raw_torque_vs_speed.png']);
exportgraphics(f1,rawPng,'Resolution',180);

%% Figure 2: STEADY-ONLY samples and separate bin means for the four groups
f2 = figure('Name','VelRamp STEADY: torque vs speed', 'Color','w', ...
    'Position',[110 110 1430 850], 'Visible',visibility);
hold on;
legendHandles2 = gobjects(4,1);
for g = 1:4
    idx = find(steady & ismember(stage,groupStages{g}));
    if ~isempty(idx)
        scatter(v(idx),tau(idx),7,colors(g,:),'filled', ...
            'MarkerFaceAlpha',0.22,'MarkerEdgeColor','none', ...
            'HandleVisibility','off');
    end
    [bx,by] = speedBinMeans(v(idx),tau(idx),edges,P.minSteadyBinSamples);
    if ~isempty(bx)
        legendHandles2(g) = plot(bx,by,'-','Color',colors(g,:), ...
            'LineWidth',2.8,'DisplayName',groupNames{g});
    else
        legendHandles2(g) = plot(NaN,NaN,'-','Color',colors(g,:), ...
            'LineWidth',2.8,'DisplayName',groupNames{g});
    end
end
formatTorqueAxes(P);
legend(legendHandles2,groupNames,'Location','northoutside','NumColumns',4);
title(sprintf('VelRamp STEADY only: %d / %d samples (%.1f%%)', ...
    nnz(steady),n,100*nnz(steady)/n),'Interpreter','none');
subtitle(sprintf('10-point range <= %.3f rad/s | slope <= %.2f rad/s^2 | %d ms minimum', ...
    P.maxShortRangeRadS,P.maxSpeedSlopeRadS2,round(P.minContinuousSec*1000)));
steadyPng = fullfile(outDir,[stem '_steady_torque_vs_speed.png']);
exportgraphics(f2,steadyPng,'Resolution',180);

%% Figure 3: diagnostic for tuning thresholds
f3 = figure('Name','VelRamp steady detection diagnostic','Color','w', ...
    'Position',[140 100 1450 850], 'Visible',visibility);
plotEvery = max(1,round(n/15000));
displayIdx = (1:plotEvery:n)';
ax1 = subplot(3,1,1);
plot(t(displayIdx),v(displayIdx),'Color',[0.55 0.62 0.68], ...
    'DisplayName','Measured velocity'); hold on;
plot(t(displayIdx),u(displayIdx),'k:','LineWidth',1.05, ...
    'DisplayName','Command velocity');
scatter(t(steady),v(steady),3,[0 0.48 0.75],'filled', ...
    'MarkerFaceAlpha',0.45,'DisplayName','Accepted steady');
grid on; ylabel('Speed (rad/s)');
title('Speed command / measured velocity / accepted steady samples');
legend('Location','best');

ax2 = subplot(3,1,2);
plot(t(displayIdx),shortRange(displayIdx),'Color',[0.10 0.44 0.76]); hold on;
yline(P.maxShortRangeRadS,'r--',sprintf('Threshold %.2f rad/s',P.maxShortRangeRadS));
grid on; ylabel('10-point range (rad/s)');
title('Instantaneous variation criterion');

ax3 = subplot(3,1,3);
if P.enableLongTrendCheck
    plot(t(displayIdx),speedSlope(displayIdx),'Color',[0.1 0.58 0.33], ...
        'DisplayName','Velocity slope'); hold on;
    plot(t(displayIdx),commandSlope(displayIdx),'Color',[0.75 0.42 0.10], ...
        'DisplayName','Command slope');
    yline(P.maxSpeedSlopeRadS2,'r--','Speed slope threshold', ...
        'HandleVisibility','off');
    grid on; ylabel('Slope magnitude (rad/s^2)');
    legend('Location','best');
    title('Long-term drift (prevents gradual ramps being called steady)');
else
    stairs(t(displayIdx),double(steady(displayIdx)),'Color',[0.1 0.58 0.33]);
    ylabel('Steady flag'); grid on;
    title('Steady flag (long-term trend disabled)');
end
xlabel('Elapsed time (s)');
linkaxes([ax1,ax2,ax3],'x');
xlim([t(1),t(end)]);
diagPng = fullfile(outDir,[stem '_steady_diagnostic.png']);
exportgraphics(f3,diagPng,'Resolution',160);

fprintf('\nOutput directory: %s\n',outDir);
fprintf('  %s\n  %s\n  %s\n  %s\n  %s\n', ...
    steadyCsv,summaryCsv,rawPng,steadyPng,diagPng);

%% ====================== LOCAL FUNCTIONS ===============================
function [data, names, meta] = readVelRampNumericCSV(csvFile)
    % Accept #key,value metadata and the exact ordinary CSV header.
    fid = fopen(csvFile,'rt','n','UTF-8');
    if fid < 0, error('VelRamp:OpenFailed','Cannot open: %s',csvFile); end
    cleanupFile = onCleanup(@() fclose(fid)); %#ok<NASGU>
    meta = struct();
    names = {};
    while true
        line = fgetl(fid);
        if ~ischar(line)
            error('VelRamp:MissingHeader','Missing timestamp_us CSV header in %s',csvFile);
        end
        line = strrep(line,char(65279),'');  % UTF-8 BOM if present
        line = strtrim(line);
        if startsWith(line,'#')
            text = strtrim(line(2:end));
            delim = find(text==',',1,'first');
            if isempty(delim), delim = find(text=='=',1,'first'); end
            if ~isempty(delim)
                key = matlab.lang.makeValidName(lower(strtrim(text(1:delim-1))));
                if ~isempty(key), meta.(key)=strtrim(text(delim+1:end)); end
            end
        elseif startsWith(line,'timestamp_us,')
            names = strtrim(strsplit(line,','));
            break;
        end
    end
    mandatory = {'timestamp_us','command_rad_s','velocity_rad_s', ...
                 'torque_estimate_nm','stage','feedback_valid'};
    if ~all(ismember(mandatory,names))
        error('VelRamp:Columns','CSV header lacks required columns. Found: %s', ...
            strjoin(names,', '));
    end
    formatStr = repmat('%f',1,numel(names));
    loaded = textscan(fid,formatStr,'Delimiter',',', ...
        'CollectOutput',true,'ReturnOnError',false,'EmptyValue',NaN);
    data = loaded{1};
    if isempty(data)
        error('VelRamp:EmptyCSV','No numeric samples found in %s',csvFile);
    end
    if size(data,2) ~= numel(names)
        error('VelRamp:CSVShape','CSV data width does not match header width.');
    end
end

function col = getNumericColumn(data,names,key)
    k = find(strcmp(names,key),1);
    if isempty(k), error('VelRamp:NoColumn','Required column missing: %s',key); end
    col = data(:,k);
end

function value = getMeta(meta,key,fallback)
    field = matlab.lang.makeValidName(lower(key));
    if isfield(meta,field)
        value = meta.(field);
    else
        value = fallback;
    end
end

function [xs,ys] = speedBinMeans(speed,torque,edges,minPoints)
    ok = isfinite(speed) & isfinite(torque);
    speed = speed(ok);
    torque = torque(ok);
    bins = discretize(speed,edges);
    good = ~isnan(bins);
    if ~any(good), xs=[]; ys=[]; return; end
    binN = numel(edges)-1;
    count = accumarray(bins(good),1,[binN,1],@sum,0);
    sumSpeed = accumarray(bins(good),speed(good),[binN,1],@sum,0);
    sumTorque = accumarray(bins(good),torque(good),[binN,1],@sum,0);
    use = count >= minPoints;
    xs = sumSpeed(use)./count(use);
    ys = sumTorque(use)./count(use);
end

function formatTorqueAxes(P)
    grid on; box on;
    xline(0,'-','Color',[0.57 0.62 0.67],'HandleVisibility','off');
    yline(0,'-','Color',[0.57 0.62 0.67],'HandleVisibility','off');
    xlim(P.xLimits); ylim(P.yLimits);
    xticks(-10:2.5:10); yticks(-0.8:0.2:0.8);
    xlabel('Measured motor speed (rad/s)');
    ylabel('Motor torque estimate (N m)');
    set(gca,'FontSize',12,'LineWidth',1.0);
end
