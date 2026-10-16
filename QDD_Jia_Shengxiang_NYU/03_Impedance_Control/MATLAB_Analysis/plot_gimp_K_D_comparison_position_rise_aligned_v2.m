%% GIMP K/D comparison - position-rise aligned
% Figure 1: D = 0, compare K = 4, 8, 12
% Figure 2: K = 12, compare D = 0, 0.5, 1
%
% IMPORTANT:
%   Alignment is now based on the POSITION RESPONSE RISE ONSET,
%   matching the points marked by the professor.
%
% Definition of t = 0:
%   For each run, estimate
%       q_low  = pre-release negative position plateau
%       q_final = post-release steady position
%   and set t = 0 at the first sustained crossing of
%
%       q_threshold = q_low + riseFraction*(q_final - q_low)
%
%   with riseFraction = 0.10 by default.
%
% This aligns the "start moving upward" point of the position response,
% rather than the maximum negative hand-push torque.
%
% No Signal Processing Toolbox is required.

clear; close all; clc;

outputDir = fileparts(mfilename('fullpath'));
if isempty(outputDir)
    outputDir = pwd;
end

%% ------------------------------------------------------------------------
% File mapping
% -------------------------------------------------------------------------

rootK = 'C:\Users\123\Desktop\gimp_data\gimp_k4_8_12_d0';
rootD = 'C:\Users\123\Desktop\gimp_data\gimp_k12_d0.5_1';

fileK4D0   = fullfile(rootK,'motor_data_20260821_185020.csv');
fileK8D0   = fullfile(rootK,'motor_data_20260821_185052.csv');
fileK12D0  = fullfile(rootK,'motor_data_20260821_185121.csv'); % baseline
fileK12D05 = fullfile(rootD,'motor_data_20260821_185346.csv');
fileK12D1  = fullfile(rootD,'motor_data_20260821_185419.csv');

%% ------------------------------------------------------------------------
% Settings
% -------------------------------------------------------------------------

smoothWindow = 5;

% Position-rise alignment
riseFraction = 0.05;      % 5% of position recovery from negative plateau
holdTime_s = 0.030;       % threshold must remain crossed for 30 ms

% Windows used to estimate pre-release plateau and final steady value
preWindow_s  = 0.35;      % window immediately before the position rise
postWindow_s = [0.70 1.10]; % steady region after rise onset, refined iteratively

% Display windows relative to aligned rise onset
plotWindowK = [-0.30 0.85];
plotWindowD = [-0.30 0.85];

% Colors
cK4   = [0.00 0.62 0.72];
cK8   = [0.35 0.27 0.78];
cBase = [0.08 0.09 0.13];
cD05  = [0.10 0.42 0.78];
cD1   = [0.95 0.45 0.12];

%% ------------------------------------------------------------------------
% Load + align data
% -------------------------------------------------------------------------

K4D0   = loadRunPositionRise(fileK4D0,   smoothWindow, riseFraction, ...
    holdTime_s, preWindow_s, postWindow_s);

K8D0   = loadRunPositionRise(fileK8D0,   smoothWindow, riseFraction, ...
    holdTime_s, preWindow_s, postWindow_s);

K12D0  = loadRunPositionRise(fileK12D0,  smoothWindow, riseFraction, ...
    holdTime_s, preWindow_s, postWindow_s);

K12D05 = loadRunPositionRise(fileK12D05, smoothWindow, riseFraction, ...
    holdTime_s, preWindow_s, postWindow_s);

K12D1  = loadRunPositionRise(fileK12D1,  smoothWindow, riseFraction, ...
    holdTime_s, preWindow_s, postWindow_s);

%% ------------------------------------------------------------------------
% Diagnostics
% -------------------------------------------------------------------------

fprintf('\n============================================================\n');
fprintf('POSITION-RISE ALIGNMENT\n');
fprintf('t = 0 = first sustained %.0f%% position-recovery crossing\n', ...
    100*riseFraction);
