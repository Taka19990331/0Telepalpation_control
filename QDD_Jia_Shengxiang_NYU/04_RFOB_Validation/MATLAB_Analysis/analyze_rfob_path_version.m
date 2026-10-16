%% RFOB accuracy validation vs external sensor Mz
% Path-name version: edit ONLY the two full file paths below.
%
% Motor CSV:
%   motor_data_*.csv
%
% External 6-axis sensor CSV:
%   *_bbbbb.csv
%
% Main comparison:
%   External sensor Mz  vs  RFOB_Estimated_Nm
%
% This script also checks the inertia term J*qddot because it can dominate
% the RFOB during fast manual motion.
%
% No Signal Processing Toolbox required.

clear; close all; clc;

%% ========================================================================
% 1. FILE PATHS -- EDIT THESE TWO LINES
% ========================================================================

motorFile = 'C:\Users\123\Desktop\data\data7\motor_data_20260824_144616.csv';
externalFile = 'F:\20260824_144753_bbbbb.csv';

% Folder for exported figures
outputDir = fileparts(motorFile);

assert(isfile(motorFile), ...
    'Motor CSV not found:\n%s\n\nPlease edit motorFile.', motorFile);

assert(isfile(externalFile), ...
    'External sensor CSV not found:\n%s\n\nPlease edit externalFile.', externalFile);


%% ========================================================================
% 2. SETTINGS
% ========================================================================

% ID2 nominal inertia used in the Teensy RFOB
J_NOM = 0.0116809040;          % kg*m^2

% Initial stationary period used for zero-bias estimation
biasWindow_s = 2.0;

% Light smoothing only for plotting / cross-recording alignment
smoothWindow_s = 0.05;         % 50 ms

% Time alignment search between two independently started recordings
alignmentFs_Hz = 100;
shiftMin_s = -25;
shiftMax_s =  25;
shiftStep_s = 0.01;

% Quantitative active-motion threshold
extActiveThreshold_Nm  = 0.03;
rfobActiveThreshold_Nm = 0.05;

% Quasi-static acceleration thresholds
quasiStaticAccel1 = 2.0;       % rad/s^2
quasiStaticAccel2 = 5.0;       % rad/s^2

% Plot x-range after alignment.
% [] = full overlap
plotRange_s = [];

EXPORT_FIGURES = true;


%% ========================================================================
% 3. LOAD MOTOR / RFOB CSV
% ========================================================================

M = readtable(motorFile, 'VariableNamingRule','preserve');

requiredCols = { ...
    'Time_ms', ...
    'Position_rad', ...
    'Measured_Torque_Nm', ...
    'Torque_Bias_Nm', ...
    'Torque_Zeroed_Nm', ...
    'Acceleration_rad_s2', ...
    'Observer_Friction_Nm', ...
    'RFOB_Raw_Nm', ...
    'RFOB_Estimated_Nm', ...
    'RFOB_Control_Nm', ...
    'Friction_Target_Nm', ...
    'Friction_Comp_Nm', ...
    'Torque_Command_Nm'};

missingCols = requiredCols(~ismember(requiredCols, M.Properties.VariableNames));

if ~isempty(missingCols)
    error('Motor CSV is missing column(s):\n%s', strjoin(missingCols, ', '));
end

tM = (M.Time_ms(:) - M.Time_ms(1))*1e-3;

qM        = M.Position_rad(:);
tauMeas   = M.Measured_Torque_Nm(:);
tauBias   = M.Torque_Bias_Nm(:);
tauZero   = M.Torque_Zeroed_Nm(:);
accelM    = M.Acceleration_rad_s2(:);
tauFric   = M.Observer_Friction_Nm(:);
rfobRaw   = M.RFOB_Raw_Nm(:);
rfobEst   = M.RFOB_Estimated_Nm(:);

rfobCtrl  = M.RFOB_Control_Nm(:);
fricTgt   = M.Friction_Target_Nm(:);
fricComp  = M.Friction_Comp_Nm(:);
tauCmd    = M.Torque_Command_Nm(:);

