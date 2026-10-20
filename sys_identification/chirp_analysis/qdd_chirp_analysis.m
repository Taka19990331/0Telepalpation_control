function r = qdd_chirp_analysis(dataName, opt)
%QDD_CHIRP_ANALYSIS CSV-only experiment settings, base MATLAB implementation.
% Continuous closed-loop model, recorded chirp input, fixed measured x(0),v(0).
% No Control, Signal, Optimization or System Identification Toolbox required.
if nargin < 2, opt = struct; end
defaults = struct('makeFigures',true,'saveResults',true,'frfCycles',4, ...
    'frfMaxRelativeBandwidth',0.35,'frfPoints',100,'maxIterations',500);
names = fieldnames(defaults);
for k=1:numel(names)
    if ~isfield(opt,names{k}), opt.(names{k})=defaults.(names{k}); end
end
root = fileparts(mfilename('fullpath'));
[folder,base,ext] = fileparts(char(dataName));
if isempty(ext), ext='.csv'; end
assert(strcmpi(ext,'.csv'),'Expected a CSV file.');
if isempty(folder), folder=fullfile(fileparts(root),'captures'); end
file = fullfile(folder,[base ext]);
[raw,meta] = readCapture(file);
fprintf('\n=== Experiment conditions: %s ===\n',file);
disp(meta);
c = conditions(meta);
fprintf('Kp = %.10g Nm/rad; Kd = %.10g Nm*s/rad; q0 = %.10g rad\n',c.Kp,c.Kd,c.q0);
fprintf('Installed products (not necessarily licensed):\n');
products = ver; disp(struct2table(products));
[d,checks] = prepare(raw,c,opt);
[fit,starts] = identify(d,c,opt);
pred = simulate(fit.J,fit.B,c,d);
residual = d.x-pred(:,1);
fit.RMSE = sqrt(mean(residual.^2));
fit.fitPercent = 100*(1-norm(residual)/norm(d.x-mean(d.x)));
fit.velocityRMSE = sqrt(mean((d.v-pred(:,2)).^2));
fit.fn = sqrt(c.Kp/fit.J)/(2*pi);
fit.zeta = (fit.B+c.Kd)/(2*sqrt(fit.J*c.Kp));
fit.fitDefinition = '100*(1-norm(x-x_model)/norm(x-mean(x)))';
fit.initialState = [d.x(1);d.v(1)];
fit.interpretation = 'Command-equivalent output-coordinate parameters; torque gain not calibrated.';
frf = localFRF(d,c,pred,opt);
ww=2*pi*frf.frequency_Hz;
hh=1./(c.Kp-fit.J*ww.^2+1i*(fit.B+c.Kd)*ww);
frf.modelGain_dB=20*log10(abs(hh));
frf.modelPhase_deg=angle(hh)*180/pi;
frf.phaseUnwrapped_deg=phaseForPlot(frf.real_rad_per_Nm+1i*frf.imag_rad_per_Nm);
validation = frequencyValidation(frf,fit,c);
% Dimensionless sensitivity columns: relative J, B changes on damping scale.
h = 1e-3;
pJ = simulate(fit.J*exp(h),fit.B,c,d);
bScale = max([fit.B,c.Kd,sqrt(fit.J*c.Kp)*0.01]);
pB = simulate(fit.J,fit.B+h*bScale,c,d);
S = [(pJ(:,1)-pred(:,1))/h,(pB(:,1)-pred(:,1))/h];
sv = svd(S,0);
fit.sensitivityCondition = sv(1)/max(sv(end),realmin);
fit.sensitivityParameterScales = [fit.J,bScale];
% B profile: re-optimize J; this is a cost diagnostic, NOT a confidence interval.
bGrid = unique([0,fit.B,bScale*[0.01 0.03 0.1 0.3 1 3]]);
profileCost = zeros(size(bGrid)); profileJ=profileCost;
for k=1:numel(bGrid)
    [z,profileCost(k)] = fminsearch(@(z)objective(exp(z),bGrid(k),c,d), ...
        log(fit.J),optimset('Display','off','MaxIter',opt.maxIterations));
    profileJ(k)=exp(z);
end
profile = table(bGrid(:),profileJ(:),profileCost(:), ...
    'VariableNames',{'B','J','normalizedMSE'});
