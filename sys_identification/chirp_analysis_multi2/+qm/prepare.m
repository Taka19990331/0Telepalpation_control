% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
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

if ck.feedbackAgeMax_s*c.f1*360>5
    warning('QDD:Timing','Feedback age alone allows %.2f deg phase uncertainty at f1; effective delay does not identify its cause.',ck.feedbackAgeMax_s*c.f1*360);
end
if ck.maxNominalDrift_s>0.1/c.fs
    warning('QDD:Clock','Measured timing differs from nominal: simulation uses actual command timestamps.');
end
end