if ismember('Control_Enabled',M.Properties.VariableNames)
    controlEnabled = M.Control_Enabled(:);
else
    controlEnabled = nan(size(tM));
end

if ismember('FrictionFF_Enabled',M.Properties.VariableNames)
    frictionFFEnabled = M.FrictionFF_Enabled(:);
else
    frictionFFEnabled = nan(size(tM));
end

if ismember('RFOB_Enabled',M.Properties.VariableNames)
    rfobEnabled = M.RFOB_Enabled(:);
else
    rfobEnabled = nan(size(tM));
end

if ismember('Error_Code',M.Properties.VariableNames)
    errorCode = M.Error_Code(:);
else
    errorCode = zeros(size(tM));
end

fsM = 1/median(diff(tM),'omitnan');

fprintf('\n============================================================\n');
fprintf('MOTOR FILE\n');
fprintf('============================================================\n');
fprintf('%s\n',motorFile);
fprintf('Samples             : %d\n',numel(tM));
fprintf('Duration            : %.3f s\n',tM(end));
fprintf('Estimated sample Hz : %.2f Hz\n',fsM);
fprintf('Position range      : %.4f to %.4f rad\n', ...
    min(qM,[],'omitnan'), max(qM,[],'omitnan'));
fprintf('RFOB estimate range : %.4f to %.4f Nm\n', ...
    min(rfobEst,[],'omitnan'), max(rfobEst,[],'omitnan'));
fprintf('Max |Torque Command|: %.6f Nm\n',max(abs(tauCmd),[],'omitnan'));
fprintf('Max |RFOB Control|  : %.6f Nm\n',max(abs(rfobCtrl),[],'omitnan'));
fprintf('Max |Friction Comp| : %.6f Nm\n',max(abs(fricComp),[],'omitnan'));

if max(abs(tauCmd),[],'omitnan') > 1e-4
    warning('Torque_Command_Nm is NOT zero. This is not a pure observer-only test.');
end

if max(abs(rfobCtrl),[],'omitnan') > 1e-4
    warning('RFOB_Control_Nm is NOT zero.');
end

if max(abs(fricComp),[],'omitnan') > 1e-4
    warning('Friction_Comp_Nm is NOT zero.');
end


%% ========================================================================
% 4. LOAD EXTERNAL 6-AXIS SENSOR CSV
%
% Expected file:
%   line 1-4 = metadata/header
%   numeric data afterward:
%   time, Fx, Fy, Fz, Mx, My, Mz
%
% Shift_JIS is used because the logger may contain Japanese text.
% ========================================================================

fid = fopen(externalFile,'r','n','Shift_JIS');

if fid < 0
    error('Could not open external sensor file:\n%s',externalFile);
end

cleanupObj = onCleanup(@() fclose(fid));

for k = 1:4
    line = fgetl(fid);
    if ~ischar(line)
        error('External sensor file ended before numeric data.');
    end
end

C = textscan(fid, ...
    '%f%f%f%f%f%f%f', ...
    'Delimiter',',', ...
    'CollectOutput',true, ...
    'ReturnOnError',false);

E = C{1};

clear cleanupObj;

if isempty(E) || size(E,2) < 7
    error(['Could not parse external sensor numeric data.\n' ...
           'Expected 7 columns: time,Fx,Fy,Fz,Mx,My,Mz']);
end

tE = E(:,1);
Mz = E(:,7);

% If external logger time does not begin at zero, make it relative
tE = tE - tE(1);

fsE = 1/median(diff(tE),'omitnan');

fprintf('\n============================================================\n');
fprintf('EXTERNAL SENSOR FILE\n');
fprintf('============================================================\n');
fprintf('%s\n',externalFile);
fprintf('Samples             : %d\n',numel(tE));
fprintf('Duration            : %.3f s\n',tE(end));
fprintf('Estimated sample Hz : %.2f Hz\n',fsE);


