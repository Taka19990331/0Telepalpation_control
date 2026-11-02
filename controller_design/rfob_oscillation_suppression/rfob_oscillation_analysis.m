function results = rfob_oscillation_analysis(userCfg)
% RFOB design laboratory. All calculations are local functions in this file.
% Requires MATLAB R2021b+ and Signal Processing Toolbox. No Simulink.
%% CONFIGURATION -- all user-adjustable settings (SI units unless noted)
cfg.RUN_DATA_ANALYSIS = true;
cfg.RUN_SPECTRAL_ANALYSIS = true;
cfg.RUN_AMPLITUDE_DETECTION = true;
cfg.RUN_FRICTION_COMPARISON = true;
cfg.RUN_REPLAY_ANALYSIS = true;
cfg.RUN_CLOSED_LOOP_SIM = true;
cfg.baselineOnly = false;              % Skip suppression / voluntary simulations
cfg.noise.enabled = false;
cfg.noise.seed = 17;
cfg.noise.block_s = .5;
cfg.noise.samples = [];
cfg.noise.dt_s = .0005;
cfg.RUN_PARAMETER_SWEEP = false;
cfg.RUN_ROBUSTNESS_ANALYSIS = false;
cfg.figures = true(1,10);                 % Individual Figure 1..10 switches
cfg.showFFT = false;                     % Additional FFT amplitude figure
cfg.visible = 'on';                      % 'off' for batch rendering
cfg.saveResults = true;
cfg.savePNG = false;
cfg.saveFIG = false;
cfg.outputDir = fullfile(fileparts(mfilename('fullpath')), 'results');
cfg.dataDir = fullfile(fileparts(mfilename('fullpath')), '..','..', ...
    'controller_implementation','trans_ctrl_test1','data');
cfg.files = {'trans_ctrl_test1_20261102_122211.csv', ...
    'trans_ctrl_test1_20261102_122519.csv','trans_ctrl_test1_20261102_123759.csv'};
cfg.labels = {'Free rotation (Kr=0, Kf=0)','No operation (output OFF)', ...
    'Gripped rotor (Kr=0.75, Kf=0)'};
cfg.selection.enabled = [NaN 0 1];      % NaN=both; otherwise require 0/1
cfg.selection.startup_s = 1;            % Exclude initial startup [s]
cfg.selection.transitionGuard_s = 0.5;  % Exclude either side of ON/OFF [s]
cfg.selection.timeRange_s = [0 Inf];    % Relative to first record [s]
cfg.selection.equalDuration = true;    % Longest contiguous valid run per log
cfg.selection.maxDuration_s = Inf;      % Common duration cap [s]
cfg.selection.maxGapPeriods = 3;       % Split at dropped-record gaps
cfg.spectrum.window = 8192;             % Hann samples, shortened for short runs
cfg.spectrum.overlap = 0.5;
cfg.spectrum.displayHz = [2 40];
cfg.spectrum.detailHz = [6 25];
cfg.spectrum.metricHz = [10 14];
cfg.spectrum.minSamples = 32;
cfg.controller.mode = 'compare';        % none / notch / damping / compare
cfg.controller.bandHz = [8 20];         % BPF analog half-power edges [Hz]
cfg.controller.rmsTau_s = 0.2;
cfg.controller.on_rad_s = 0.10;
cfg.controller.full_rad_s = 0.20;
cfg.controller.damping = 0.1;           % [Nm s/rad], starting point, not optimal
cfg.controller.notchHz = 12;
cfg.controller.notchWidthHz = 4;        % Denominator bandwidth [Hz]
cfg.controller.notchDepth = 0.95;       % 0=no notch, 1=zero at center
cfg.controller.gainTau_s = 0.05;         % Gain LPF, 0 disables [s]
cfg.controller.gainRate_s = 10;          % Maximum |d gamma/dt| [1/s], Inf disables
cfg.friction.widths_rad_s = [0 .05 .1 .2 .4]; % 0=original discontinuous model
cfg.friction.closedLoopWidth_rad_s = .1;
cfg.friction.curveRange_rad_s = [-1 1];
cfg.friction.curveSamples = 1001;
cfg.friction.lowSpeed_rad_s = .1;
% Simulation defaults below come from this experiment; metadata replaces them.
% Override via userCfg.sim after metadata import to run deliberate experiments.
cfg.sim.useLogMetadata = true;
cfg.sim.dt_s = .0005;
cfg.sim.duration_s = 12;
cfg.sim.discard_s = 3;
cfg.sim.J = .0116806;                    % Actual plant inertia [kg m^2]
cfg.sim.observerJ = .0116806;            % Observer nominal inertia [kg m^2]
cfg.sim.Q_Hz = 400;
cfg.sim.velocityLPF_Hz = 1000;
cfg.sim.Kr = .75;
cfg.sim.Kf = 0;
cfg.sim.limit_Nm = 1.5;
cfg.sim.delay_s = .001477;               % Command transport delay [s]
cfg.sim.motorTau_s = 0;                 % Optional torque lag [s]
cfg.sim.Kh = 66.4;                      % Assumed grip stiffness [Nm/rad]
cfg.sim.Dh = .05;                       % Assumed grip viscosity [Nm s/rad]
cfg.sim.thetaHand_rad = 0;
cfg.sim.initialTheta_rad = .01;         % Initial displacement, NOT sinusoidal drive
cfg.sim.initialVelocity_rad_s = 0;
cfg.sim.plantFrictionScale = 1;
cfg.sim.plantPositive = [.459212 .526341 .474762 .00218903]; % tc,ts,vs,b
cfg.sim.plantNegative = [.410719 .527041 .473325 .00341074];
cfg.sim.observerPositive = cfg.sim.plantPositive;
cfg.sim.observerNegative = cfg.sim.plantNegative;
cfg.sim.substeps = 10;                  % Proximal stick/slip integration per tick
cfg.sim.velocityQuantum_rad_s = 0;      % 0=ideal; MIT step approximately 75/4095
cfg.sim.torqueQuantum_Nm = 0;            % 0=ideal; MIT step approximately 64/4095
cfg.sim.transparencyHz = .5;            % Separate voluntary motion test [Hz]
cfg.sim.transparencyTorque_Nm = .8;     % Exceeds nominal breakaway friction [Nm]
cfg.sim.transparencyKh = 0;             % Free rotation test [Nm/rad]
cfg.sim.transparencyDh = .02;           % Assumed hand viscosity [Nm s/rad]
cfg.validation.frequencyToleranceHz = 2;
cfg.validation.rmsRatioRange = [.5 2];
cfg.validation.sustainedRatioRange = [.5 2]; % Late/early RMS in retained interval
cfg.validation.minimumRMS_rad_s = .01;
cfg.sweep.axes = {'controller.damping','controller.on_rad_s'};
cfg.sweep.values = {0:.05:.3, .05:.025:.15}; % Full threshold must exceed on
cfg.sweep.mode = 'damping';
cfg.sweep.maxRuns = 200;
cfg.robustness.Kh = [40 66.4 90];        % [Nm/rad]
cfg.robustness.Dh = [.02 .05 .15];       % [Nm s/rad]
cfg.robustness.delay_s = [.0005 .001477 .003];
cfg.plot.responsePoints = 2048;
cfg.plot.lineWidth = 1;
cfg.plot.exportDPI = 150;
%% Resolve overrides and prerequisites
if nargin < 1, userCfg = struct; end
cfg = mergeConfig(cfg,userCfg);
validateConfig(cfg);
assert(exist('pwelch','file')==2, 'Signal Processing Toolbox (pwelch) is required.');
results = struct; logs = cell(1,3);
needLogs = cfg.RUN_DATA_ANALYSIS || cfg.RUN_SPECTRAL_ANALYSIS || ...
    cfg.RUN_AMPLITUDE_DETECTION || cfg.RUN_FRICTION_COMPARISON || ...
    cfg.RUN_REPLAY_ANALYSIS || cfg.RUN_CLOSED_LOOP_SIM || cfg.sim.useLogMetadata || cfg.noise.enabled;
