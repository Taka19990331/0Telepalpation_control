clear; clc; close all;

%% ============================================================
% AK60 Stribeck Friction Identification
%
% Same experiment/data as the previous script, but the low-speed
% region is now INCLUDED in the fit so that the Stribeck drop is
% visible.
%
% Positive:
%   tau_f = Fc_p + (Ts_p-Fc_p)*exp(-(w/Vs_p)^2) + B_p*w
%
% Negative:
%   tau_f = -[Fc_n + (Ts_n-Fc_n)*exp(-(|w|/Vs_n)^2)] + B_n*w
%
% Ts is identified independently from breakaway torque.
% Fc, B and Vs are fitted with a pure-MATLAB Vs grid search + least squares.
%% ============================================================

%% Load CSV
csvFile = 'C:\Users\123\Desktop\data\data7\ak60_friction_20260818_154344.csv';
data = readtable(csvFile,"VariableNamingRule","preserve");

%% Motor parameters
Kt = 0.5994;          % Nm/A
J  = 0.0042243000;    % kg*m^2

%% Signal processing / identification parameters
fc = 15.0;                  % speed LPF [Hz]
BREAKAWAY_SPEED = 0.30;     % breakaway detection [rad/s]
STATIC_SPEED_MAX = 0.10;    % approximately static [rad/s]
BREAKAWAY_LOOKBACK_SEC = 0.30;

% IMPORTANT: this must be far below 0.30 rad/s, otherwise the
% Stribeck region gets thrown away again.
STRIBECK_MIN_SPEED = 0.02;  % moving-side fit threshold [rad/s]
STRIBECK_MAX_SPEED = Inf;
VS_INITIAL_P = 0.15;        % initial Stribeck velocity [rad/s]
VS_INITIAL_N = 0.15;

%% Select active torque-ramp samples
idxRamp = data.("Phase") == 2 ...
    & isfinite(data.("Time_ms")) ...
    & isfinite(data.("Measured_Speed_rad_s")) ...
    & isfinite(data.("Measured_Iq_A")) ...
    & isfinite(data.("Torque_Ramp_Slope_Nm_per_s"));

time_ms = double(data.("Time_ms")(idxRamp));
w_raw = double(data.("Measured_Speed_rad_s")(idxRamp));
Iq = double(data.("Measured_Iq_A")(idxRamp));
slopeIndex = double(data.("Slope_Index")(idxRamp));
rampSlope = double(data.("Torque_Ramp_Slope_Nm_per_s")(idxRamp));
repeatIndex = double(data.("Repeat_Index")(idxRamp));

tau_motor = Kt .* Iq;

%% Trial IDs
trialID = slopeIndex*100 + repeatIndex;
uniqueTrials = unique(trialID);

fprintf('\n========================================\n');
fprintf('AK60 STRIBECK FRICTION IDENTIFICATION\n');
fprintf('========================================\n');
fprintf('Ramp samples     = %d\n',length(w_raw));
fprintf('Number of trials = %d\n',length(uniqueTrials));

%% Filter speed and calculate acceleration, trial by trial
w_filtered = nan(size(w_raw));
acceleration = nan(size(w_raw));

for i = 1:length(uniqueTrials)
    idxTrial = trialID == uniqueTrials(i);
    wTrial = w_raw(idxTrial);
    tTrial = time_ms(idxTrial)*1e-3;
    N = length(wTrial);
    if N < 3, continue; end

    dtVec = diff(tTrial);
    dtVec = dtVec(isfinite(dtVec) & dtVec > 0);
    if isempty(dtVec), continue; end
    dtMean = median(dtVec);

    alphaLPF = (2*pi*fc*dtMean)/(1 + 2*pi*fc*dtMean);
    wFilt = zeros(size(wTrial));
    wFilt(1) = wTrial(1);
    for k = 2:N
        wFilt(k) = wFilt(k-1) + alphaLPF*(wTrial(k)-wFilt(k-1));
    end

    aTrial = zeros(size(wFilt));
    dtLocal = tTrial(2)-tTrial(1);
    if dtLocal > 0
        aTrial(1) = (wFilt(2)-wFilt(1))/dtLocal;
    end
    for k = 2:N-1
        dtLocal = tTrial(k+1)-tTrial(k-1);
        if dtLocal > 0
            aTrial(k) = (wFilt(k+1)-wFilt(k-1))/dtLocal;
        end
    end
    dtLocal = tTrial(N)-tTrial(N-1);
    if dtLocal > 0
        aTrial(N) = (wFilt(N)-wFilt(N-1))/dtLocal;
    end

    w_filtered(idxTrial) = wFilt;
    acceleration(idxTrial) = aTrial;