%% ========================================================================
% 5. REMOVE INITIAL BIAS
% ========================================================================

idxBiasM = tM <= biasWindow_s;
idxBiasE = tE <= biasWindow_s;

if nnz(idxBiasM) < 5
    idxBiasM = 1:min(100,numel(tM));
end

if nnz(idxBiasE) < 5
    idxBiasE = 1:min(100,numel(tE));
end

rfobOffset = median(rfobEst(idxBiasM),'omitnan');
extOffset  = median(Mz(idxBiasE),'omitnan');

rfobZero = rfobEst - rfobOffset;
extZero  = Mz - extOffset;

fprintf('\n============================================================\n');
fprintf('ZERO BIAS\n');
fprintf('============================================================\n');
fprintf('RFOB offset        : %+0.6f Nm\n',rfobOffset);
fprintf('External Mz offset : %+0.6f Nm\n',extOffset);


%% ========================================================================
% 6. LIGHT SMOOTHING FOR ALIGNMENT
% ========================================================================

nM = max(1,round(smoothWindow_s*fsM));
nE = max(1,round(smoothWindow_s*fsE));

rfobSmooth = movmean(rfobZero,nM);
extSmooth  = movmean(extZero,nE);


%% ========================================================================
% 7. AUTOMATIC RECORDING-TIME ALIGNMENT
%
% Search shift such that:
%
%       external(t) ~= RFOB(t + shift)
%
% The resulting shift is mainly recording-start offset.
% DO NOT report this directly as observer delay.
% ========================================================================

shiftList = shiftMin_s:shiftStep_s:shiftMax_s;
corrList = nan(size(shiftList));

for k = 1:numel(shiftList)

    sh = shiftList(k);

    tStart = max(tE(1), tM(1)-sh);
    tEnd   = min(tE(end), tM(end)-sh);

    if (tEnd-tStart) < 5
        continue;
    end

    tc = (tStart:1/alignmentFs_Hz:tEnd)';

    x = interp1(tE,extSmooth,tc,'linear',nan);
    y = interp1(tM,rfobSmooth,tc+sh,'linear',nan);

    good = isfinite(x) & isfinite(y);

    if nnz(good) < 200
        continue;
    end

    x = x(good);
    y = y(good);

    x = x - mean(x);
    y = y - mean(y);

    den = sqrt(sum(x.^2)*sum(y.^2));

    if den > eps
        corrList(k) = sum(x.*y)/den;
    end
end

if all(~isfinite(corrList))
    error('Automatic time alignment failed.');
end

[~,bestIdx] = max(abs(corrList));
bestShift = shiftList(bestIdx);
initialCorr = corrList(bestIdx);

% Automatically account for opposite sensor sign convention
if initialCorr < 0
    signFactor = -1;
else
    signFactor = 1;
end

extSmooth = signFactor*extSmooth;
extZero   = signFactor*extZero;

fprintf('\n============================================================\n');
fprintf('TIME ALIGNMENT\n');
fprintf('============================================================\n');
fprintf('Best recording offset : %+0.3f s\n',bestShift);
fprintf('Correlation at search : %+0.4f\n',initialCorr);
fprintf('External sign factor  : %+d\n',signFactor);
fprintf('NOTE: recording offset is NOT the RFOB observer delay.\n');


%% ========================================================================
% 8. COMMON ALIGNED GRID
% ========================================================================

tStart = max(tE(1), tM(1)-bestShift);
tEnd   = min(tE(end), tM(end)-bestShift);

t = (tStart:1/alignmentFs_Hz:tEnd)';

extA  = interp1(tE,extSmooth,t,'linear',nan);
rfobA = interp1(tM,rfobSmooth,t+bestShift,'linear',nan);

accelA   = interp1(tM,accelM,t+bestShift,'linear',nan);
tauMeasA = interp1(tM,tauMeas,t+bestShift,'linear',nan);
tauZeroA = interp1(tM,tauZero,t+bestShift,'linear',nan);
tauFricA = interp1(tM,tauFric,t+bestShift,'linear',nan);
rfobRawA = interp1(tM,rfobRaw,t+bestShift,'linear',nan);
qA       = interp1(tM,qM,t+bestShift,'linear',nan);

