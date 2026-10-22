% RAW_STRIBECK_FIT Fit an asymmetric Stribeck model to the exact raw data
% used by plot_friction_results.m's "Raw velocity-torque data" scatter.
% Run this file from any current folder. Outputs are written beside this file.

clear; clc;
outDir = fileparts(mfilename('fullpath'));
analyzeDir = fileparts(outDir);
repoRoot = fileparts(fileparts(analyzeDir));
inputCsv = fullfile(repoRoot, 'data', 'FR0007.csv');
if ~isfile(inputCsv), error('Input CSV not found: %s', inputCsv); end

% Match plot_friction_results.m exactly: phase 1..4, commanded torque above
% the configured zero tolerance, and finite speed/feedback torque. The x/y
% values are the original columns; no filtering, smoothing, or sign change.
T = readtable(inputCsv, 'CommentStyle', '#');
phase = double(T.phase);
command = double(T.target_torque_Nm);
omegaAll = double(T.speed_rad_s);
torqueAll = double(T.feedback_torque_estimate_Nm);
rawMask = phase >= 1 & phase <= 4 & abs(command) > 1e-9 & ...
    isfinite(omegaAll) & isfinite(torqueAll);
omega = omegaAll(rawMask);
torque = torqueAll(rawMask);
if isempty(omega), error('No valid raw-scatter data were selected from %s.', inputCsv); end

fitThreshold = 0.3; % rad/s; exclude the dense near-zero region from identification.

plusMask = omega > 0;
minusMask = omega < 0;
zeroMask = omega == 0;
plusFitMask = omega > fitThreshold;
minusFitMask = omega < -fitThreshold;
plus = fitDirection(omega(plusFitMask), torque(plusFitMask));
minus = fitDirection(omega(minusFitMask), torque(minusFitMask));

% A significant opposite-sign fraction is a diagnostic only. The measured
% torque convention is retained exactly as plotted and is never flipped.
positiveDiscordance = mean(torque(plusFitMask) < 0);
negativeDiscordance = mean(torque(minusFitMask) > 0);
signWarning = (any(plusFitMask) && positiveDiscordance > 0.1) || ...
    (any(minusFitMask) && negativeDiscordance > 0.1);

% One figure: all raw scatter points and any available directional fits.
fig = figure('Name','Asymmetric Stribeck fit','Color','w');
ax = axes(fig); hold(ax,'on'); grid(ax,'on');
scatter(ax, omega, torque, 7, [0.18 0.48 0.72], 'filled', ...
    'MarkerFaceAlpha',0.28,'MarkerEdgeAlpha',0.28,'DisplayName','Raw data');
lineHandles = gobjects(0,1);
lineLabels = {};
if plus.ok
    wp = linspace(min(omega(plusFitMask)), max(omega(plusFitMask)), 800);
    hp = plot(ax, wp, modelValue(wp, plus.p, 1), '-', ...
        'Color',[0.85 0.25 0.15],'LineWidth',2,'DisplayName','Stribeck fit');
    lineHandles(end+1,1) = hp; lineLabels{end+1} = 'Stribeck fit'; %#ok<SAGROW>
end
if minus.ok
    wm = linspace(min(omega(minusFitMask)), max(omega(minusFitMask)), 800);
    if isempty(lineLabels)
        displayName = 'Stribeck fit';
    else
        displayName = '';
    end
    hm = plot(ax, wm, modelValue(wm, minus.p, -1), '-', ...
        'Color',[0.20 0.62 0.34],'LineWidth',2,'DisplayName',displayName);
    lineHandles(end+1,1) = hm; %#ok<AGROW>
    if isempty(lineLabels), lineLabels{end+1} = 'Stribeck fit'; end %#ok<SAGROW>
end
xlabel(ax,'Angular velocity [rad/s]');
ylabel(ax,'Friction torque [Nm]');
if isempty(lineHandles)
    legend(ax, 'Raw data','Location','best');
else
    legend(ax, [findobj(ax,'DisplayName','Raw data'); lineHandles(1)], ...
        {'Raw data','Stribeck fit'}, 'Location','best');
end
savefig(fig, fullfile(outDir,'fit.fig'));
exportgraphics(fig, fullfile(outDir,'fit.png'), 'Resolution',200);

% Fit statistics cover the thresholded identification data only.
fitOmega = [omega(plusFitMask); omega(minusFitMask)];
fitTorque = [torque(plusFitMask); torque(minusFitMask)];
fitPrediction = NaN(size(fitOmega));
if plus.ok
    fitPrediction(1:sum(plusFitMask)) = modelValue(omega(plusFitMask),plus.p,1);
end
if minus.ok
    fitPrediction(sum(plusFitMask)+(1:sum(minusFitMask))) = ...
        modelValue(omega(minusFitMask),minus.p,-1);
end
if plus.ok && minus.ok
    overall = metrics(fitTorque,fitPrediction);
else
    overall = struct('rmse',NaN,'r2',NaN,'n',0);
