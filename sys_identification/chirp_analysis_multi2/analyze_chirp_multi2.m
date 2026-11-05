% Analyze ten concatenated logarithmic chirp sweeps in a telemetry CSV.
% Start is detected from command_Nm; each sweep ends at the first recorded
% zero point of chirp_Nm following active command torque.
root = fileparts(mfilename('fullpath'));
projectRoot = fileparts(root);
inputFile = fullfile(projectRoot,'captures','MC0001_20261105_101724.csv');
outputRoot = fullfile(root,'results');
if ~isfolder(outputRoot), mkdir(outputRoot); end
out = fullfile(outputRoot,char(datetime('now','Format','yyyyMMdd_HHmmss_SSS')));
while isfolder(out), out=[out '_new']; end
mkdir(out);

T = readtable(inputFile,'VariableNamingRule','preserve');
required = {'index','actual_us','phase','position_deg','speed_rad_s', ...
    'chirp_Nm','pd_Nm','command_Nm'};
assert(all(ismember(required,T.Properties.VariableNames)), ...
    'Input CSV is missing required telemetry columns.');
assert(all(isfinite(T{:,required}),'all'),'Input contains missing/nonfinite telemetry.');

cmd = T.command_Nm;
chirp = T.chirp_Nm;
zeroTol = 1e-10;
starts = zeros(0,1); ends = zeros(0,1); terminalZeroObserved = false(0,1);
cursor = 1;
while cursor <= height(T)
    % The first nonzero commanded torque after a zero-torque rest marks start.
    rel = find(abs(cmd(cursor:end)) > zeroTol,1,'first');
    if isempty(rel), break; end
    s = cursor + rel - 1;
    % Require at least one chirp input sample before accepting an endpoint.
    nz = find(abs(chirp(s:end)) > zeroTol,1,'first');
    assert(~isempty(nz),'Command torque started without a chirp input.');
    k = s + nz - 1;
    e = NaN;
    while k <= height(T)
        if abs(chirp(k)) <= zeroTol && abs(cmd(k)) <= zeroTol && ...
                abs(chirp(k-1)) > zeroTol
            e = k; % First recorded terminal zero after active chirp torque.
            break
        else
            k = k + 1;
        end
    end
    observed = isfinite(e);
    if ~observed
        expectedEndIndex=T.index(s)+round(20*2000);
        assert(numel(starts)==9 && T.index(s)==T.index(starts(end))+46000 && ...
            T.index(end)==expectedEndIndex-1 && height(T)-s+1==40000, ...
            'No recorded zero endpoint found after sample %d.',T.index(s));
        break % Ignore the incomplete final sweep; only measured zero endpoints are analyzed.
    end
    starts(end+1,1)=s; ends(end+1,1)=e; terminalZeroObserved(end+1,1)=observed; %#ok<SAGROW>
    cursor=e+1;
end
assert(numel(starts)==9,'Expected 9 sweeps with recorded zero endpoints, detected %d.',numel(starts));

fs = 1/median(diff(T.actual_us))*1e6;
assert(abs(fs-2000)<1,'Unexpected sample rate: %.3f Hz.',fs);
f0=1; f1=100; duration=20; klog=log(f1/f0)/duration;
freq=logspace(log10(f0),log10(f1),100)';
nTrial=numel(starts);
fitC=struct('Kp',0.00834*180/pi,'Kd',0.1,'f0',f0,'f1',f1, ...
    'k',klog,'T',duration);
fitOptions=qm.defaults(struct('maxIterations',350));
fitTrials=cell(nTrial,1);
summary=table('Size',[nTrial 9], ...
    'VariableTypes',{'double','double','double','double','double','double','double','double','logical'}, ...
    'VariableNames',{'trial','start_index','end_index','samples','duration_s', ...
    'start_command_Nm','end_chirp_Nm','peak_abs_command_Nm','terminal_zero_observed'});