end

%% Friction residual
% tau_f = Kt*Iq - J*alpha
tau_friction = tau_motor - J.*acceleration;

%% Static / breakaway friction identification
breakawayTorque = [];
breakawayDirection = [];
breakawaySlope = [];
breakawaySlopeIndex = [];
breakawayRepeat = [];

for i = 1:length(uniqueTrials)
    idxG = find(trialID == uniqueTrials(i));
    if isempty(idxG), continue; end

    wTrial = w_filtered(idxG);
    tauTrial = tau_motor(idxG);
    tTrial = time_ms(idxG)*1e-3;
    rampSlopeTrial = rampSlope(idxG(1));
    slopeIndexTrial = slopeIndex(idxG(1));
    repeatTrial = repeatIndex(idxG(1));

    if rampSlopeTrial > 0
        kBreak = find(wTrial >= BREAKAWAY_SPEED,1,"first");
    elseif rampSlopeTrial < 0
        kBreak = find(wTrial <= -BREAKAWAY_SPEED,1,"first");
    else
        continue;
    end
    if isempty(kBreak), continue; end

    breakTime = tTrial(kBreak);
    preStart = breakTime - BREAKAWAY_LOOKBACK_SEC;
    idxPre = tTrial >= preStart & tTrial < breakTime ...
        & abs(wTrial) <= STATIC_SPEED_MAX & isfinite(tauTrial);
    if ~any(idxPre)
        idxPre = tTrial >= preStart & tTrial < breakTime & isfinite(tauTrial);
    end
    if ~any(idxPre), continue; end

    tauPre = tauTrial(idxPre);
    if rampSlopeTrial > 0
        TsTrial = max(tauPre);
        dir = 1;
    else
        TsTrial = min(tauPre);
        dir = -1;
    end

    breakawayTorque(end+1,1) = TsTrial;
    breakawayDirection(end+1,1) = dir;
    breakawaySlope(end+1,1) = rampSlopeTrial;
    breakawaySlopeIndex(end+1,1) = slopeIndexTrial;
    breakawayRepeat(end+1,1) = repeatTrial;
end

idxBreakP = breakawayDirection > 0;
idxBreakN = breakawayDirection < 0;
Ts_p = median(breakawayTorque(idxBreakP),"omitnan");
Ts_n = median(abs(breakawayTorque(idxBreakN)),"omitnan");

fprintf('\n========================================\n');
fprintf('STATIC / BREAKAWAY FRICTION\n');
fprintf('========================================\n');
fprintf('Ts_p = %.10f Nm\n',Ts_p);
fprintf('Ts_n = %.10f Nm\n',Ts_n);

%% Stribeck fitting data
% Keep low-speed MOVING samples. This is the main change.
valid = isfinite(w_filtered) & isfinite(tau_friction);
w = w_filtered(valid);
tau = tau_friction(valid);

idxP = w >= STRIBECK_MIN_SPEED & w <= STRIBECK_MAX_SPEED;
idxN = w <= -STRIBECK_MIN_SPEED & abs(w) <= STRIBECK_MAX_SPEED;
wP = w(idxP); tauP = tau(idxP);
wN = w(idxN); tauN = tau(idxN);

fprintf('\n========================================\n');
fprintf('STRIBECK FITTING DATA\n');
fprintf('========================================\n');
fprintf('Min fitted speed = %.4f rad/s\n',STRIBECK_MIN_SPEED);
fprintf('Positive samples = %d\n',length(wP));
fprintf('Negative samples = %d\n',length(wN));

%% Initial guesses from higher-speed linear part
highSpeedThreshold = max(BREAKAWAY_SPEED,0.5);
idxInitP = wP >= highSpeedThreshold;
idxInitN = wN <= -highSpeedThreshold;

if sum(idxInitP) >= 5
    beta = [ones(sum(idxInitP),1),wP(idxInitP)]\tauP(idxInitP);
    Fc0p = max(0.01,beta(1));
    B0p = max(1e-6,beta(2));
else
    Fc0p = 0.7*Ts_p; B0p = 0.001;
end
if sum(idxInitN) >= 5
    beta = [-ones(sum(idxInitN),1),wN(idxInitN)]\tauN(idxInitN);
    Fc0n = max(0.01,beta(1));
    B0n = max(1e-6,beta(2));
else
    Fc0n = 0.7*Ts_n; B0n = 0.001;
end
Fc0p = min(Fc0p,0.95*Ts_p);
Fc0n = min(Fc0n,0.95*Ts_n);