fprintf('============================================================\n');

printAnchor('K=4, D=0',    K4D0);
printAnchor('K=8, D=0',    K8D0);
printAnchor('K=12, D=0',   K12D0);
printAnchor('K=12, D=0.5', K12D05);
printAnchor('K=12, D=1',   K12D1);

%% ------------------------------------------------------------------------
% Figure 1: STIFFNESS comparison, D = 0
% Only the two panels currently used in the presentation:
%   1) Hand-push input
%   2) Position response
% -------------------------------------------------------------------------

runsK   = {K4D0,K8D0,K12D0};
labelsK = {'K = 4, D = 0','K = 8, D = 0', ...
           'K = 12, D = 0 (baseline)'};
colorsK = {cK4,cK8,cBase};
widthsK = [1.8 1.8 2.8];

figK = plotTwoPanelComparison( ...
    runsK,labelsK,colorsK,widthsK, ...
    'GIMP Stiffness Comparison (D = 0)',plotWindowK);

exportgraphics(figK,fullfile(outputDir, ...
    'gimp_stiffness_position_rise_aligned.png'),'Resolution',300);

exportgraphics(figK,fullfile(outputDir, ...
    'gimp_stiffness_position_rise_aligned.pdf'),'ContentType','vector');

%% ------------------------------------------------------------------------
% Figure 2: DAMPING comparison, K = 12
% -------------------------------------------------------------------------

runsD   = {K12D0,K12D05,K12D1};
labelsD = {'K = 12, D = 0 (baseline)','K = 12, D = 0.5', ...
           'K = 12, D = 1'};
colorsD = {cBase,cD05,cD1};
widthsD = [2.8 1.8 1.8];

figD = plotTwoPanelComparison( ...
    runsD,labelsD,colorsD,widthsD, ...
    'GIMP Damping Comparison (K = 12)',plotWindowD);

exportgraphics(figD,fullfile(outputDir, ...
    'gimp_damping_position_rise_aligned.png'),'Resolution',300);

exportgraphics(figD,fullfile(outputDir, ...
    'gimp_damping_position_rise_aligned.pdf'),'ContentType','vector');

%% ------------------------------------------------------------------------
% Optional: position-only PPT figures
% -------------------------------------------------------------------------

figKpos = plotPositionOnly( ...
    runsK,labelsK,colorsK,widthsK, ...
    'Stiffness Verification',plotWindowK);

exportgraphics(figKpos,fullfile(outputDir, ...
    'gimp_stiffness_position_only_aligned.png'),'Resolution',300);

figDpos = plotPositionOnly( ...
    runsD,labelsD,colorsD,widthsD, ...
    'Damping Verification',plotWindowD);

exportgraphics(figDpos,fullfile(outputDir, ...
    'gimp_damping_position_only_aligned.png'),'Resolution',300);

%% ========================================================================
% Local functions
% ========================================================================