tauInertiaA = J_NOM*accelA;


%% ========================================================================
% 9. ACTIVE MOTION REGION
% ========================================================================

valid = isfinite(extA) & isfinite(rfobA);

active = valid & ...
    (abs(extA) >= extActiveThreshold_Nm | ...
     abs(rfobA) >= rfobActiveThreshold_Nm);

if nnz(active) < 100
    warning('Active threshold selected too few points. Using all valid samples.');
    active = valid;
end


%% ========================================================================
% 10. METRICS
% ========================================================================

calcMetrics = @(mask) localMetrics(extA(mask),rfobA(mask));

allMetrics = calcMetrics(active);

maskQS2 = active & isfinite(accelA) & abs(accelA) < quasiStaticAccel1;
maskQS5 = active & isfinite(accelA) & abs(accelA) < quasiStaticAccel2;

mQS2 = calcMetrics(maskQS2);
mQS5 = calcMetrics(maskQS5);

% RFOB error vs inertia term
err = rfobA - extA;
diagMask = active & isfinite(err) & isfinite(tauInertiaA);

err0 = err(diagMask) - mean(err(diagMask));
iner0 = tauInertiaA(diagMask) - mean(tauInertiaA(diagMask));

corrErrInertia = sum(err0.*iner0) / ...
    sqrt(sum(err0.^2)*sum(iner0.^2));

rmsMotor   = sqrt(mean(tauMeasA(diagMask).^2));
rmsZero    = sqrt(mean(tauZeroA(diagMask).^2));
rmsInertia = sqrt(mean(tauInertiaA(diagMask).^2));
rmsFric    = sqrt(mean(tauFricA(diagMask).^2));


fprintf('\n============================================================\n');
fprintf('RFOB ACCURACY - ACTIVE MOTION\n');
fprintf('============================================================\n');
printMetrics('All active motion',allMetrics);

fprintf('\n');
printMetrics(sprintf('|acceleration| < %.1f rad/s^2',quasiStaticAccel1),mQS2);

fprintf('\n');
printMetrics(sprintf('|acceleration| < %.1f rad/s^2',quasiStaticAccel2),mQS5);

fprintf('\n============================================================\n');
fprintf('OBSERVER TERM DIAGNOSTICS\n');
fprintf('============================================================\n');
fprintf('RMS measured motor torque : %.4f Nm\n',rmsMotor);
fprintf('RMS zeroed motor torque   : %.4f Nm\n',rmsZero);
fprintf('RMS J*qddot               : %.4f Nm\n',rmsInertia);
fprintf('RMS observer friction     : %.4f Nm\n',rmsFric);
fprintf('Corr(error, J*qddot)      : %.4f\n',corrErrInertia);


%% ========================================================================
% 11. FIGURE 1 - RFOB VS EXTERNAL SENSOR
% ========================================================================

fig1 = figure('Color','w','Position',[80 80 1300 650]);
ax1 = axes('Parent',fig1);
hold(ax1,'on');

plot(ax1,t,extA,'LineWidth',1.8, ...
    'DisplayName','External sensor M_z');

plot(ax1,t,rfobA,'LineWidth',1.8, ...
    'DisplayName','RFOB estimate');

yline(ax1,0,'HandleVisibility','off');

xlabel(ax1,'Aligned time [s]');
ylabel(ax1,'Torque [N m]');

title(ax1,sprintf( ...
    'RFOB Validation: r = %.2f, RMS ratio = %.2f', ...
    allMetrics.corr,allMetrics.rmsRatio));

legend(ax1,'Location','best');
grid(ax1,'on');
box(ax1,'on');
formatAxis(ax1);

if ~isempty(plotRange_s)
    xlim(ax1,plotRange_s);
end


%% ========================================================================
% 12. FIGURE 2 - NORMALIZED TREND
% ========================================================================