if fit.B <= bScale*1e-3
    warning('QDD:BoundaryB','B is at/near zero. This does not establish negligible physical friction.');
end
near = starts.normalizedMSE <= min(starts.normalizedMSE)*1.01+1e-10;
if range(starts.B(near)) > 0.2*bScale || fit.sensitivityCondition>1e3
    warning('QDD:Identifiability','B or J is weakly determined; inspect starts and B cost profile.');
end
if fit.fitPercent<70
    warning('QDD:PoorFit','Poor second-order fit: consider friction, torque calibration, timing and unmodeled dynamics.');
end
fprintf('\n=== Identification (same-record fit, not independent validation) ===\n');
fprintf('J = %.10g kg*m^2\nB = %.10g Nm*s/rad\nKp = %.10g Nm/rad\nKd = %.10g Nm*s/rad\n',fit.J,fit.B,c.Kp,c.Kd);
fprintf('fn = %.6g Hz; zeta = %.6g\nFit = %.4f %%\nAngle RMSE = %.8g rad\n',fit.fn,fit.zeta,fit.fitPercent,fit.RMSE);
fprintf('Fit definition: %s\n',fit.fitDefinition);
fprintf('Torque absolute gain is not calibrated in CSV: J/B absolute values are conditional.\n');
fprintf('No coherence reported: a single sweep does not supply independent repeats at each frequency.\n');
fprintf('Local chirp-phase regression is approximate; rejected wide-band/edge windows are retained as flags.\n');
disp(starts); disp(profile);
r = struct('sourceCSV',file,'metadata',meta,'conditions',c,'options',opt, ...
    'checks',checks,'fit',fit,'starts',starts,'BProfile',profile, ...
    'data',d,'prediction',pred,'residual',residual,'frf',frf, ...
    'frequencyValidation',validation,'products',products,'matlabVersion',version);
figs = [];
if opt.makeFigures, figs = figuresFor(r,raw); end
if opt.saveResults
    out = fullfile(root,'results',base);
    if ~isfolder(out), mkdir(out); end
    save(fullfile(out,'analysis.mat'),'r');
    writetable(meta,fullfile(out,'csv_conditions.csv'));
    writetable(struct2table(fit,'AsArray',true),fullfile(out,'parameters.csv'));
    writetable(starts,fullfile(out,'multistart.csv'));
    writetable(profile,fullfile(out,'B_profile.csv'));
    writetable(frf,fullfile(out,'frf.csv'));
    for k=1:numel(figs)
        savefig(figs(k),fullfile(out,sprintf('figure_%d.fig',k)));
        exportgraphics(figs(k),fullfile(out,sprintf('figure_%d.png',k)),'Resolution',160);
    end
    writeReport(fullfile(out,'report.txt'),r);
    fprintf('Saved: %s\n',out);
end
end

function [T,M] = readCapture(file)
assert(isfile(file),'CSV not found: %s',file);
lines = splitlines(string(fileread(file)));
isComment = startsWith(strtrim(lines),'#');
comments = lines(isComment);
keys = strings(0,1); values=keys;
for i=1:numel(comments)
    s=char(comments(i)); eq=find(s=='=',1);
    if ~isempty(eq)
        keys(end+1,1)=strtrim(s(2:eq-1)); %#ok<AGROW>
        values(end+1,1)=strtrim(s(eq+1:end)); %#ok<AGROW>
    end
end
assert(numel(unique(keys))==numel(keys),'Duplicate metadata keys in CSV.');
M=table(keys,values,'VariableNames',{'Key','Value'});
header=find(~isComment & strlength(strtrim(lines))>0,1);
assert(~isempty(header),'CSV has no data header.');
io=detectImportOptions(file,'NumHeaderLines',header-1,'VariableNamingRule','preserve');
T=readtable(file,io);
assert(height(T)>10,'Insufficient samples.');
end

function value = textValue(M,key)
i=find(M.Key==string(key));
assert(isscalar(i),'Missing/ambiguous CSV condition: %s',key);
value=M.Value(i);
end

function x = number(M,key)
x=str2double(textValue(M,key));
assert(isfinite(x),'Non-numeric CSV condition: %s',key);
end