for tr=1:nTrial
    s=starts(tr); e=ends(tr); idx=s:e;
    d=T(idx,:);
    t=(d.index-d.index(1))/fs;
    u=d.chirp_Nm;
    fitRows=idx(d.phase==1);
    fitQ=T.position_deg(fitRows)*pi/180;
    fitV=T.speed_rad_s(fitRows);
    fitPD=T.pd_Nm(fitRows);
    q0Fit=median(fitQ+(fitPD+fitC.Kd*fitV)/fitC.Kp);
    fitD.t=(T.actual_us(fitRows)-T.actual_us(fitRows(1)))*1e-6;
    fitD.x=fitQ-q0Fit; fitD.v=fitV; fitD.u=T.chirp_Nm(fitRows);
    fitD.phase=ones(size(fitD.u));
    fitD.nominalChirpTime=(T.index(fitRows)-T.index(fitRows(1)))/fs;
    fitD.initial=[fitD.x(1);fitD.v(1)];
    [dtUs,~,fitD.dtGroup]=unique(round(diff(fitD.t)*1e6));
    fitD.uniqueDt=dtUs*1e-6;
    fitTrials{tr}=struct('file',sprintf('trial_%02d',tr),'c',fitC,'d',fitD);
    assert(numel(idx)>=round(0.95*duration*fs),'Trial %d is unexpectedly short.',tr);
    writetable(d,fullfile(out,sprintf('trial_%02d_trimmed.csv',tr)));
    active=find(abs(u)>zeroTol,1,'last');
    sweepDuration=t(active);
    summary{tr,1:8}=[tr,T.index(s),T.index(e),numel(idx),sweepDuration,cmd(s),chirp(e),max(abs(cmd(idx)))];
    summary.terminal_zero_observed(tr)=terminalZeroObserved(tr);
end
writetable(summary,fullfile(out,'sweep_summary.csv'));

% Match the source code's four-term chirp regression and repetition-weighted H1.
U=complex(nan(numel(freq),nTrial)); Y=U;
for tr=1:nTrial
    d=fitTrials{tr}.d;
    F=qm.regression(fitTrials{tr},[d.u d.x],fitOptions);
    U(:,tr)=F.C(:,1); Y(:,tr)=F.C(:,2);
end
S=qm.spectra(U,Y); H=Y./U;
frf=table(freq,real(S.H1),imag(S.H1),abs(S.H1),angle(S.H1)*180/pi,S.coherence,S.count, ...
    'VariableNames',{'frequency_Hz','H1_real_rad_per_Nm','H1_imag_rad_per_Nm', ...
    'H1_gain_rad_per_Nm','H1_phase_deg','coherence','validTrials'});
writetable(frf,fullfile(out,'frf_ensemble.csv'));
perTrial=table(repmat(freq,nTrial,1),repelem((1:nTrial)',numel(freq)), ...
    real(H(:)),imag(H(:)),abs(H(:)),angle(H(:))*180/pi, ...
    'VariableNames',{'frequency_Hz','trial','H_real_rad_per_Nm','H_imag_rad_per_Nm','gain_rad_per_Nm','phase_deg'});
writetable(perTrial,fullfile(out,'frf_by_trial.csv'));

% Equal-trial-weight fit using the source program's exact ZOH model/cost.
[jb,jBStarts]=qm.fit(fitTrials,fitOptions);
jBResult=table(jb(1),jb(2),min(jBStarts.cost), ...
    'VariableNames',{'J_kg_m2','B_Nm_s_per_rad','normalized_angle_mse'});
writetable(jBResult,fullfile(out,'j_b_estimate.csv'));
writetable(jBStarts,fullfile(out,'j_b_multistart.csv'));
delay=qm.delay(fitTrials,jb,fitOptions,false);
jBDelay=table(jb(1),jb(2),delay.frf_s,delay.time_s, ...
    'VariableNames',{'J_kg_m2','B_Nm_s_per_rad','Td_FRF_s','Td_time_s'});
writetable(jBDelay,fullfile(out,'j_b_delay_summary.csv'));
writetable(delay.frfScan,fullfile(out,'delay_frf_scan.csv'));
writetable(delay.timeScan,fullfile(out,'delay_time_scan.csv'));
qsummary=qm.summaryAnalysis(fitTrials,fitOptions,out);
qm.summaryFigures(qsummary);
save(fullfile(out,'analysis.mat'),'inputFile','starts','ends','terminalZeroObserved','summary','freq','U','Y','H','S','frf','perTrial','jBResult','jBStarts','delay','jBDelay','qsummary','fs','zeroTol','-v7.3');
fprintf('Analyzed %d sweeps at %.2f Hz. J=%.6g kg m^2, B=%.6g Nm s/rad, Td_FRF=%.6g s, Td_time=%.6g s. Results: %s\n', ...
    nTrial,fs,jb(1),jb(2),delay.frf_s,delay.time_s,out);