fig2 = figure('Color','w','Position',[100 100 1300 580]);
ax2 = axes('Parent',fig2);
hold(ax2,'on');

maxExt = max(abs(extA),[],'omitnan');
maxRFOB = max(abs(rfobA),[],'omitnan');

extNorm = extA/max(maxExt,eps);
rfobNorm = rfobA/max(maxRFOB,eps);

plot(ax2,t,extNorm,'LineWidth',1.8, ...
    'DisplayName','External M_z normalized');

plot(ax2,t,rfobNorm,'LineWidth',1.8, ...
    'DisplayName','RFOB normalized');

yline(ax2,0,'HandleVisibility','off');

xlabel(ax2,'Aligned time [s]');
ylabel(ax2,'Normalized torque');
title(ax2,sprintf('RFOB Trend Tracking   |   r = %.2f',allMetrics.corr));

legend(ax2,'Location','best');
grid(ax2,'on');
box(ax2,'on');
formatAxis(ax2);

if ~isempty(plotRange_s)
    xlim(ax2,plotRange_s);
end


%% ========================================================================
% 13. FIGURE 3 - INERTIA / ACCELERATION DIAGNOSTICS
% ========================================================================

fig3 = figure('Color','w','Position',[120 50 1300 850]);

tl = tiledlayout(fig3,3,1, ...
    'TileSpacing','compact', ...
    'Padding','compact');

title(tl,'RFOB Dynamic Error Diagnostics', ...
    'FontSize',21,'FontWeight','bold');

ax31 = nexttile(tl);
hold(ax31,'on');

plot(ax31,t,tauMeasA,'LineWidth',1.5, ...
    'DisplayName','Measured motor torque');

plot(ax31,t,tauInertiaA,'LineWidth',1.5, ...
    'DisplayName','J qddot');

plot(ax31,t,tauFricA,'LineWidth',1.5, ...
    'DisplayName','Observer friction');

ylabel(ax31,'Torque [N m]');
legend(ax31,'Location','best');
grid(ax31,'on');
box(ax31,'on');
formatAxis(ax31);

ax32 = nexttile(tl);
hold(ax32,'on');

plot(ax32,t,extA,'LineWidth',1.5, ...
    'DisplayName','External M_z');

plot(ax32,t,rfobA,'LineWidth',1.5, ...
    'DisplayName','RFOB estimate');

ylabel(ax32,'Torque [N m]');
legend(ax32,'Location','best');
grid(ax32,'on');
box(ax32,'on');
formatAxis(ax32);

ax33 = nexttile(tl);
hold(ax33,'on');

plot(ax33,t,err,'LineWidth',1.5, ...
    'DisplayName','RFOB - external');

plot(ax33,t,tauInertiaA,'LineWidth',1.5, ...
    'DisplayName','J qddot');

xlabel(ax33,'Aligned time [s]');
ylabel(ax33,'Torque [N m]');
legend(ax33,'Location','best');
grid(ax33,'on');
box(ax33,'on');
formatAxis(ax33);

linkaxes([ax31 ax32 ax33],'x');

if ~isempty(plotRange_s)
    xlim(ax31,plotRange_s);
end


%% ========================================================================
% 14. FIGURE 4 - QUASI-STATIC ACCURACY
% ========================================================================

fig4 = figure('Color','w','Position',[140 100 1300 580]);
ax4 = axes('Parent',fig4);
hold(ax4,'on');

plot(ax4,t,extA,'LineWidth',1.7, ...
    'DisplayName','External M_z');

plot(ax4,t,rfobA,'LineWidth',1.7, ...
    'DisplayName','RFOB estimate');

% Highlight samples with low acceleration
yLimTemp = [ ...
    min([extA;rfobA],[],'omitnan'), ...
    max([extA;rfobA],[],'omitnan')];

qsIntervals = logicalToIntervals(maskQS2,t);