function c = conditions(M)
assert(textValue(M,'FORMAT')=="CHIRPSI1_CSV_WITH_METADATA_V1", ...
    'Unsupported metadata format; no experiment defaults will be substituted.');
assert(textValue(M,'CHIRP_TYPE')=="logarithmic",'Only documented logarithmic sweeps supported.');
assert(textValue(M,'REFERENCE_VALID_FOR_PD')=="True",'CSV has no valid PD reference.');
assert(textValue(M,'RESULT.valid')=="True" && textValue(M,'RESULT.complete')=="True", ...
    'Capture invalid/incomplete. Inspect CSV result metadata; no automatic fit.');
assert(number(M,'CONFIG.MotorProtocol')==0,'Only verified AK80 standard MIT coordinate convention supported.');
assert(number(M,'CONFIG.NoMotor')==0,'Synthetic/no-motor capture is not physical identification.');
assert(number(M,'CONFIG.PositionPolicy')==0,'Unknown position policy; cannot unwrap safely.');
assert(textValue(M,'PD_TORQUE_FORMULA')=="tau_PD = Kp*(theta_ref-theta) - Kd*omega", ...
    'Unrecognized PD law.');
assert(textValue(M,'POSITION_DEFINITION')=="theta_ref and theta [deg]; omega [rad/s]", ...
    'Unrecognized angle/speed units.');
assert(textValue(M,'CHIRP_TORQUE_FORMULA')=="tau_chirp(t) = A(t)*sin(2*pi*f0*(exp(k*t)-1)/k)" ...
    && textValue(M,'AMPLITUDE_FORMULA')=="A(t) = A0 + (A1-A0)*x(t)^a", ...
    'Unrecognized chirp phase/amplitude definition; cannot reconstruct or demodulate safely.');
c.fs=number(M,'SAMPLING_FREQUENCY_HZ (fs)');
c.f0=number(M,'START_FREQUENCY_HZ (f0)');
c.f1=number(M,'END_FREQUENCY_HZ (f1)');
c.T=number(M,'SWEEP_DURATION_S (T)');
c.A0=number(M,'START_AMPLITUDE_NM (A0)');
c.A1=number(M,'END_AMPLITUDE_NM (A1)');
c.a=number(M,'AMPLITUDE_EXPONENT (a)');
c.Kp=number(M,'KP_PD (Kp) [Nm/deg]')*180/pi;
c.Kd=number(M,'KD_PD (Kd) [Nm/(rad/s)]');
c.q0=number(M,'REFERENCE_POSITION_DEG (theta_ref)')*pi/180;
c.start=number(M,'CHIRP_START_RECORD_TIME_S (t_start)');
c.startIndex=number(M,'SWEEP_START_INDEX (sweep_start_index)');
c.limit=number(M,'TORQUE_LIMIT_NM (tau_limit)');
c.feedbackTimeout=number(M,'CONFIG.FeedbackTimeout_us')*1e-6;
c.maxSpeed=number(M,'CONFIG.VelocityMax_rad_s');
c.positionMin=number(M,'CONFIG.PositionMin_rad');
c.positionMax=number(M,'CONFIG.PositionMax_rad');
c.torqueEncodingMax=number(M,'CONFIG.MITTorqueMax_Nm');
c.preDuration=number(M,'PRE_HOLD_DURATION_S (T_pre)');
c.postDuration=number(M,'POST_HOLD_DURATION_S (T_post)');
assert(c.fs>0 && c.f1>c.f0 && c.f0>0 && c.f1<c.fs/2 && c.T>0 && c.Kp>0 && c.Kd>=0, ...
    'Invalid sampling, sweep or gain conditions.');
c.k=log(c.f1/c.f0)/c.T;
c.axisConvention = 'AK80 MIT position/velocity and Nm command used directly; no additional gear conversion.';
c.torqueCalibration = 'Firmware performs no physical torque calibration; output-shaft Nm interpretation is conditional.';
end

function [d,ck] = prepare(T,c,opt) %#ok<INUSD>
required={'index','nominal_us','actual_us','phase','position_deg','speed_rad_s', ...
    'pd_Nm','command_Nm','torque_limited','feedback_age_us','feedback_sequence','can_tx_accepted','motor_error'};