function S = loadRunPositionRise(filePath,smoothWindow,riseFraction, ...
    holdTime_s,preWindow_s,postWindow_s)

    assert(isfile(filePath),'File not found: %s',filePath);

    T = readtable(filePath,'VariableNamingRule','preserve');

    required = {'Time_ms','Position_deg','External_Torque_For_Gimp_Nm', ...
        'Tracking_Error_deg','Command_Torque_Nm'};

    assert(all(ismember(required,T.Properties.VariableNames)), ...
        'Required columns are missing in %s',filePath);

    time   = T.Time_ms(:)*1e-3;
    q      = T.Position_deg(:);
    tauExt = T.External_Torque_For_Gimp_Nm(:);
    err    = T.Tracking_Error_deg(:);
    tauCmd = T.Command_Torque_Nm(:);

    valid = isfinite(time) & isfinite(q) & isfinite(tauExt) & ...
        isfinite(err) & isfinite(tauCmd);

    if ismember('Feedback_Valid',T.Properties.VariableNames)
        valid = valid & T.Feedback_Valid(:)==1;
    end

    if ismember('Motor_Safe',T.Properties.VariableNames)
        valid = valid & T.Motor_Safe(:)==1;
    end

    time   = time(valid);
    q      = q(valid);
    tauExt = tauExt(valid);
    err    = err(valid);
    tauCmd = tauCmd(valid);

    [time,order] = sort(time);
    q      = q(order);
    tauExt = tauExt(order);
    err    = err(order);
    tauCmd = tauCmd(order);

    [time,idx] = unique(time,'stable');
    q      = q(idx);
    tauExt = tauExt(idx);
    err    = err(idx);
    tauCmd = tauCmd(idx);

    % Zero position using the initial stationary section
    initial = time <= time(1)+0.25;
    if ~any(initial)
        initial = 1:min(20,numel(q));
    end
    q = q - median(q(initial),'omitnan');

    % Smooth for robust event detection / plotting
    qSmooth      = smoothdata(q,'movmean',smoothWindow);
    tauExtSmooth = smoothdata(tauExt,'movmean',smoothWindow);
    errSmooth    = smoothdata(err,'movmean',smoothWindow);
    tauCmdSmooth = smoothdata(tauCmd,'movmean',smoothWindow);

    dt = median(diff(time),'omitnan');
    holdSamples = max(1,round(holdTime_s/dt));

    % ---------------------------------------------------------------------
    % IMPORTANT FIX:
    % Find the LOWEST POSITION first, then search ONLY AFTER that point.
    %
    % The previous version searched around the torque minimum. For D = 0.5
    % the position passed the threshold once while still moving DOWNWARD,
    % so MATLAB picked the wrong event. This caused the blue curve to be
    % shifted to the wrong place.
    % ---------------------------------------------------------------------
    [~,iQmin] = min(qSmooth);
    tQmin = time(iQmin);

    % Estimate the negative pre-release plateau around the position minimum.
    preStart = max(1, iQmin - round(preWindow_s/dt));
    preEnd   = min(numel(qSmooth), iQmin + round(0.05/dt));
    qLow = median(qSmooth(preStart:preEnd),'omitnan');

    % Estimate final steady position from the last 20% of the recording.
    % This is more robust than using a window relative to torque minimum.
    finalStart = max(1,round(0.80*numel(qSmooth)));
    qFinal = median(qSmooth(finalStart:end),'omitnan');

    % Position recovery threshold
    qThreshold = qLow + riseFraction*(qFinal-qLow);

    % Search ONLY AFTER the minimum-position point.
    riseIndex = [];

    for i = iQmin+1 : numel(time)-holdSamples

        if qSmooth(i) >= qThreshold

            segment = qSmooth(i:i+holdSamples-1);

            % Must remain above threshold
            sustained = all(segment >= qThreshold);

            % Must be genuinely moving upward, not just noise on the plateau
            dqSegment = diff(segment);
            upwardFraction = mean(dqSegment >= 0);
            netRise = segment(end)-segment(1);

            upward = upwardFraction >= 0.60 && netRise > 0;

            if sustained && upward
                riseIndex = i;
                break;
            end
        end
    end

    assert(~isempty(riseIndex), ...
        'Could not detect position rise onset in %s',filePath);

    riseTime = time(riseIndex);

    % Final aligned data
    S.time   = time-riseTime;
    S.q      = qSmooth;
    S.tauExt = tauExtSmooth;
    S.error  = errSmooth;
    S.tauCmd = tauCmdSmooth;

    % Diagnostics
    S.file = filePath;
    S.originalRiseTime = riseTime;
    S.positionMinimumTime = tQmin;
    S.qLow = qLow;
    S.qFinal = qFinal;
    S.qThreshold = qThreshold;
end