if needLogs
    for k=1:3, logs{k}=readLog(fullfile(cfg.dataDir,cfg.files{k}),cfg,k); end
    if cfg.selection.equalDuration
        duration = min(cellfun(@(d)d.t(d.indices(end))-d.t(d.indices(1)),logs));
        for k=1:3
            d=logs{k}; d.indices=d.indices(d.t(d.indices)<=d.t(d.indices(1))+duration); logs{k}=d;
        end
    end
    for k=1:3, logs{k}=uniformSegment(logs{k}); end
    if cfg.sim.useLogMetadata
        m=logs{3}.meta;
        cfg.sim.dt_s=m.period_us*1e-6; cfg.sim.J=m.J; cfg.sim.observerJ=m.J;
        cfg.sim.Q_Hz=m.rfob_q_hz; cfg.sim.velocityLPF_Hz=m.velocity_lpf_hz;
        cfg.sim.limit_Nm=m.torque_limit_Nm;
        cfg.sim.Kr=m.initial_K_R; cfg.sim.Kf=m.initial_K_F;
        [p,n]=frictionParameters(m);
        cfg.sim.plantPositive=p; cfg.sim.plantNegative=n;
        cfg.sim.observerPositive=p; cfg.sim.observerNegative=n;
        if isfield(userCfg,'sim'), cfg.sim=mergeConfig(cfg.sim,userCfg.sim); end
    end
end
if cfg.noise.enabled
    ns=logs{2}.segment;
    cfg.noise.samples=[ns.velocity_raw_rad_s ns.feedback_torque_Nm];
    cfg.noise.samples=cfg.noise.samples-mean(cfg.noise.samples,1);
    cfg.noise.dt_s=logs{2}.dt;
end
validateConfig(cfg); results.cfg=cfg;
if cfg.RUN_DATA_ANALYSIS
    for k=1:3
        d=logs{k}; s=d.segment;
        results.data(k)=struct('condition',cfg.labels{k},'metadata',d.meta, ...
            'selectedStart_s',s.t(1),'selectedEnd_s',s.t(end),'samples',numel(s.t), ...
            'enabledFraction',mean(s.control_enabled),'velocityRMS',rms(s.velocity_filtered_rad_s), ...
            'timestampJitterRMS_s',d.jitter,'gapCount',d.gaps,'intervals',d.intervals);
    end
    disp(struct2table(rmfield(results.data,{'metadata','intervals'})));
end
if cfg.RUN_SPECTRAL_ANALYSIS
    for k=1:3
        d=logs{k}; s=d.segment;
        results.spectra{k}=spectra(s.velocity_filtered_rad_s,d.dt,cfg);
        results.torqueSpectra{k}=spectra(s.external_torque_est_Nm,d.dt,cfg);
        results.rfobSpectra{k}=spectra(s.torque_RFOB_Nm,d.dt,cfg);
    end
end
if cfg.RUN_AMPLITUDE_DETECTION
    for k=1:3
        d=logs{k}; results.detector{k}=detector(d.segment.velocity_filtered_rad_s,d.dt,cfg);
    end
end
if cfg.RUN_FRICTION_COMPARISON
    for k=1:3, results.friction{k}=frictionComparison(logs{k},cfg); end
    fprintf('\nTrajectory-fixed friction counterfactual: NOT a closed-loop suppression ratio.\n');
    disp(results.friction{3}.summary);
    disp(results.friction{3}.reconstruction);
end
if cfg.RUN_REPLAY_ANALYSIS
    results.replay=replay(logs{3},cfg);
    fprintf('\nReplay holds measured motion fixed; no prediction of changed rotor motion.\n');
    disp(results.replay.summary);
end
if cfg.RUN_CLOSED_LOOP_SIM
    fprintf('\nValidating autonomous baseline first (initial displacement only)...\n');
    base=simulate(cfg,'none',false); measured=logs{3}.segment;
    mm=metrics(measured.t,measured.velocity_filtered_rad_s,measured.external_torque_est_Nm, ...
        measured.torque_RFOB_Nm,zeros(size(measured.t)),zeros(size(measured.t)), ...
        measured.torque_cmd_Nm,measured.torque_cmd_unsat_Nm,logs{3}.dt,cfg);
    bm=base.measurementMetrics; % Like-for-like sensor output; true motion retained in base.metrics
    ratio=bm.bandRMS/mm.bandRMS;
    reproduced=abs(bm.peakHz-mm.peakHz)<=cfg.validation.frequencyToleranceHz && ...
        ratio>=cfg.validation.rmsRatioRange(1) && ratio<=cfg.validation.rmsRatioRange(2) && ...
        bm.sustainedRatio>=cfg.validation.sustainedRatioRange(1) && ...
        bm.sustainedRatio<=cfg.validation.sustainedRatioRange(2) && ...
        bm.bandRMS>=cfg.validation.minimumRMS_rad_s;
    results.validation=struct('reproduced',reproduced,'measured',mm,'baseline',bm, ...
        'velocityRMSratio',ratio,'note','Screening only: hand/friction states are not identified.');
    commonBaseline=rmfield(bm,setdiff(fieldnames(bm),fieldnames(mm)));
    disp(struct2table([mm commonBaseline],'RowNames',{'Measured','Model baseline'}));
    if ~reproduced
        warning('RFOB:Unvalidated','Baseline does not reproduce measured oscillation. Comparisons below are exploratory model predictions only.');
    end
    modes=selectedModes(cfg); if cfg.baselineOnly, modes={'none'}; end
    closed=cell(size(modes)); transparent=closed;
    for k=1:numel(modes)
        if strcmp(modes{k},'none'), closed{k}=base; else, closed{k}=simulate(cfg,modes{k},false); end
        if ~cfg.baselineOnly, transparent{k}=simulate(cfg,modes{k},true); end
    end
    results.closed=closed; results.transparency=transparent; results.modes=modes;
    results.closedSummary=summaryTable(closed,modes); disp(results.closedSummary);
    if ~cfg.baselineOnly, results.transparencySummary=summaryTable(transparent,modes); end