assert(all(ismember(required,T.Properties.VariableNames)),'Missing required telemetry columns.');
assert(all(isfinite(T{:,required}),'all'),'Missing/nonfinite telemetry: repair acquisition, not silent interpolation.');
assert(all(diff(T.index)==1),'Dropped/duplicate/out-of-order sample indices.');
t=T.actual_us*1e-6;
assert(all(diff(t)>0),'Nonmonotonic actual timestamps (wrap/reset/duplicate).');
assert(max(abs(T.nominal_us*1e-6-T.index/c.fs))<1/c.fs/10,'Nominal timestamps disagree with CSV fs.');
sweep=T.phase==1; active=T.phase==1 | T.phase==2;
ii=find(active); sw=find(sweep);
assert(~isempty(sw) && all(diff(ii)==1) && all(diff(sw)==1),'Noncontiguous/missing sweep or PD interval.');
assert(T.index(sw(1))==c.startIndex && abs(t(sw(1))-c.start)<1/c.fs,'CSV sweep start mismatch.');
assert(numel(sw)==round(c.T*c.fs),'Sweep length differs from CSV duration.');
assert(sum(T.phase==2)==round(c.postDuration*c.fs),'PD tail length differs from CSV duration.');
assert(all(ismember(T.phase,[0 1 2 3])),'Unknown phase value.');
q=T.position_deg*pi/180; v=T.speed_rad_s;
assert(all(q>c.positionMin & q<c.positionMax),'Position at/outside encoded range.');
assert(all(abs(v)<=c.maxSpeed+1e-6),'Velocity outside encoded range.');
assert(all(T.command_Nm(T.phase==0)==0) && all(T.pd_Nm(T.phase==0)==0), ...
    'Initial averaging interval is not zero torque with PD disabled.');
% Multi-turn position has no 2*pi wrapping. Unwrap would corrupt valid motion.
maxStep=c.maxSpeed*(diff(t)+max(T.feedback_age_us)*1e-6)+0.01;
assert(all(abs(diff(q))<=maxStep),'Discontinuous multi-turn angle: cannot infer a valid wrap correction.');
phaseTime=(T.index-c.startIndex)/c.fs;
f=c.f0*exp(c.k*max(0,min(c.T,phaseTime)));
amp=c.A0+(c.A1-c.A0)*max(0,min(1,(f-c.f0)/(c.f1-c.f0))).^c.a;
phi=2*pi*c.f0*expm1(c.k*phaseTime)/c.k;
reconstructed=zeros(height(T),1);
reconstructed(sweep)=amp(sweep).*sin(phi(sweep));
if ismember('chirp_Nm',T.Properties.VariableNames)
    u=T.chirp_Nm;
    assert(all(isfinite(u)),'Nonfinite chirp input.');
    ck.chirpSource='recorded chirp_Nm';
    ck.chirpReconstructionMaxError=max(abs(u-reconstructed));
    assert(ck.chirpReconstructionMaxError<0.02,'Recorded chirp inconsistent with CSV formula/settings.');
else
    u=reconstructed; ck.chirpSource='reconstructed from CSV logarithmic chirp formula, zero initial phase';
    ck.chirpReconstructionMaxError=0;
end
expectedPD=zeros(height(T),1);
expectedPD(active)=-c.Kp*(q(active)-c.q0)-c.Kd*v(active);
ck.PDMaxError=max(abs(T.pd_Nm(active)-expectedPD(active)));
assert(ck.PDMaxError<1e-4,'Recorded PD does not match CSV law/units.');
ck.limitedRows=sum(T.torque_limited~=0);
ck.commandSumMaxError=max(abs(T.command_Nm(active)-u(active)-T.pd_Nm(active)));
assert(ck.limitedRows==0 && all(abs(T.command_Nm(active))<c.limit+1e-5) ...
    && ck.commandSumMaxError<1e-4,'Torque limiting/command override invalidates closed-loop model.');