% ============================================================
% PURE BASE-MATLAB STRIBECK FIT
%
% No optimset, no fminsearch, no Optimization Toolbox.
%
% For a fixed Vs, the Stribeck model is linear in Fc and B:
%
% Positive:
%   tau = Ts*e + Fc*(1-e) + B*w
%
% Negative:
%   tau = -Ts*e - Fc*(1-e) + B*w
%
% where:
%   e = exp(-(abs(w)/Vs)^2)
%
% Therefore we sweep Vs, solve Fc and B by least squares,
% and keep the physically valid solution with minimum SSE.
% ============================================================

VS_SEARCH_MIN = 0.005;   % rad/s
VS_SEARCH_MAX = 2.0;     % rad/s
VS_GRID_N = 1200;

fprintf('\n========================================\n');
fprintf('PURE MATLAB STRIBECK GRID SEARCH\n');
fprintf('========================================\n');
fprintf('Vs range = %.5f ... %.5f rad/s\n',VS_SEARCH_MIN,VS_SEARCH_MAX);
fprintf('Grid N   = %d\n',VS_GRID_N);

[Fc_p,B_p,Vs_p,tauPredP,sseP] = ...
    fitStribeckGridPositive( ...
        wP,tauP,Ts_p, ...
        VS_SEARCH_MIN,VS_SEARCH_MAX,VS_GRID_N);

[Fc_n,B_n,Vs_n,tauPredN,sseN] = ...
    fitStribeckGridNegative( ...
        wN,tauN,Ts_n, ...
        VS_SEARCH_MIN,VS_SEARCH_MAX,VS_GRID_N);

fprintf('Positive best SSE = %.10e\n',sseP);
fprintf('Negative best SSE = %.10e\n',sseN);

r2P = calcR2(tauP,tauPredP);
r2N = calcR2(tauN,tauPredN);
rmseP = sqrt(mean((tauP-tauPredP).^2));
rmseN = sqrt(mean((tauN-tauPredN).^2));

fprintf('\n========================================\n');
fprintf('POSITIVE STRIBECK FIT\n');
fprintf('========================================\n');
fprintf('Ts_p   = %.10f Nm\n',Ts_p);
fprintf('Fc_p   = %.10f Nm\n',Fc_p);
fprintf('Vs_p   = %.10f rad/s\n',Vs_p);
fprintf('B_p    = %.10e Nm*s/rad\n',B_p);
fprintf('R2_p   = %.6f\n',r2P);
fprintf('RMSE_p = %.10f Nm\n',rmseP);

fprintf('\n========================================\n');
fprintf('NEGATIVE STRIBECK FIT\n');
fprintf('========================================\n');
fprintf('Ts_n   = %.10f Nm\n',Ts_n);
fprintf('Fc_n   = %.10f Nm\n',Fc_n);
fprintf('Vs_n   = %.10f rad/s\n',Vs_n);
fprintf('B_n    = %.10e Nm*s/rad\n',B_n);
fprintf('R2_n   = %.6f\n',r2N);
fprintf('RMSE_n = %.10f Nm\n',rmseN);

fprintf('\n========================================\n');
fprintf('ARDUINO CONSTANTS\n');
fprintf('========================================\n');
fprintf('const float TS_P_NM = %.10ef;\n',Ts_p);
fprintf('const float FC_P_NM = %.10ef;\n',Fc_p);
fprintf('const float VS_P_RAD_S = %.10ef;\n',Vs_p);
fprintf('const float B_P_NMS_RAD = %.10ef;\n',B_p);
fprintf('\n');
fprintf('const float TS_N_NM = %.10ef;\n',Ts_n);
fprintf('const float FC_N_NM = %.10ef;\n',Fc_n);
fprintf('const float VS_N_RAD_S = %.10ef;\n',Vs_n);
fprintf('const float B_N_NMS_RAD = %.10ef;\n',B_n);

%% Fitted curves
wMax = max(abs(w));
wCurveP = linspace(1e-4,wMax,1500);
wCurveN = linspace(-wMax,-1e-4,1500);
tauCurveP = stribeckPos(wCurveP,Ts_p,Fc_p,B_p,Vs_p);
tauCurveN = stribeckNeg(wCurveN,Ts_n,Fc_n,B_n,Vs_n);

%% Figure 1: full friction map
figure('Color','w','Position',[120 80 1100 760]);
hold on; grid on; box on;
scatter(w,tau,8,'o');
plot(wCurveP,tauCurveP,'LineWidth',2.5);
plot(wCurveN,tauCurveN,'LineWidth',2.5);
yline(Ts_p,'--','Ts+','LineWidth',1.3);
yline(-Ts_n,'--','Ts-','LineWidth',1.3);
xline(0,'k-'); yline(0,'k-');
xlabel('Filtered Angular Velocity \omega [rad/s]');
ylabel('Estimated Friction Torque \tau_f [Nm]');
title('AK60 Friction Identification with Asymmetric Stribeck Model');
legend('Measured same-time scatter','Stribeck fit +','Stribeck fit -','Location','best');