end
if cfg.RUN_PARAMETER_SWEEP, results.sweep=parameterSweep(cfg); end
if cfg.RUN_ROBUSTNESS_ANALYSIS, results.robustness=robustness(cfg); end
results.notes = {'Replay and friction counterfactual hold the trajectory fixed.', ...
    'Plant uses proximal stick/slip friction; observer uses signed Stribeck or tanh.', ...
    'BPF/notch: bilinear transform with center prewarp; all controller filters causal.', ...
    'Closed-loop predictions require baseline validation and hardware verification.'};
figs=drawFigures(results,logs,cfg);
if cfg.saveResults || cfg.savePNG || cfg.saveFIG
    if ~exist(cfg.outputDir,'dir'), mkdir(cfg.outputDir); end
    if cfg.saveResults
        save(fullfile(cfg.outputDir,'analysis_results.mat'),'results','-v7.3');
        if isfield(results,'closedSummary'), writetable(results.closedSummary,fullfile(cfg.outputDir,'closed_loop_metrics.csv')); end
        if isfield(results,'transparencySummary'), writetable(results.transparencySummary,fullfile(cfg.outputDir,'transparency_metrics.csv')); end
        if isfield(results,'friction'), writetable(results.friction{3}.summary,fullfile(cfg.outputDir,'friction_metrics.csv')); end
        if isfield(results,'replay'), writetable(results.replay.summary,fullfile(cfg.outputDir,'replay_metrics.csv')); end
        if isfield(results,'robustness'), writetable(results.robustness,fullfile(cfg.outputDir,'robustness_metrics.csv')); end
        if isfield(results,'sweep'), writetable(results.sweep.metrics,fullfile(cfg.outputDir,'sweep_metrics.csv')); end
    end
    for k=1:numel(figs)
        stem=fullfile(cfg.outputDir,sprintf('figure_%02d',figs(k).Number));
        if cfg.savePNG, exportgraphics(figs(k),[stem '.png'],'Resolution',cfg.plot.exportDPI); end
        if cfg.saveFIG, savefig(figs(k),[stem '.fig']); end
    end
end
end

function c=mergeConfig(c,u)
assert(isstruct(u),'Configuration override must be a struct.');
f=fieldnames(u);
for k=1:numel(f)
    assert(isfield(c,f{k}),'Unknown configuration field: %s',f{k});
    if isstruct(c.(f{k})), c.(f{k})=mergeConfig(c.(f{k}),u.(f{k})); else, c.(f{k})=u.(f{k}); end
end
end

function validateConfig(c)
a=c.controller; s=c.sim;
assert(ismember(a.mode,{'none','notch','damping','compare'}),'Invalid controller mode.');
assert(numel(c.figures)==10 && all(ismember(c.figures,[0 1])),'figures must have 10 booleans.');
assert(a.on_rad_s>=0 && a.full_rad_s>a.on_rad_s,'full threshold must exceed on threshold.');
assert(a.rmsTau_s>0 && a.gainTau_s>=0 && a.gainRate_s>0 && a.damping>=0,'Invalid detector/gain settings.');
assert(a.bandHz(1)>0 && a.bandHz(2)>a.bandHz(1) && a.bandHz(2)<.5/s.dt_s,'Invalid BPF band.');
assert(a.notchHz>0 && a.notchHz<.5/s.dt_s && a.notchWidthHz>0 && a.notchDepth>=0 && a.notchDepth<=1,'Invalid notch.');
assert(s.J>0 && s.observerJ>0 && s.dt_s>0 && s.Q_Hz>0 && s.velocityLPF_Hz>0 && s.limit_Nm>0,'Invalid model parameters.');
assert(s.duration_s>s.discard_s && s.discard_s>=0 && s.delay_s>=0 && s.motorTau_s>=0,'Invalid time settings.');
assert(s.substeps>=1 && s.substeps==fix(s.substeps),'substeps must be a positive integer.');
assert(s.Kh>=0 && s.Dh>=0 && s.transparencyKh>=0 && s.transparencyDh>=0,'Negative hand impedance.');
assert(s.velocityQuantum_rad_s>=0 && s.torqueQuantum_Nm>=0 && s.plantFrictionScale>=0,'Invalid plant nonlinearity.');
assert(all(c.friction.widths_rad_s>=0) && c.friction.closedLoopWidth_rad_s>0,'Invalid smoothing width.');
assert(c.spectrum.overlap>=0 && c.spectrum.overlap<1,'Overlap must be in [0,1).');
end

function d=readLog(path,c,k)
assert(isfile(path),'Missing CSV: %s',path);
fid=fopen(path,'r'); cleanup=onCleanup(@()fclose(fid)); m=struct; headers=0;
while true
    line=fgetl(fid); assert(ischar(line),'Missing CSV header: %s',path);
    if ~startsWith(line,'#'), break; end
    headers=headers+1; parts=strsplit(line(2:end),',');
    val=str2double(parts{2}); if isnan(val), val=parts{2}; end
    m.(matlab.lang.makeValidName(parts{1}))=val;
end
T=readtable(path,'NumHeaderLines',headers,'VariableNamingRule','preserve','CommentStyle','#');
required={'timestamp_us','velocity_raw_rad_s','velocity_filtered_rad_s','feedback_torque_Nm', ...
    'torque_bias_Nm','feedback_age_us','friction_model_Nm','rfob_input_Nm','rfob_filter_state_Nm', ...
    'external_torque_est_Nm','torque_RFOB_Nm','torque_cmd_Nm','torque_cmd_unsat_Nm','control_enabled','K_R','K_F'};