assert(all(T.can_tx_accepted(active)==1) && all(T.motor_error(active)==0),'CAN transmission/motor fault.');
assert(all(T.feedback_age_us(active)*1e-6<c.feedbackTimeout),'Stale motor feedback.');
assert(all(diff(T.feedback_sequence(active))>=0),'Feedback sequence reset.');
ck.repeatedFeedbackRows=sum(diff(T.feedback_sequence(active))==0);
ck.feedbackAgeMedian_s=median(T.feedback_age_us(active))*1e-6;
ck.feedbackAgeMax_s=max(T.feedback_age_us(active))*1e-6;
ck.actualDtMedian_s=median(diff(t));
ck.actualDtMin_s=min(diff(t)); ck.actualDtMax_s=max(diff(t));
ck.maxNominalDrift_s=max(abs(t-T.nominal_us*1e-6));
ck.commandMax_Nm=max(abs(T.command_Nm(active)));
ck.torqueQuantization_Nm=2*c.torqueEncodingMax/4095;
ck.timeAlignment='Feedback snapshot precedes command; no fitted or automatic delay correction.';
% Exact measured command timing is retained. No resampling or time shifts.
d.t=t(ii)-t(ii(1)); d.recordTime=t; d.qAll=q; d.vAll=v; d.uAll=u;
d.x=q(ii)-c.q0; d.v=v(ii); d.u=u(ii); d.phase=T.phase(ii);
d.nominalChirpTime=phaseTime(ii);
d.dt=diff(d.t);
% Integer microsecond timestamps allow caching a small set of exact propagators.
[dtUs,~,d.dtGroup]=unique(round(d.dt*1e6));
d.uniqueDt=dtUs*1e-6;
d.initial=[d.x(1);d.v(1)];
assert(numel(d.uniqueDt)<200,'Excessive timing variation; inspect timestamps before identification.');
assert(norm(d.x-mean(d.x))>1e-8 && norm(d.u)>1e-8,'Insufficient excitation/response.');
ck.postHoldSamples=sum(d.phase==2);
ck.preHoldSamples=sum(T.phase==0);
disp(ck);
if ck.feedbackAgeMax_s*c.f1*360>5
    warning('QDD:Timing','Feedback age alone allows %.2f deg phase uncertainty at f1; no delay parameter is fitted.',ck.feedbackAgeMax_s*c.f1*360);
end
if ck.maxNominalDrift_s>0.1/c.fs
    warning('QDD:Clock','Measured timing differs from nominal: simulation uses actual command timestamps.');
end
end

function y = simulate(J,B,c,d)
% Exact ZOH integration between measured command timestamps.
A=[0 1;-c.Kp/J -(B+c.Kd)/J]; b=[0;1/J];
n=numel(d.u); y=zeros(n,2); y(1,:)=d.initial.';
P=zeros(2,2,numel(d.uniqueDt)); Q=zeros(2,numel(d.uniqueDt));
for j=1:numel(d.uniqueDt)
    E=expm([A b;0 0 0]*d.uniqueDt(j));
    P(:,:,j)=E(1:2,1:2); Q(:,j)=E(1:2,3);
end
if isscalar(d.uniqueDt)
    % Equivalent discrete transfer recursion, including measured initial state.
    % Compiled FILTER avoids a long interpreted loop at every objective call.
    D=P(:,:,1); g=Q(:,1); z=d.initial;
    den=[1,-trace(D),det(D)];
    y(:,1)=filter([0,g(1),D(1,2)*g(2)-D(2,2)*g(1)],den,d.u, ...
        [z(1);D(1,2)*z(2)-D(2,2)*z(1)]);
    y(:,2)=filter([0,g(2),D(2,1)*g(1)-D(1,1)*g(2)],den,d.u, ...
        [z(2);D(2,1)*z(1)-D(1,1)*z(2)]);
    return
end
z=d.initial;
for i=1:n-1
    j=d.dtGroup(i);
    z=P(:,:,j)*z+Q(:,j)*d.u(i);
    y(i+1,:)=z.';
end
end

function cost = objective(J,B,c,d)
if ~isfinite(J+B) || J<=realmin || B<0
    cost=1e100; return
end
try
    y=simulate(J,B,c,d);
    cost=mean((y(:,1)-d.x).^2)/max(var(d.x,1),eps);
    if ~isfinite(cost), cost=1e100; end
catch
    cost=1e100;
end
end

function [fit,starts] = identify(d,c,opt)
% Broad starting scales inferred from CSV sweep band, not motor constants.
freq=c.f0*(c.f1/c.f0).^[0 0.5 1];
jSeed=c.Kp./(2*pi*freq).^2;
o=optimset('Display','off','MaxIter',opt.maxIterations,'MaxFunEvals',3*opt.maxIterations, ...
    'TolX',1e-7,'TolFun',1e-9);