%% Figure 2: low-speed zoom, the important Stribeck figure
figure('Color','w','Position',[170 120 1050 700]);
hold on; grid on; box on;
LOW_SPEED_VIEW = max(0.5,3*max(Vs_p,Vs_n));
idxLow = abs(w) <= LOW_SPEED_VIEW;
scatter(w(idxLow),tau(idxLow),12,'o');
ip = wCurveP <= LOW_SPEED_VIEW;
in = abs(wCurveN) <= LOW_SPEED_VIEW;
plot(wCurveP(ip),tauCurveP(ip),'LineWidth',3.0);
plot(wCurveN(in),tauCurveN(in),'LineWidth',3.0);
yline(Ts_p,'--','Ts+','LineWidth',1.3);
yline(-Ts_n,'--','Ts-','LineWidth',1.3);
xline(0,'k-'); yline(0,'k-');
xlim([-LOW_SPEED_VIEW LOW_SPEED_VIEW]);
xlabel('Filtered Angular Velocity \omega [rad/s]');
ylabel('Estimated Friction Torque \tau_f [Nm]');
title(sprintf('AK60 Low-Speed Stribeck Region (Vs+=%.3f, Vs-=%.3f rad/s)',Vs_p,Vs_n));
legend('Measured low-speed scatter','Stribeck fit +','Stribeck fit -','Location','best');

%% Figure 3: breakaway repeatability
figure('Color','w','Position',[210 150 900 550]);
hold on; grid on; box on;
trialNumber = (1:length(breakawayTorque))';
scatter(trialNumber(idxBreakP),breakawayTorque(idxBreakP),70,'filled');
scatter(trialNumber(idxBreakN),breakawayTorque(idxBreakN),70,'filled');
yline(Ts_p,'--','Median Ts+','LineWidth',1.5);
yline(-Ts_n,'--','Median Ts-','LineWidth',1.5);
xlabel('Breakaway Trial'); ylabel('Breakaway Motor Torque [Nm]');
title('AK60 Breakaway Torque Repeatability');

%% Figure 4: residuals
figure('Color','w','Position',[250 180 900 520]);
hold on; grid on; box on;
scatter(wP,tauP-tauPredP,10,'o');
scatter(wN,tauN-tauPredN,10,'o');
yline(0,'k--','LineWidth',1.2);
xlabel('Angular Velocity \omega [rad/s]');
ylabel('Stribeck Fit Residual [Nm]');
title('AK60 Stribeck Friction Fit Residual');

%% Figure 5: raw vs filtered speed
figure('Color','w','Position',[280 200 950 520]);
hold on; grid on; box on;
plot(time_ms,w_raw,'LineWidth',0.7);
plot(time_ms,w_filtered,'LineWidth',1.3);
xlabel('Time [ms]'); ylabel('Angular Velocity [rad/s]');
title('AK60 Raw and Filtered Speed');
legend('Raw Speed','Filtered Speed','Location','best');

%% Local functions

function z = logit(p)
    p = min(max(p,1e-8),1-1e-8);
    z = log(p./(1-p));
end

function tau = stribeckPos(w,Ts,Fc,B,Vs)
    w = abs(w);
    tau = Fc + (Ts-Fc).*exp(-(w./Vs).^2) + B.*w;
end

function tau = stribeckNeg(w,Ts,Fc,B,Vs)
    a = abs(w);
    mag = Fc + (Ts-Fc).*exp(-(a./Vs).^2);
    tau = -mag + B.*w;
end