function printAnchor(label,S)

    fprintf('%-14s rise = %.4f s   q_low = %+7.3f deg   q_final = %+7.3f deg   threshold = %+7.3f deg\n', ...
        label,S.originalRiseTime,S.qLow,S.qFinal,S.qThreshold);
end

function fig = plotTwoPanelComparison(runs,labels,colors,widths,mainTitle,xWindow)

    fig = figure('Color','w','Position',[100 80 1250 680]);
    layout = tiledlayout(2,1,'TileSpacing','compact','Padding','compact');

    title(layout,mainTitle, ...
        'FontName','Arial','FontSize',22,'FontWeight','bold');

    % ---------------------------------------------------------------------
    % External torque
    % ---------------------------------------------------------------------
    ax1 = nexttile;
    hold(ax1,'on');

    for i = 1:numel(runs)
        plot(ax1,runs{i}.time,runs{i}.tauExt, ...
            'Color',colors{i}, ...
            'LineWidth',widths(i), ...
            'DisplayName',labels{i});
    end

    xline(ax1,0,'--','Color',[0.40 0.43 0.49], ...
        'LineWidth',1.2,'HandleVisibility','off');
    yline(ax1,0,'Color',[0.72 0.74 0.78], ...
        'LineWidth',0.9,'HandleVisibility','off');

    xlim(ax1,xWindow);
    grid(ax1,'on');
    box(ax1,'on');

    title(ax1,'Hand-push input','FontSize',18,'FontWeight','bold');
    ylabel(ax1,'External torque [N m]','FontSize',18);

    legend(ax1,'Location','northeast','Box','on','FontSize',16);

    styleAxis(ax1);

    % ---------------------------------------------------------------------
    % Position response
    % ---------------------------------------------------------------------
    ax2 = nexttile;
    hold(ax2,'on');

    for i = 1:numel(runs)
        plot(ax2,runs{i}.time,runs{i}.q, ...
            'Color',colors{i}, ...
            'LineWidth',widths(i), ...
            'DisplayName',labels{i});
    end

    xline(ax2,0,'--','Color',[0.40 0.43 0.49], ...
        'LineWidth',1.2,'HandleVisibility','off');
    yline(ax2,0,'Color',[0.72 0.74 0.78], ...
        'LineWidth',0.9,'HandleVisibility','off');

    xlim(ax2,xWindow);
    grid(ax2,'on');
    box(ax2,'on');

    title(ax2,'Position response','FontSize',18,'FontWeight','bold');
    xlabel(ax2,'Time from position-rise onset [s]','FontSize',18);
    ylabel(ax2,'Position [deg]','FontSize',18);

    styleAxis(ax2);

    linkaxes([ax1 ax2],'x');
end

function fig = plotPositionOnly(runs,labels,colors,widths,mainTitle,xWindow)

    fig = figure('Color','w','Position',[120 120 1150 460]);
    ax = axes(fig);
    hold(ax,'on');

    for i = 1:numel(runs)
        plot(ax,runs{i}.time,runs{i}.q, ...
            'Color',colors{i}, ...
            'LineWidth',widths(i), ...
            'DisplayName',labels{i});
    end

    xline(ax,0,'--','Color',[0.40 0.43 0.49], ...
        'LineWidth',1.2,'HandleVisibility','off');
    yline(ax,0,'Color',[0.72 0.74 0.78], ...
        'LineWidth',0.9,'HandleVisibility','off');

    xlim(ax,xWindow);
    grid(ax,'on');
    box(ax,'on');

    title(ax,mainTitle,'FontSize',20,'FontWeight','bold');
    xlabel(ax,'Time from position-rise onset [s]','FontSize',18);
    ylabel(ax,'Position [deg]','FontSize',18);
    legend(ax,'Location','northeast','Box','on','FontSize',16);

    styleAxis(ax);
end

function styleAxis(ax)

    ax.FontName = 'Arial';
    ax.FontSize = 16;
    ax.LineWidth = 1.1;
    ax.GridAlpha = 0.16;
end