assert(all(ismember(required,T.Properties.VariableNames)),'Required columns missing in %s',path);
t=(T.timestamp_us-T.timestamp_us(1))*1e-6; dt=m.log_period_us*1e-6;
assert(all(diff(t)>0),'Nonmonotonic timestamps: %s',path);
valid=all(isfinite(T{:,required}),2) & t>=c.selection.startup_s & ...
    t>=c.selection.timeRange_s(1) & t<=c.selection.timeRange_s(2) & T.feedback_age_us< m.feedback_timeout_us;
if ismember('fault_code',T.Properties.VariableNames), valid=valid & T.fault_code==0; end
if ~isnan(c.selection.enabled(k)), valid=valid & T.control_enabled==c.selection.enabled(k); end
changes=find(diff(T.control_enabled)~=0)+1;
for j=changes', valid(abs(t-t(j))<c.selection.transitionGuard_s)=false; end
starts=[1;changes]; ends=[changes-1;height(T)];
intervals=table(t(starts),t(ends),T.control_enabled(starts),'VariableNames',{'start_s','end_s','enabled'});
breaks=[true; diff(t)>c.selection.maxGapPeriods*dt];
start=find(valid & ([true;~valid(1:end-1)] | breaks));
stop=find(valid & ([~valid(2:end);true] | [breaks(2:end);true]));
assert(~isempty(start),'No usable interval for %s; adjust selection settings.',path);
[~,j]=max(t(stop)-t(start)); ix=(start(j):stop(j))';
ix=ix(t(ix)<=t(ix(1))+c.selection.maxDuration_s);
assert(numel(ix)>=c.spectrum.minSamples,'Selected run is too short: %s',path);
d=struct('T',T,'t',t,'dt',dt,'meta',m,'indices',ix,'intervals',intervals, ...
    'jitter',rms(diff(t)-dt),'gaps',sum(diff(t)>c.selection.maxGapPeriods*dt));
fprintf('%s: %.3f..%.3f s, enabled %.2f, %d gaps; J=%g, Q=%g Hz, velocity LPF=%g Hz, limit=%g Nm\n', ...
    c.labels{k},t(ix(1)),t(ix(end)),mean(T.control_enabled(ix)),d.gaps,m.J,m.rfob_q_hz,m.velocity_lpf_hz,m.torque_limit_Nm);
end

function d=uniformSegment(d)
ix=d.indices; t=(d.t(ix(1)):d.dt:d.t(ix(end)))';
s=struct('t',t); names=d.T.Properties.VariableNames;
for k=1:numel(names)
    % Zero-order hold avoids using future measurements in replay/controller.
    s.(names{k})=interp1(d.t(ix),d.T.(names{k})(ix),t,'previous');
end
d.segment=s;
end

function [p,n]=frictionParameters(m)
p=[m.positive_tc m.positive_ts m.positive_vs m.positive_b];
n=[m.negative_tc m.negative_ts m.negative_vs m.negative_b];
end

function y=friction(w,p,n,width)
P=repmat(p,numel(w),1); P(w(:)<0,:)=repmat(n,nnz(w(:)<0),1);
x=w(:); mag=P(:,1)+(P(:,2)-P(:,1)).*exp(-(x./P(:,3)).^2);
if width==0, direction=sign(x); else, direction=tanh(x/width); end
y=reshape(mag.*direction+P(:,4).*x,size(w));
end

function f=filters(dt,c)
% Prewarped bilinear substitution s=K*(1-z^-1)/(1+z^-1).
a=c.controller; fc=sqrt(prod(a.bandHz)); wn=2*pi*fc;
K=wn/tan(pi*fc*dt); bw=2*pi*diff(a.bandHz);
den=[K*K+bw*K+wn*wn,2*(wn*wn-K*K),K*K-bw*K+wn*wn];
f.bpB=[bw*K 0 -bw*K]/den(1); f.bpA=den/den(1);
wn=2*pi*a.notchHz; K=wn/tan(pi*a.notchHz*dt); bw=2*pi*a.notchWidthHz;
den=[K*K+bw*K+wn*wn,2*(wn*wn-K*K),K*K-bw*K+wn*wn];
num=[K*K+(1-a.notchDepth)*bw*K+wn*wn,2*(wn*wn-K*K),K*K-(1-a.notchDepth)*bw*K+wn*wn];
f.nB=num/den(1); f.nA=den/den(1);
end

function d=detector(w,dt,c)
f=filters(dt,c); bp=filter(f.bpB,f.bpA,w);
a=exp(-dt/c.controller.rmsTau_s); A=sqrt(max(0,filter(1-a,[1 -a],bp.^2)));
g=zeros(size(A)); prev=0;
for j=1:numel(A), prev=gainStep(A(j),prev,dt,c.controller); g(j)=prev; end
d=struct('bp',bp,'amplitude',A,'gamma',g);
end

function g=gainStep(A,prev,dt,c)
target=min(1,max(0,(A-c.on_rad_s)/(c.full_rad_s-c.on_rad_s)));
if c.gainTau_s==0, alpha=1; else, alpha=1-exp(-dt/c.gainTau_s); end
delta=alpha*(target-prev); bound=c.gainRate_s*dt;
g=prev+max(-bound,min(bound,delta));
end

function s=spectra(x,dt,c)
x=x(:)-mean(x); N=min(c.spectrum.window,numel(x)); win=hann(N,'periodic');
[P,f]=pwelch(x,win,floor(c.spectrum.overlap*N),N,1/dt,'onesided');
s=struct('f',f,'P',P,'windowSamples',N,'resolutionHz',1/(N*dt), ...
    'normalization','One-sided Welch density, periodic Hann; each record mean removed.');
end

function r=bandRMS(x,dt,band,c)
s=spectra(x,dt,c); df=s.f(2)-s.f(1);
% Bin-overlap integration also works when the requested band is narrower than df.
weight=max(0,min(s.f+df/2,band(2))-max(s.f-df/2,band(1)));
r=sqrt(sum(s.P.*weight));
end

function out=frictionComparison(d,c)
T=d.T; m=d.meta; [p,n]=frictionParameters(m); w=T.velocity_filtered_rad_s;
widths=c.friction.widths_rad_s; count=numel(widths); torque=zeros(height(T),count); ext=torque;
% RX timestamp inferred from ISR timestamp minus feedback age. Multiple RX frames
% between log rows are not recoverable; reconstruction residual reports this.
rx=T.timestamp_us*1e-6-T.feedback_age_us*1e-6;
valid=T.feedback_age_us<m.feedback_timeout_us; first=find(valid,1);
assert(~isempty(first),'No valid feedback records.');
g=2*pi*m.rfob_q_hz; nominal=friction(w,p,n,0);
reconstructed=zeros(height(T),1); state=T.rfob_filter_state_Nm(first);
reconstructed(first)=m.J*g*w(first)-state;
for i=first+1:height(T)
    dt=max(0,rx(i)-rx(i-1));
    if valid(i) && dt>0
        input=T.feedback_torque_Nm(i)-T.torque_bias_Nm(i)-nominal(i)+m.J*g*w(i);
        state=state+(1-exp(-g*dt))*(input-state);
    end
    reconstructed(i)=m.J*g*w(i)-state;