rows=zeros(0,7);
for j=jSeed
    for ratio=[0.1 1 10]
        b=max(c.Kd,sqrt(j*c.Kp))*ratio;
        [z,val,flag,out]=fminsearch(@(z)objective(exp(z(1)),exp(z(2)),c,d),log([j b]),o);
        rows(end+1,:)=[j b exp(z(1)) exp(z(2)) val flag out.iterations]; %#ok<AGROW>
    end
    % Explicit boundary ensures B=0 is admissible (not merely exp(-large)).
    [z,val,flag,out]=fminsearch(@(z)objective(exp(z),0,c,d),log(j),o);
    rows(end+1,:)=[j 0 exp(z) 0 val flag out.iterations]; %#ok<AGROW>
end
starts=array2table(rows,'VariableNames',{'initialJ','initialB','J','B','normalizedMSE','exitflag','iterations'});
eligible=find(starts.exitflag>0 & isfinite(starts.normalizedMSE) & starts.normalizedMSE<1e99);
assert(~isempty(eligible),'No optimizer start converged; no identification reported.');
[~,idx]=min(starts.normalizedMSE(eligible)); best=eligible(idx);
fit=struct('J',starts.J(best),'B',starts.B(best),'normalizedMSE',starts.normalizedMSE(best));
if any(starts.exitflag<=0), warning('QDD:Convergence','Some starts did not converge; see multistart table.'); end
end