end
if plus.ok, plusStats = metrics(torque(plusFitMask),modelValue(omega(plusFitMask),plus.p,1));
else, plusStats = struct('rmse',NaN,'r2',NaN,'n',0); end
if minus.ok, minusStats = metrics(torque(minusFitMask),modelValue(omega(minusFitMask),minus.p,-1));
else, minusStats = struct('rmse',NaN,'r2',NaN,'n',0); end

writeResults(fullfile(outDir,'result.txt'), inputCsv, omega, torque, ...
    plusMask, minusMask, zeroMask, fitThreshold, plusFitMask, minusFitMask, ...
    plus, minus, overall, plusStats, minusStats, positiveDiscordance, ...
    negativeDiscordance, signWarning);
fprintf('Raw scatter rows: %d (positive %d, negative %d, zero %d)\n', ...
    numel(omega),sum(plusMask),sum(minusMask),sum(zeroMask));
fprintf('Fit threshold: |omega| > %.6g rad/s; fit samples: positive %d, negative %d\n', ...
    fitThreshold,sum(plusFitMask),sum(minusFitMask));
fprintf('Outputs written to %s\n',outDir);

function result = fitDirection(w, y)
result = struct('ok',false,'p',nan(1,4),'exitflag',NaN, ...
    'message','','n',numel(w),'range',[NaN NaN],'sse',NaN);
if isempty(w)
    result.message = 'No nonzero-speed samples in this direction.'; return
end
result.range = [min(w),max(w)];
u = abs(w);
if numel(w) < 8
    result.message = sprintf('Insufficient samples: %d available; at least 8 required.',numel(w)); return
end
if numel(unique(u)) < 4 || max(u)-min(u) <= eps(max(u))*10
    result.message = 'Insufficient speed variation: need at least four distinct speed magnitudes.'; return
end
yscale = max([max(abs(y)),1e-6]);
wscale = max([max(u),1e-6]);
opts = optimset('Display','off','MaxIter',5000,'MaxFunEvals',12000, ...
    'TolX',1e-9,'TolFun',1e-12);
% Log-scaled parameters impose tau_c>0, tau_s>tau_c, v_s>0, b>0.
starts = [log(0.15) log(0.7) log(0.25) log(0.01); ...
          log(0.4)  log(0.4) log(0.10) log(0.001); ...
          log(0.05) log(1.2) log(0.5)  log(0.03)];
best = Inf; bestQ = []; bestFlag = NaN;
for k=1:size(starts,1)
    [q,fval,exitflag] = fminsearch(@(q)objective(q,w,y,yscale,wscale),starts(k,:),opts);
    if isfinite(fval) && fval < best
        best=fval; bestQ=q; bestFlag=exitflag;
    end
end
if isempty(bestQ)
    result.message = 'Optimization did not produce a finite objective.'; return
end
p = decode(bestQ,yscale,wscale);
if any(~isfinite(p))
    result.message = 'Optimization returned non-finite parameters.'; return
end
result.ok=true; result.p=p; result.exitflag=bestFlag;
result.sse=best*yscale^2;
if bestFlag > 0
    result.message = sprintf('Converged (fminsearch exit flag %d).',bestFlag);
else
    result.message = sprintf('Best finite solution returned, but convergence was not confirmed (exit flag %d).',bestFlag);
end
end

function f = objective(q,w,y,yscale,wscale)
p = decode(q,yscale,wscale);
if any(~isfinite(p)), f=realmax; return; end
r = modelValue(w,p,sign(w))-y;
f = sum((r/yscale).^2);
if ~isfinite(f), f=realmax; end
end

function p = decode(q,yscale,wscale)
q = max(-30,min(30,q));
tc = yscale*exp(q(1));
delta = yscale*exp(q(2));
vs = wscale*exp(q(3));
b = (yscale/wscale)*exp(q(4));
p = [tc,tc+delta,vs,b];
end

function yhat = modelValue(w,p,direction)
yhat = direction .* (p(1)+(p(2)-p(1)).*exp(-(abs(w)./p(3)).^2)) + p(4).*w;
end

function m = metrics(y,yhat)
r = yhat-y;
m.n=numel(y); m.rmse=sqrt(mean(r.^2));
sse=sum(r.^2); sst=sum((y-mean(y)).^2);
if sst==0, m.r2=NaN; else, m.r2=1-sse/sst; end
end

function writeResults(path,inputCsv,w,y,plusMask,minusMask,zeroMask, ...
    fitThreshold,plusFitMask,minusFitMask,plus,minus,overall,plusStats, ...
    minusStats,discordPlus,discordMinus,signWarning)
fid=fopen(path,'w');
if fid<0, error('Cannot write result file: %s',path); end
c=onCleanup(@()fclose(fid));
fprintf(fid,'Asymmetric Stribeck friction model fit\n');
fprintf(fid,'======================================\n\n');
fprintf(fid,'Model used (omega ~= 0):\n');
fprintf(fid,'tau_hat(omega) = sgn(omega) * [tau_c_sigma + (tau_s_sigma - tau_c_sigma) * exp(-(abs(omega)/v_s_sigma)^2)] + b_sigma * omega\n');
fprintf(fid,'sigma=+ for omega>0 and sigma=- for omega<0; the measured torque sign was retained.\n');
fprintf(fid,'Constraints: tau_s >= tau_c >= 0, v_s > 0, b >= 0.\n\n');
writeDirection(fid,'Positive rotation (+)',plus, ...
    {'tau_c_plus','tau_s_plus','v_s_plus','b_plus'});