end
for k=1:count
    torque(:,k)=friction(w,p,n,widths(k)); correction=0;
    for i=first:height(T)
        if i>first && valid(i)
            alpha=1-exp(-g*max(0,rx(i)-rx(i-1)));
            correction=correction+alpha*((nominal(i)-torque(i,k))-correction);
        end
        % Anchored counterfactual retains logged baseline and propagates delta state.
        ext(i,k)=T.external_torque_est_Nm(i)-correction;
    end
end
ix=d.indices; t=d.segment.t;
ft=interp1(d.t(ix),torque(ix,:),t,'previous'); et=interp1(d.t(ix),ext(ix,:),t,'previous');
fr=zeros(count,1); er=fr; low=fr;
mask=abs(d.segment.velocity_filtered_rad_s)<=c.friction.lowSpeed_rad_s;
for k=1:count
    fr(k)=bandRMS(ft(:,k),d.dt,c.spectrum.metricHz,c);
    er(k)=bandRMS(et(:,k),d.dt,c.spectrum.metricHz,c);
    if any(mask), low(k)=rms(ft(mask,k)); else, low(k)=NaN; end
end
err=reconstructed(ix)-T.external_torque_est_Nm(ix);
inputErr=T.feedback_torque_Nm(ix)-T.torque_bias_Nm(ix)-nominal(ix)+m.J*g*w(ix)-T.rfob_input_Nm(ix);
identityErr=m.J*g*w(ix)-T.rfob_filter_state_Nm(ix)-T.external_torque_est_Nm(ix);
out.reconstruction=table(rms(err),max(abs(err)),rms(inputErr),rms(identityErr),rms(nominal(ix)-T.friction_model_Nm(ix)), ...
    'VariableNames',{'recursiveRMS_Nm','recursiveMax_Nm','inputRMS_Nm','stateIdentityRMS_Nm','frictionRMS_Nm'});
out.summary=table(widths(:),fr,er,low,'VariableNames',{'width_rad_s','frictionBandRMS_Nm','externalBandRMS_Nm','lowSpeedFrictionRMS_Nm'});
out.t=t; out.torque=ft; out.external=et; out.widths=widths; out.reconstructed=reconstructed(ix);
out.curveW=linspace(c.friction.curveRange_rad_s(1),c.friction.curveRange_rad_s(2),c.friction.curveSamples)';
for k=1:count, out.curve(:,k)=friction(out.curveW,p,n,widths(k)); end
out.note='Anchored delta-state counterfactual; held measured velocity/motor torque. Missing RX updates cannot be recovered.';
end

function out=replay(d,c)
s=d.segment; dt=d.dt; w=s.velocity_filtered_rad_s; det=detector(w,dt,c); f=filters(dt,c);
notched=filter(f.nB,f.nA,s.external_torque_est_Nm); enabled=s.control_enabled;
base=enabled.*(s.K_R.*s.external_torque_est_Nm+s.K_F.*s.friction_model_Nm);
extra=[zeros(size(w)),enabled.*s.K_R.*det.gamma.*(notched-s.external_torque_est_Nm), ...
    -enabled.*det.gamma*c.controller.damping.*det.bp];
u=base+extra; cmd=max(-d.meta.torque_limit_Nm,min(d.meta.torque_limit_Nm,u));
for k=1:3
    M(k)=metrics(s.t,w,s.external_torque_est_Nm,base,extra(:,k),det.gamma,cmd(:,k),u(:,k),dt,c); %#ok<AGROW>
end
out=struct('t',s.t,'w',w,'detector',det,'extra',extra,'unsaturated',u,'command',cmd, ...
    'power',extra.*w,'loggedCommand',s.torque_cmd_Nm,'dt',dt,'summary',struct2table(M));
out.summary.mode={'none';'notch';'damping'};
out.baselineCommandErrorRMS_Nm=rms(cmd(:,1)-s.torque_cmd_Nm);
end

function modes=selectedModes(c)
if strcmp(c.controller.mode,'compare'), modes={'none','smooth','notch','damping'};
elseif strcmp(c.controller.mode,'none')
    modes={'none','smooth'};
else
    modes={'none','smooth',c.controller.mode};
end
end