function F = localFRF(d,c,pred,opt)
% Weighted regression on known chirp phase, local offset and linear trend.
% Complex coefficient cos - i*sin, hence output/input gives phase lag.
f=logspace(log10(c.f0),log10(c.f1),opt.frfPoints).';
n=numel(f); H=complex(nan(n,1)); Hlocal=H;
valid=false(n,1); bandwidth=nan(n,1); cycles=bandwidth; quality=bandwidth;
ts=d.nominalChirpTime;
phi=2*pi*c.f0*expm1(c.k*ts)/c.k;
for i=1:n
    center=log(f(i)/c.f0)/c.k;
    width=opt.frfCycles/f(i);
    selected=abs(ts-center)<=width/2 & d.phase==1;
    ix=find(selected);
    if numel(ix)<16, continue; end
    z=(ts(ix)-center)/(width/2);
    w=0.5*(1+cos(pi*z));
    X=[cos(phi(ix)),sin(phi(ix)),ones(numel(ix),1),z];
    Xw=X.*sqrt(w);
    if rcond(Xw.'*Xw)<1e-10, continue; end
    Y=[d.u(ix),d.x(ix),pred(ix,1)];
    coef=Xw\(Y.*sqrt(w));
    C=coef(1,:)-1i*coef(2,:);
    if abs(C(1))<1e-9, continue; end
    H(i)=C(2)/C(1); Hlocal(i)=C(3)/C(1);
    bandwidth(i)=(c.f0*exp(c.k*ts(ix(end)))-c.f0*exp(c.k*ts(ix(1))))/f(i);
    cycles(i)=(phi(ix(end))-phi(ix(1)))/(2*pi);
    err=Y(:,2)-X*coef(:,2);
    quality(i)=1-sum(w.*err.^2)/max(sum(w.*(Y(:,2)-sum(w.*Y(:,2))/sum(w)).^2),eps);
    valid(i)=center-width/2>=0 && center+width/2<=c.T && ...
        bandwidth(i)<=opt.frfMaxRelativeBandwidth && cycles(i)>=0.9*opt.frfCycles;
end

F=table(f,real(H),imag(H),20*log10(abs(H)),angle(H)*180/pi,valid, ...
    bandwidth,cycles,quality,real(Hlocal),imag(Hlocal), ...
    'VariableNames',{'frequency_Hz','real_rad_per_Nm','imag_rad_per_Nm','gain_dB', ...
    'phase_deg','usableWindow','relativeBandwidth','cycles','localRegressionR2', ...
    'modelLocalReal','modelLocalImag'});
end

function v = frequencyValidation(F,fit,c)
f=F.frequency_Hz; w=2*pi*f;
H=1./(c.Kp-fit.J*w.^2+1i*(fit.B+c.Kd)*w);
E=F.real_rad_per_Nm+1i*F.imag_rad_per_Nm;
ok=F.usableWindow & isfinite(E);
v=struct('usableMin_Hz',NaN,'usableMax_Hz',NaN,'medianGainError_dB',NaN, ...
    'medianAbsPhaseError_deg',NaN,'experimentalPeak_Hz',NaN, ...
    'modelResonance_Hz',NaN,'upperBandSlope_dB_decade',NaN);
if fit.zeta<1/sqrt(2), v.modelResonance_Hz=fit.fn*sqrt(1-2*fit.zeta^2); end
if any(ok)
    ff=f(ok); ee=E(ok);
    v.usableMin_Hz=min(ff); v.usableMax_Hz=max(ff);
    v.medianGainError_dB=median(20*log10(abs(ee./H(ok))));
    v.medianAbsPhaseError_deg=median(abs(angle(ee./H(ok))*180/pi));
    [~,i]=max(abs(ee)); v.experimentalPeak_Hz=ff(i);
    upper=ff>=max(ff)/3;
    if sum(upper)>2
        slope=polyfit(log10(ff(upper)),20*log10(abs(ee(upper))),1);
        v.upperBandSlope_dB_decade=slope(1);
    end
end
disp(v);
end

function figs = figuresFor(r,T)
d=r.data; c=r.conditions; p=r.prediction; F=r.frf; fit=r.fit;
figs=gobjects(5,1);
figs(1)=figure('Name','1 - Recorded data','Color','w');
tiledlayout(3,1);
nexttile; plot(d.recordTime,[d.uAll T.command_Nm]); ylabel('Torque [Nm]'); legend('Chirp','Total');
mark(c);
nexttile; plot(d.recordTime,d.qAll); ylabel('q [rad]'); mark(c);
nexttile; plot(d.recordTime,d.vAll); ylabel('Velocity [rad/s]'); xlabel('Record time [s]'); mark(c);
figs(2)=figure('Name','2 - FRF validation','Color','w');
f=F.frequency_Hz; w=2*pi*f;
H=1./(c.Kp-fit.J*w.^2+1i*(fit.B+c.Kd)*w);
L=F.modelLocalReal+1i*F.modelLocalImag;
ok=F.usableWindow;
tiledlayout(2,1);
nexttile; semilogx(f,20*log10(abs(H)),'k-',f,20*log10(abs(L)),'b--', ...
    f(ok),F.gain_dB(ok),'ro',f(~ok),F.gain_dB(~ok),'x','Color',[0.65 0.65 0.65]);
% Set each line color explicitly (the final property applies to all lines).
ax=gca; ax.Children(1).Color=[0.65 0.65 0.65]; ax.Children(2).Color='r'; ax.Children(3).Color='b'; ax.Children(4).Color='k';
ylabel('Gain [dB re rad/Nm]'); grid on;
legend('Analytic model','Model with same local estimator','Experiment usable window','Rejected window','Location','best');
nexttile; lines=semilogx(f,phaseForPlot(H),'k-',f,phaseForPlot(L),'b--', ...
    f(ok),F.phaseUnwrapped_deg(ok),'ro',f(~ok),F.phaseUnwrapped_deg(~ok),'x');
lines(4).Color=[0.65 0.65 0.65];
ylabel('Phase [deg, unwrapped]'); xlabel('Frequency [Hz]'); grid on;
figs(3)=figure('Name','3 - Angle response','Color','w');
plot(d.t+c.start,[d.x+c.q0,p(:,1)+c.q0]); ylabel('q [rad]'); xlabel('Record time [s]');
legend('Measured','Model'); title(sprintf('Same-record fit %.2f%%; RMSE %.4g rad',fit.fitPercent,fit.RMSE)); mark(c);
figs(4)=figure('Name','4 - Angle residual','Color','w');
plot(d.t+c.start,r.residual); ylabel('Measured - model [rad]'); xlabel('Record time [s]'); mark(c);
figs(5)=figure('Name','5 - Velocity response','Color','w');
plot(d.t+c.start,[d.v,p(:,2)]); ylabel('Velocity [rad/s]'); xlabel('Record time [s]');
legend('Measured','Model'); mark(c);
end

function mark(c)
xline(c.start,'--','Chirp start','HandleVisibility','off', ...
    'LabelOrientation','horizontal','LabelVerticalAlignment','bottom');
xline(c.start+c.T,'--','Chirp end','HandleVisibility','off', ...
    'LabelOrientation','horizontal','LabelVerticalAlignment','bottom');
grid on;
end

function p = phaseForPlot(H)
p=nan(size(H)); ok=isfinite(H);
p(ok)=unwrap(angle(H(ok)))*180/pi;
end

function writeReport(file,r)
fid=fopen(file,'w','n','UTF-8'); assert(fid>=0,'Cannot write report.');
cleanup=onCleanup(@()fclose(fid));
fprintf(fid,'Source: %s\nMATLAB: %s\n',r.sourceCSV,r.matlabVersion);
fprintf(fid,'J=%.12g kg*m^2\nB=%.12g Nm*s/rad\nKp=%.12g Nm/rad\nKd=%.12g Nm*s/rad\n',r.fit.J,r.fit.B,r.conditions.Kp,r.conditions.Kd);
fprintf(fid,'fn=%.9g Hz\nzeta=%.9g\nFit=%.9g %%\nRMSE=%.9g rad\n',r.fit.fn,r.fit.zeta,r.fit.fitPercent,r.fit.RMSE);
fprintf(fid,'Fit formula: %s\n',r.fit.fitDefinition);
fprintf(fid,'Fixed initial x,v: %.9g rad, %.9g rad/s\n',r.fit.initialState);
fprintf(fid,'Fit uses sweep + PD-only tail, exact ZOH at actual command times. Pre-average excluded (PD disabled).\n');
fprintf(fid,'Only J,B fitted. No delay, offset, gain or initial-state nuisance parameters fitted.\n');
fprintf(fid,'Same-record comparison is not independent model validation.\n');
fprintf(fid,'FRF: Hann-weighted local chirp-phase sin/cos + offset/trend regression. Approximate quasi-steady estimate.\n');
fprintf(fid,'Usable windows: %d/%d. Broad relative bandwidth or truncated windows rejected.\n',sum(r.frf.usableWindow),height(r.frf));
fprintf(fid,'Low-frequency/edge estimates can be biased by transients. Local model curve shows estimator/sweep bias.\n');
fprintf(fid,'Local regression R2 is NOT coherence; coherence omitted for lack of independent sweep repetitions.\n');
fprintf(fid,'Torque Nm convention follows firmware MIT output coordinates; no gear multiplication applied.\n');
fprintf(fid,'Absolute torque calibration unavailable. Absolute J/B cannot be established independently of torque gain.\n');
fprintf(fid,'Feedback predates command: median/max age %.9g/%.9g s. No automatic temporal shift.\n',r.checks.feedbackAgeMedian_s,r.checks.feedbackAgeMax_s);
fprintf(fid,'CAN torque quantization step %.9g Nm; logged request assumed equal to physical torque.\n',r.checks.torqueQuantization_Nm);
fprintf(fid,'B near zero, variable multistart estimates, or flat B profile indicate limited identifiability.\n');
fprintf(fid,'B profile is a cost diagnostic, not a statistical confidence interval.\n');
fprintf(fid,'Poor fits may reflect friction/cogging/backlash, feedback age, command calibration, or driver dynamics.\n');
fprintf(fid,'Do not interpret a best-fit second-order parameter as independently validated physical inertia/friction.\n');
v=r.frequencyValidation;
fprintf(fid,'Usable FRF band %.5g to %.5g Hz; median gain error %.5g dB, median absolute phase error %.5g deg.\n', ...
    v.usableMin_Hz,v.usableMax_Hz,v.medianGainError_dB,v.medianAbsPhaseError_deg);
fprintf(fid,'Largest usable experimental magnitude at %.5g Hz (not necessarily a resonance).\n',v.experimentalPeak_Hz);
fprintf(fid,'Model resonance %.5g Hz (NaN means no displacement resonance); upper-band slope %.5g dB/decade.\n', ...
    v.modelResonance_Hz,v.upperBandSlope_dB_decade);
fprintf(fid,'Below usable FRF band, low-frequency gain/resonance cannot be validated from this local estimate.\n');
end