for k = 1:size(qsIntervals,1)
    patch(ax4, ...
        [qsIntervals(k,1) qsIntervals(k,2) qsIntervals(k,2) qsIntervals(k,1)], ...
        [yLimTemp(1) yLimTemp(1) yLimTemp(2) yLimTemp(2)], ...
        [0.8 0.8 0.8], ...
        'FaceAlpha',0.12, ...
        'EdgeColor','none', ...
        'HandleVisibility','off');
end

% Bring lines to front
ch = ax4.Children;
lineMask = arrayfun(@(h) strcmp(h.Type,'line'),ch);
uistack(ch(lineMask),'top');

xlabel(ax4,'Aligned time [s]');
ylabel(ax4,'Torque [N m]');

title(ax4,sprintf( ...
    'Quasi-static regions: |qddot| < %.1f rad/s^2, RMS ratio = %.2f', ...
    quasiStaticAccel1,mQS2.rmsRatio));

legend(ax4,'Location','best');
grid(ax4,'on');
box(ax4,'on');
formatAxis(ax4);

if ~isempty(plotRange_s)
    xlim(ax4,plotRange_s);
end


%% ========================================================================
% 15. EXPORT
% ========================================================================

if EXPORT_FIGURES
    exportgraphics(fig1,fullfile(outputDir,'01_RFOB_vs_external.png'), ...
        'Resolution',300);

    exportgraphics(fig2,fullfile(outputDir,'02_RFOB_normalized_trend.png'), ...
        'Resolution',300);

    exportgraphics(fig3,fullfile(outputDir,'03_RFOB_inertia_diagnostics.png'), ...
        'Resolution',300);

    exportgraphics(fig4,fullfile(outputDir,'04_RFOB_quasistatic_accuracy.png'), ...
        'Resolution',300);

    fprintf('\nFigures exported to:\n%s\n',outputDir);
end


%% ========================================================================
% LOCAL FUNCTIONS
% ========================================================================

function m = localMetrics(x,y)

    good = isfinite(x) & isfinite(y);
    x = x(good);
    y = y(good);

    m.N = numel(x);

    if m.N < 5
        m.corr = NaN;
        m.rmsExternal = NaN;
        m.rmsRFOB = NaN;
        m.rmsRatio = NaN;
        m.rmse = NaN;
        return;
    end

    x0 = x-mean(x);
    y0 = y-mean(y);

    den = sqrt(sum(x0.^2)*sum(y0.^2));

    if den > eps
        m.corr = sum(x0.*y0)/den;
    else
        m.corr = NaN;
    end

    m.rmsExternal = sqrt(mean(x.^2));
    m.rmsRFOB = sqrt(mean(y.^2));

    if m.rmsExternal > eps
        m.rmsRatio = m.rmsRFOB/m.rmsExternal;
    else
        m.rmsRatio = NaN;
    end

    m.rmse = sqrt(mean((y-x).^2));
end


function printMetrics(name,m)

    fprintf('%s\n',name);
    fprintf('  Samples            : %d\n',m.N);
    fprintf('  Correlation r      : %.4f\n',m.corr);
    fprintf('  External RMS       : %.4f Nm\n',m.rmsExternal);
    fprintf('  RFOB RMS           : %.4f Nm\n',m.rmsRFOB);
    fprintf('  RFOB / External    : %.4f x\n',m.rmsRatio);
    fprintf('  RMSE               : %.4f Nm\n',m.rmse);
end


function formatAxis(ax)

    ax.FontName = 'Arial';
    ax.FontSize = 16;
    ax.LineWidth = 1.0;

    ax.XLabel.FontSize = 18;
    ax.YLabel.FontSize = 18;

    ax.GridAlpha = 0.20;
end


function intervals = logicalToIntervals(mask,t)

    mask = mask(:);
    t = t(:);

    if isempty(mask)
        intervals = zeros(0,2);
        return;
    end

    d = diff([false;mask;false]);

    iStart = find(d==1);
    iEnd   = find(d==-1)-1;

    intervals = [t(iStart) t(iEnd)];
end