function out=simulate(c,mode,voluntary)
s=c.sim; dt=s.dt_s; t=(0:dt:s.duration_s)'; N=numel(t); f=filters(dt,c);
w=zeros(N,1); theta=w; ext=w; tr=w; extra=w; u=w; cmd=w; applied=w; appliedExtra=w;
A=w; gamma=w; human=w; wf=w; observerFriction=w; plantFriction=w; feedbackTorque=w;
noise=zeros(N,2);
if c.noise.enabled
    z=c.noise.samples; assert(size(z,1)>32 && c.noise.block_s>0,'Invalid noise samples/block.');
    stream=RandStream('mt19937ar','Seed',c.noise.seed);
    L=min(size(z,1),max(2,round(c.noise.block_s/c.noise.dt_s)));
    M=ceil(t(end)/c.noise.dt_s)+1; sampled=zeros(M+L,2);
    for b=1:L:M
        start=randi(stream,size(z,1)-L+1);
        sampled(b:b+L-1,:)=z(start:start+L-1,:);
    end
    noise=interp1((0:size(sampled,1)-1)'*c.noise.dt_s,sampled,t,'previous');
end
bpState=zeros(2,1); notchState=bpState;
theta(1)=s.initialTheta_rad; w(1)=s.initialVelocity_rad_s;
state=0; variance=0; motor=0; motorExtra=0;
lag=s.delay_s/dt; lag0=floor(lag); fraction=lag-lag0;
alphaQ=1-exp(-2*pi*s.Q_Hz*dt); alphaV=1-exp(-2*pi*s.velocityLPF_Hz*dt);
alphaA=1-exp(-dt/c.controller.rmsTau_s);
if s.motorTau_s==0, alphaM=1; else, alphaM=1-exp(-dt/s.motorTau_s); end
Kh=s.Kh; Dh=s.Dh;
if voluntary, Kh=s.transparencyKh; Dh=s.transparencyDh; theta(1)=s.thetaHand_rad; w(1)=0; end
width=0; if strcmp(mode,'smooth'), width=c.friction.closedLoopWidth_rad_s; end
for i=1:N
    raw=quantize(w(i)+noise(i,1),s.velocityQuantum_rad_s);
    if i==1, wf(i)=raw; else, wf(i)=wf(i-1)+alphaV*(raw-wf(i-1)); end
    fr=friction(wf(i),s.observerPositive,s.observerNegative,width);
    observerFriction(i)=fr; feedbackTorque(i)=quantize(motor+noise(i,2),s.torqueQuantum_Nm);
    input=feedbackTorque(i)-fr+s.observerJ*2*pi*s.Q_Hz*wf(i);
    if i==1, state=s.observerJ*2*pi*s.Q_Hz*wf(i); else, state=state+alphaQ*(input-state); end
    ext(i)=s.observerJ*2*pi*s.Q_Hz*wf(i)-state;
    [bp,bpState]=filter(f.bpB,f.bpA,wf(i),bpState);
    [nt,notchState]=filter(f.nB,f.nA,ext(i),notchState);
    variance=variance+alphaA*(bp*bp-variance); A(i)=sqrt(max(0,variance));
    prev=0; if i>1, prev=gamma(i-1); end
    gamma(i)=gainStep(A(i),prev,dt,c.controller);
    tr(i)=s.Kr*ext(i);
    if strcmp(mode,'notch'), extra(i)=s.Kr*gamma(i)*(nt-ext(i)); end
    if strcmp(mode,'damping'), extra(i)=-gamma(i)*c.controller.damping*bp; end
    base=tr(i)+s.Kf*fr; u(i)=base+extra(i); cmd(i)=clip(u(i),s.limit_Nm);
    % Difference of saturated commands isolates incremental actuator contribution.
    incremental=cmd(i)-clip(base,s.limit_Nm);
    if i==1, incr=zeros(N,1); end
    incr(i)=incremental;
    j=i-lag0; delayed=(1-fraction)*history(cmd,j)+fraction*history(cmd,j-1);
    delayedExtra=(1-fraction)*history(incr,j)+fraction*history(incr,j-1);
    motor=motor+alphaM*(delayed-motor); motorExtra=motorExtra+alphaM*(delayedExtra-motorExtra);
    applied(i)=motor; appliedExtra(i)=motorExtra;
    drive=0; if voluntary, drive=s.transparencyTorque_Nm*sin(2*pi*s.transparencyHz*t(i)); end
    human(i)=-Kh*(theta(i)-s.thetaHand_rad)-Dh*w(i)+drive;
    if i<N
        x=theta(i); v=w(i); h=dt/s.substeps; impulse=0;
        for j=1:s.substeps
            net=motor-Kh*(x-s.thetaHand_rad)-Dh*v+drive;
            % Proximal dry friction: exact sticking when impulse is below static bound.
            % No discontinuous sign force is explicitly integrated through zero.
            oldV=v; trial=v+h*net/s.J;
            if trial>=0, p=s.plantPositive; else, p=s.plantNegative; end
            F=s.plantFrictionScale*(p(1)+(p(2)-p(1))*exp(-(v/p(3))^2));
            v=sign(trial)*max(0,abs(trial)-h*F/s.J)/(1+h*s.plantFrictionScale*p(4)/s.J);
            impulse=impulse+h*net-s.J*(v-oldV);
            x=x+h*v;
        end
        w(i+1)=v; theta(i+1)=x; plantFriction(i)=impulse/dt;
    end
end
assert(all(isfinite(w)) && all(isfinite(ext)),'Nonfinite closed-loop state: reduce dt or revise parameters.');
ix=t>=s.discard_s;
M=metrics(t(ix),w(ix),ext(ix),tr(ix),extra(ix),gamma(ix),cmd(ix),u(ix),dt,c);
onset=find(gamma>0,1); M.activationTime_s=NaN;
if ~isempty(onset) && ismember(mode,{'notch','damping'}), M.activationTime_s=t(onset); end
M.appliedExtraPower_W=mean(appliedExtra(ix).*w(ix));
M.appliedExtraEnergy_J=trapz(t(ix),appliedExtra(ix).*w(ix));
X=[sin(2*pi*s.transparencyHz*t(ix)),cos(2*pi*s.transparencyHz*t(ix)),ones(sum(ix),1)];
coef=X\w(ix); M.lowFrequencyMobility_rad_s_Nm=hypot(coef(1),coef(2))/s.transparencyTorque_Nm;
M.lowFrequencyPhase_deg=atan2d(coef(2),coef(1));
if ~voluntary
    M.lowFrequencyMobility_rad_s_Nm=NaN; M.lowFrequencyPhase_deg=NaN;
end
out=struct('t',t,'w',w,'theta',theta,'external',ext,'rfob',tr,'extra',extra,'command',cmd, ...
    'unsaturated',u,'applied',applied,'appliedExtra',appliedExtra,'amplitude',A,'gamma',gamma, ...
    'humanTorque',human,'measuredVelocity',wf,'observerFriction',observerFriction, ...
    'plantFriction',plantFriction,'feedbackTorque',feedbackTorque,'measurementNoise',noise, ...
    'metrics',M,'mode',mode,'voluntary',voluntary,'dt',dt);
out.measurementMetrics=metrics(t(ix),wf(ix),ext(ix),tr(ix),extra(ix),gamma(ix),cmd(ix),u(ix),dt,c);
end

function y=history(x,j)
if j<1, y=0; else, y=x(j); end
end
function y=quantize(x,q)
if q>0, y=q*round(x/q); else, y=x; end
end
function y=clip(x,limit)
y=max(-limit,min(limit,x));
end

function m=metrics(t,w,e,r,extra,gamma,cmd,u,dt,c)
s=spectra(w,dt,c); band=c.controller.bandHz; mask=s.f>=band(1)&s.f<=band(2);
[p,j]=max(s.P(mask)); freq=s.f(mask);
if isempty(j) || p<=eps, peakHz=NaN; else, peakHz=freq(j); end
activation=find(gamma>0,1); if isempty(activation), onset=NaN; else, onset=t(activation); end
mid=floor(numel(w)/2); early=bandRMS(w(1:mid),dt,band,c); late=bandRMS(w(mid+1:end),dt,band,c);
if early>eps, sustained=late/early; else, sustained=NaN; end
if isnan(peakHz), phase=NaN; else
    X=[sin(2*pi*peakHz*t),cos(2*pi*peakHz*t),ones(size(t))];
    cw=X\w; cr=X\r; phase=mod(atan2d(cr(2),cr(1))-atan2d(cw(2),cw(1))+180,360)-180;
end
m=struct('bandRMS',bandRMS(w,dt,band,c),'peakHz',peakHz,'peakPSD',p, ...
    'maximumSpeed',max(abs(w)),'externalRMS_Nm',rms(e),'rfobRMS_Nm',rms(r), ...
    'rfobPhaseRelativeVelocity_deg',phase,'sustainedRatio',sustained, ...
    'activationTime_s',onset,'extraRMS_Nm',rms(extra),'saturationFraction',mean(abs(u-cmd)>eps), ...
    'extraPower_W',mean(extra.*w),'extraEnergy_J',trapz(t,extra.*w), ...
    'positivePowerFraction',mean(extra.*w>0));
end

function T=summaryTable(r,modes)
M=cellfun(@(s)s.metrics,r); T=struct2table(M); T.mode=modes(:); T=movevars(T,'mode','Before',1);
end

function c=setPath(c,path,value)
% Sweep-only scalar aliases for the two band edges.
if strcmp(path,'controller.bandLowHz'), c.controller.bandHz(1)=value; return; end
if strcmp(path,'controller.bandHighHz'), c.controller.bandHz(2)=value; return; end
f=strsplit(path,'.'); assert(numel(f)==2 && isfield(c,f{1}) && isfield(c.(f{1}),f{2}),'Invalid sweep path: %s',path);
c.(f{1}).(f{2})=value;
end

function out=parameterSweep(c)
v=c.sweep.values; assert(numel(v)==2 && numel(c.sweep.axes)==2,'Sweep needs exactly two axes.');
assert(numel(v{1})*numel(v{2})<=c.sweep.maxRuns,'Sweep exceeds maxRuns.');
R=nan(numel(v{1}),numel(v{2})); mobility=R; rows=cell(numel(R),1); n=0;
for i=1:numel(v{1})
    for j=1:numel(v{2})
        cc=setPath(setPath(c,c.sweep.axes{1},v{1}(i)),c.sweep.axes{2},v{2}(j)); validateConfig(cc);
        a=simulate(cc,c.sweep.mode,false); b=simulate(cc,c.sweep.mode,true);
        R(i,j)=a.metrics.bandRMS; mobility(i,j)=b.metrics.lowFrequencyMobility_rad_s_Nm;
        n=n+1; rows{n}=a.metrics; rows{n}.axis1=v{1}(i); rows{n}.axis2=v{2}(j); rows{n}.mobility=mobility(i,j);
    end
end
out=struct('rms',R,'mobility',mobility,'axes',{c.sweep.axes},'values',{v},'metrics',struct2table(vertcat(rows{:})));
end

function out=robustness(c)
rows={}; modes=selectedModes(c);
for K=c.robustness.Kh
    for D=c.robustness.Dh
        for delay=c.robustness.delay_s
            cc=c; cc.sim.Kh=K; cc.sim.Dh=D; cc.sim.delay_s=delay; validateConfig(cc);
            for j=1:numel(modes)
                r=simulate(cc,modes{j},false); row=r.metrics; row.Kh=K; row.Dh=D; row.delay_s=delay; row.mode=modes{j}; rows{end+1}=row; %#ok<AGROW>
            end
        end
    end
end
out=struct2table(vertcat(rows{:})); disp(out);
end

function figs=drawFigures(r,logs,c)
figs=gobjects(0); names={'Velocity spectra: three experimental conditions', ...
    'Velocity and RFOB torque spectra','Oscillation-amplitude detector, evaluated on existing logs', ...
    'Friction smoothing comparison','Filter frequency responses','Replay suppression comparison', ...
    'Closed-loop oscillation suppression','Closed-loop PSD comparison', ...
    'Suppression vs transparency','Parameter sweep maps'};
required={'spectra','spectra','detector','friction','','replay','closed','closed','closed','sweep'};
for id=1:10
    if c.baselineOnly && id==9, continue; end
    if ~c.figures(id), continue; end
    if ~isempty(required{id}) && ~isfield(r,required{id})
        fprintf('Figure %d skipped: %s analysis was not run.\n',id,required{id}); continue;
    end
    h=figure(id); clf(h); set(h,'Name',names{id},'Visible',c.visible,'Color','w'); figs(end+1)=h; %#ok<AGROW>
    switch id
        case 1
            hold on;
            for k=1:3, s=r.spectra{k}; plot(s.f,s.P,'LineWidth',c.plot.lineWidth); end
            set(gca,'YScale','log'); xlim(c.spectrum.displayHz); grid on;
            xlabel('Frequency (Hz)'); ylabel('Velocity PSD ((rad/s)^2/Hz)'); legend(c.labels,'Location','best'); title(names{id});
        case 2
            tiledlayout(3,1);
            series={r.spectra,r.torqueSpectra,r.rfobSpectra}; labels={'Velocity ((rad/s)^2/Hz)','External torque (Nm^2/Hz)','RFOB torque (Nm^2/Hz)'};
            for j=1:3
                nexttile; hold on;
                for k=1:3, s=series{j}{k}; plot(s.f,s.P); end
                set(gca,'YScale','log'); xlim(c.spectrum.detailHz); ylabel(labels{j}); grid on;
            end
            xlabel('Frequency (Hz)'); legend(c.labels); sgtitle(names{id});
        case 3
            tiledlayout(3,1);
            for k=1:3
                nexttile; d=r.detector{k}; plot(logs{k}.segment.t,d.amplitude); hold on;
                yline(c.controller.on_rad_s,'--','On'); yline(c.controller.full_rad_s,'--','Full');
                ylabel('RMS (rad/s)'); title(c.labels{k}); grid on;
            end
            xlabel('Log time (s)'); sgtitle(names{id});
        case 4
            d=r.friction{3}; labels=compose('width = %g rad/s',d.widths);
            tiledlayout(3,2);
            nexttile; plot(d.curveW,d.curve); xlabel('Velocity (rad/s)'); ylabel('Friction (Nm)'); legend(labels,'Location','best'); grid on;
            nexttile; plot(d.t,d.torque); xlabel('Log time (s)'); ylabel('Friction (Nm)'); grid on;
            nexttile; hold on;
            for k=1:numel(d.widths), s=spectra(d.torque(:,k),logs{3}.dt,c); plot(s.f,s.P); end
            set(gca,'YScale','log'); xlim(c.spectrum.displayHz); xlabel('Frequency (Hz)'); ylabel('Friction PSD (Nm^2/Hz)'); grid on;
            nexttile; plot(d.widths,[d.summary.frictionBandRMS_Nm d.summary.externalBandRMS_Nm],'-o');
            xlabel('Smoothing width (rad/s)'); ylabel('Band RMS (Nm)'); legend('Friction','RFOB estimate'); title(sprintf('%g-%g Hz',c.spectrum.metricHz)); grid on;
            nexttile; plot(d.t,d.external); xlabel('Log time (s)'); ylabel('RFOB estimate (Nm)'); grid on;
            nexttile; plot(d.widths,d.summary.lowSpeedFrictionRMS_Nm,'-o'); xlabel('Smoothing width (rad/s)'); ylabel('Low-speed friction RMS (Nm)'); grid on;
            sgtitle('Friction: fixed trajectory counterfactual, not vibration suppression');
        case 5
            f=filters(c.sim.dt_s,c); [bp,hz]=freqz(f.bpB,f.bpA,c.plot.responsePoints,1/c.sim.dt_s); nt=freqz(f.nB,f.nA,c.plot.responsePoints,1/c.sim.dt_s);
            tiledlayout(2,1); nexttile; plot(hz,20*log10(max(abs([bp nt]),eps))); xlim(c.spectrum.displayHz); ylabel('Gain (dB)'); legend('BPF','Notch'); grid on;
            nexttile; plot(hz,unwrap(angle([bp nt]))*180/pi); xlim(c.spectrum.displayHz); ylabel('Phase (deg)'); xlabel('Frequency (Hz)'); grid on;
            sgtitle('Causal bilinear filters (center prewarped)');
        case 6
            d=r.replay; tiledlayout(3,2);
            nexttile; plot(d.t,d.extra); ylabel('Extra torque (Nm)'); legend('Baseline','Notch','Damping'); grid on;
            nexttile; plot(d.t,d.command); hold on; plot(d.t,d.loggedCommand,':k'); ylabel('Command (Nm)'); grid on;
            nexttile; hold on;
            for k=1:3, s=spectra(d.command(:,k),d.dt,c); plot(s.f,s.P); end
            set(gca,'YScale','log'); xlim(c.spectrum.displayHz); xlabel('Frequency (Hz)'); ylabel('Command PSD (Nm^2/Hz)'); grid on;
            nexttile; plot(d.t,d.detector.amplitude); hold on; yline(c.controller.on_rad_s,'--'); yline(c.controller.full_rad_s,'--'); ylabel('Amplitude (rad/s)'); grid on;
            nexttile; plot(d.t,d.detector.gamma); ylabel('Gain gamma'); xlabel('Log time (s)'); grid on;
            nexttile; plot(d.t,d.power); ylabel('Extra torque * velocity (W)'); xlabel('Log time (s)'); grid on;
            sgtitle('Replay: measured motion fixed; saturation and power in metrics table');
        case 7
            tiledlayout(6,1); fields={'w','external','rfob','extra','amplitude','gamma'};
            labels={'Velocity (rad/s)','External (Nm)','RFOB (Nm)','Extra (Nm)','RMS (rad/s)','Gamma'}; ax=gobjects(6,1);
            for j=1:6
                ax(j)=nexttile; hold on;
                for k=1:numel(r.closed), d=r.closed{k}; plot(d.t,d.(fields{j})); end
                ylabel(labels{j}); grid on;
            end
            linkaxes(ax,'x'); xlabel('Time (s)'); legend(r.modes,'Location','best');
            sgtitle(modelTitle(r,names{id}));
        case 8
            tiledlayout(2,1);
            for j=1:2
                nexttile; hold on;
                for k=1:numel(r.closed)
                    d=r.closed{k}; ix=d.t>=c.sim.discard_s;
                    if j==1, x=d.w(ix); else, x=d.command(ix); end
                    s=spectra(x,d.dt,c); plot(s.f,s.P);
                end
                set(gca,'YScale','log'); xlim(c.spectrum.displayHz); grid on;
                if j==1, ylabel('Velocity PSD ((rad/s)^2/Hz)'); else, ylabel('Command PSD (Nm^2/Hz)'); end
            end
            legend(r.modes); xlabel('Frequency (Hz)'); sgtitle(modelTitle(r,names{id}));
        case 9
            tiledlayout(2,1); nexttile; hold on;
            for k=1:numel(r.closed)
                x=r.transparency{k}.metrics.lowFrequencyMobility_rad_s_Nm; y=r.closed{k}.metrics.bandRMS;
                plot(x,y,'o'); text(x,y,[' ' r.modes{k}]);
            end
            xlabel('Low-frequency mobility amplitude ((rad/s)/Nm), higher = easier motion'); ylabel('Grip band RMS (rad/s)'); grid on;
            nexttile; hold on;
            for k=1:numel(r.transparency), d=r.transparency{k}; plot(d.t,d.w); end
            xlabel('Time (s), separate voluntary force test'); ylabel('Velocity (rad/s)'); legend(r.modes); grid on;
            sgtitle(modelTitle(r,'Assumed free-rotation impedance; not identified human transparency'));
        case 10
            d=r.sweep; tiledlayout(1,2);
            nexttile; imagesc(d.values{2},d.values{1},d.rms); axis xy; colorbar; title('Grip velocity band RMS (rad/s)');
            xlabel(d.axes{2},'Interpreter','none'); ylabel(d.axes{1},'Interpreter','none');
            nexttile; imagesc(d.values{2},d.values{1},d.mobility); axis xy; colorbar; title('Low-frequency mobility ((rad/s)/Nm)');
            xlabel(d.axes{2},'Interpreter','none'); ylabel(d.axes{1},'Interpreter','none');
    end
end
if c.showFFT
    if ~isfield(r,'spectra'), fprintf('FFT figure skipped: spectral analysis is OFF.\n'); return; end
    h=figure(11); clf(h); set(h,'Visible',c.visible,'Name','FFT amplitude (not PSD)','Color','w'); figs(end+1)=h;
    hold on;
    for k=1:3
        x=logs{k}.segment.velocity_filtered_rad_s; N=numel(x); win=hann(N,'periodic');
        v=abs(fft((x-mean(x)).*win))/sum(win); v=v(1:floor(N/2)+1);
        if rem(N,2)==0, v(2:end-1)=2*v(2:end-1); else, v(2:end)=2*v(2:end); end
        plot((0:floor(N/2))'/(N*logs{k}.dt),v);
    end
    xlim(c.spectrum.displayHz); xlabel('Frequency (Hz)'); ylabel('One-sided peak amplitude (rad/s)');
    title('FFT: full selected run, periodic Hann, coherent-gain normalization, df = 1/(N dt)'); legend(c.labels); grid on;
end
end

function titleText=modelTitle(r,s)
if r.validation.reproduced, suffix=' (baseline screening passed; model prediction)';
else, suffix=' (baseline NOT validated; exploratory model only)'; end
titleText=[s suffix];
end