function [Fc,B,Vs,tauPred,bestSSE] = ...
    fitStribeckGridPositive(w,tauMeasured,Ts,VsMin,VsMax,gridN)

    w = w(:);
    tauMeasured = tauMeasured(:);

    VsGrid = logspace(log10(VsMin),log10(VsMax),gridN);

    bestSSE = Inf;
    Fc = NaN;
    B = NaN;
    Vs = NaN;
    tauPred = nan(size(tauMeasured));

    for i = 1:length(VsGrid)

        currentVs = VsGrid(i);

        e = exp(-(w./currentVs).^2);

        % tau = Ts*e + Fc*(1-e) + B*w
        y = tauMeasured - Ts.*e;

        A = [(1-e), w];

        beta = A \ y;

        currentFc = beta(1);
        currentB = beta(2);

        if ~isfinite(currentFc) || ~isfinite(currentB)
            continue;
        end

        if currentFc < 0 || currentFc > Ts || currentB < 0
            continue;
        end

        currentPred = ...
            Ts.*e ...
            + currentFc.*(1-e) ...
            + currentB.*w;

        residual = tauMeasured-currentPred;
        currentSSE = sum(residual.^2);

        if currentSSE < bestSSE
            bestSSE = currentSSE;
            Fc = currentFc;
            B = currentB;
            Vs = currentVs;
            tauPred = currentPred;
        end

    end

    if ~isfinite(bestSSE)
        error('No physically valid positive Stribeck fit found.');
    end

    % Refine around the best coarse-grid value.
    refineGrid = linspace(max(VsMin,Vs/1.08),min(VsMax,Vs*1.08),1500);

    for i = 1:length(refineGrid)

        currentVs = refineGrid(i);
        e = exp(-(w./currentVs).^2);
        y = tauMeasured-Ts.*e;
        A = [(1-e),w];
        beta = A\y;

        currentFc = beta(1);
        currentB = beta(2);

        if ~isfinite(currentFc) || ~isfinite(currentB)
            continue;
        end

        if currentFc < 0 || currentFc > Ts || currentB < 0
            continue;
        end

        currentPred = ...
            Ts.*e ...
            + currentFc.*(1-e) ...
            + currentB.*w;

        residual = tauMeasured-currentPred;
        currentSSE = sum(residual.^2);

        if currentSSE < bestSSE
            bestSSE = currentSSE;
            Fc = currentFc;
            B = currentB;
            Vs = currentVs;
            tauPred = currentPred;
        end

    end
end


function [Fc,B,Vs,tauPred,bestSSE] = ...
    fitStribeckGridNegative(w,tauMeasured,Ts,VsMin,VsMax,gridN)

    w = w(:);
    tauMeasured = tauMeasured(:);

    VsGrid = logspace(log10(VsMin),log10(VsMax),gridN);

    bestSSE = Inf;
    Fc = NaN;
    B = NaN;
    Vs = NaN;
    tauPred = nan(size(tauMeasured));

    for i = 1:length(VsGrid)

        currentVs = VsGrid(i);

        a = abs(w);
        e = exp(-(a./currentVs).^2);

        % tau = -Ts*e - Fc*(1-e) + B*w
        y = tauMeasured + Ts.*e;

        A = [-(1-e), w];

        beta = A \ y;

        currentFc = beta(1);
        currentB = beta(2);

        if ~isfinite(currentFc) || ~isfinite(currentB)
            continue;
        end

        if currentFc < 0 || currentFc > Ts || currentB < 0
            continue;
        end

        currentPred = ...
            -Ts.*e ...
            - currentFc.*(1-e) ...
            + currentB.*w;

        residual = tauMeasured-currentPred;
        currentSSE = sum(residual.^2);

        if currentSSE < bestSSE
            bestSSE = currentSSE;
            Fc = currentFc;
            B = currentB;
            Vs = currentVs;
            tauPred = currentPred;
        end

    end

    if ~isfinite(bestSSE)
        error('No physically valid negative Stribeck fit found.');
    end

    % Refine around the best coarse-grid value.
    refineGrid = linspace(max(VsMin,Vs/1.08),min(VsMax,Vs*1.08),1500);

    for i = 1:length(refineGrid)

        currentVs = refineGrid(i);

        a = abs(w);
        e = exp(-(a./currentVs).^2);
        y = tauMeasured+Ts.*e;
        A = [-(1-e),w];
        beta = A\y;

        currentFc = beta(1);
        currentB = beta(2);

        if ~isfinite(currentFc) || ~isfinite(currentB)
            continue;
        end

        if currentFc < 0 || currentFc > Ts || currentB < 0
            continue;
        end

        currentPred = ...
            -Ts.*e ...
            - currentFc.*(1-e) ...
            + currentB.*w;

        residual = tauMeasured-currentPred;
        currentSSE = sum(residual.^2);

        if currentSSE < bestSSE
            bestSSE = currentSSE;
            Fc = currentFc;
            B = currentB;
            Vs = currentVs;
            tauPred = currentPred;
        end

    end
end

function r2 = calcR2(y,yhat)
    y = y(:); yhat = yhat(:);
    v = isfinite(y) & isfinite(yhat);
    y = y(v); yhat = yhat(v);
    ssRes = sum((y-yhat).^2);
    ssTot = sum((y-mean(y)).^2);
    if ssTot < 1e-12
        r2 = NaN;
    else
        r2 = 1-ssRes/ssTot;
    end
end