writeDirection(fid,'Negative rotation (-)',minus, ...
    {'tau_c_minus','tau_s_minus','v_s_minus','b_minus'});
fprintf(fid,'Parameter meanings and units:\n');
fprintf(fid,'tau_c: Coulomb friction torque [Nm]\ntau_s: low-speed Stribeck friction torque [Nm]\n');
fprintf(fid,'v_s: Stribeck characteristic speed [rad/s]\nb: viscous friction coefficient [Nm/(rad/s)]\n\n');
fprintf(fid,'Data counts: raw scatter total=%d; positive=%d; negative=%d; zero-speed=%d.\n', ...
    numel(w),sum(plusMask),sum(minusMask),sum(zeroMask));
fprintf(fid,'Fit threshold: fitThreshold=%.6g rad/s; fit only omega>fitThreshold or omega<-fitThreshold.\n',fitThreshold);
fprintf(fid,'Identification data counts: total=%d; positive=%d; negative=%d.\n', ...
    sum(plusFitMask)+sum(minusFitMask),sum(plusFitMask),sum(minusFitMask));
fprintf(fid,'Identification speed ranges: positive=[%.6g, %.6g] rad/s; negative=[%.6g, %.6g] rad/s.\n', ...
    rangeOrNaN(w(plusFitMask)), rangeOrNaN(w(minusFitMask)));
fprintf(fid,'Fit statistics use unweighted raw torque residuals on thresholded identification data only.\n');
fprintf(fid,'Overall RMSE=%.6g Nm; R^2=%.6g (N=%d).\n',overall.rmse,overall.r2,overall.n);
fprintf(fid,'Positive RMSE=%.6g Nm; R^2=%.6g (N=%d).\n',plusStats.rmse,plusStats.r2,plusStats.n);
fprintf(fid,'Negative RMSE=%.6g Nm; R^2=%.6g (N=%d).\n\n',minusStats.rmse,minusStats.r2,minusStats.n);
fprintf(fid,'Positive-direction optimization: %s\n',plus.message);
fprintf(fid,'Negative-direction optimization: %s\n',minus.message);
if ~(plus.ok && minus.ok)
    fprintf(fid,'Overall metrics are NaN because a complete two-direction prediction over the identification data is unavailable.\n');
end
if isnan(plusStats.r2), fprintf(fid,'Positive R^2 is NaN because its total torque variation is zero or the direction was not fitted.\n'); end
if isnan(minusStats.r2), fprintf(fid,'Negative R^2 is NaN because its total torque variation is zero or the direction was not fitted.\n'); end
fprintf(fid,'Opposite-sign fraction in identification data: positive speed / negative torque=%.6g; negative speed / positive torque=%.6g.\n',discordPlus,discordMinus);
if signWarning
    fprintf(fid,'WARNING: more than 10%% of samples in at least one direction have torque opposite to the model sign convention. No sign correction was applied; inspect model/data sign compatibility.\n');
else
    fprintf(fid,'Sign check: no direction exceeds the 10%% opposite-sign warning threshold.\n');
end
fprintf(fid,'\nData file: %s\n',inputCsv);
fprintf(fid,'Raw source: main.m calls plot_friction_results; its "Raw velocity-torque data" scatter uses T.speed_rad_s and T.feedback_torque_estimate_Nm.\n');
fprintf(fid,'Selection copied from plot_friction_results.m: phase 1..4, abs(target_torque_Nm)>1e-9 Nm, finite speed and feedback torque.\n');
fprintf(fid,'No smoothing, binning, averaging, steady-state selection, or sign change was applied.\n');
fprintf(fid,'Angular velocity unit: rad/s. Feedback estimated torque unit: Nm.\n');
end

function writeDirection(fid,label,r,names)
fprintf(fid,'%s parameters:\n',label);
if r.ok
    p=r.p;
    fprintf(fid,'  %s = %.6g Nm\n  %s = %.6g Nm\n  %s = %.6g rad/s\n  %s = %.6g Nm/(rad/s)\n', ...
        names{1},p(1),names{2},p(2),names{3},p(3),names{4},p(4));
    if contains(names{1},'plus'), sigma=1; else, sigma=-1; end
    fprintf(fid,'  Substituted equation: tau_hat(omega) = (%+d) * [%.6g + (%.6g - %.6g) * exp(-(abs(omega)/%.6g)^2)] + %.6g * omega\n', ...
        sigma,p(1),p(2),p(1),p(3),p(4));
else
    fprintf(fid,'  Not identified. Reason: %s\n',r.message);
    for j=1:4, fprintf(fid,'  %s = NaN (not identified)\n',names{j}); end
end
fprintf(fid,'\n');
end

function r = rangeOrNaN(x)
if isempty(x), r=[NaN NaN]; else, r=[min(x),max(x)]; end
end
