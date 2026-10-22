function [steady, diagnostics, steps] = extract_steady_state(T, meta, cfg)
%EXTRACT_STEADY_STATE Search commanded steps for defensible steady windows.
required = {'actual_us','phase','step_index','event','target_torque_Nm','position_deg', ...
    'speed_rad_s','feedback_torque_estimate_Nm','feedback_rx_us','feedback_age_us', ...
    'feedback_sequence','motor_error','settling_status','reason'};
for k=1:numel(required)
    if ~ismember(required{k},T.Properties.VariableNames)
        error('Required CSV column missing: %s',required{k});
    end
end
if isfield(meta,'config')
    if isfield(meta.config,'POSITION_MIN_RAD'), cfg.motion.position_min_rad=meta.config.POSITION_MIN_RAD; else, cfg.motion.position_min_rad=-12.5; end
    if isfield(meta.config,'POSITION_MAX_RAD'), cfg.motion.position_max_rad=meta.config.POSITION_MAX_RAD; else, cfg.motion.position_max_rad=12.5; end
    if isfield(meta.config,'PERIOD_US'), cfg.steady.expected_feedback_period_s=meta.config.PERIOD_US*1e-6; end
    if isfield(meta.config,'VELOCITY_MAX_RAD_S'), cfg.motion.stationary_speed_resolution_rad_s=2*meta.config.VELOCITY_MAX_RAD_S/4095; end
else
    cfg.motion.position_min_rad=-12.5; cfg.motion.position_max_rad=12.5;
end
cfg.motion.position_quantization_rad=(cfg.motion.position_max_rad-cfg.motion.position_min_rad)/65535;
t=double(T.actual_us)*1e-6;
active=double(T.phase)>=1 & double(T.phase)<=4;
keys=[double(T.phase),double(T.step_index)]; rows=find(active); groups={};
if ~isempty(rows)
    cuts=[1;find(any(diff(keys(rows,:),1,1)~=0,2))+1;numel(rows)+1];
    for q=1:numel(cuts)-1, groups{end+1}=rows(cuts(q):cuts(q+1)-1); end %#ok<AGROW>
end

base=struct('Phase',NaN,'StepIndex',NaN,'TargetTorqueNm',NaN, ...
 'MeanFeedbackTorqueNm',NaN,'MeanSpeedRadS',NaN,'SpeedStdRadS',NaN,'TorqueStdNm',NaN, ...
 'EstimatedAccelerationRadS2',NaN,'SelectedWindowStartTimeS',NaN,'SelectedWindowEndTimeS',NaN, ...
 'RequestedWindowDurationS',NaN,'DurationS',NaN,'NumValidFeedbackSamples',0,'NumRawWindowSamples',0, ...
 'ValidFeedbackFraction',NaN,'FeedbackStartCoverageS',NaN,'FeedbackEndCoverageS',NaN, ...
 'MaxFeedbackGapS',NaN,'MaxInvalidStreakS',NaN,'FeedbackUpdateHz',NaN,'FeedbackValid',false, ...
 'MotionClassification',"UNCERTAIN",'LowSpeedBehavior',"UNRESOLVED",'MotionConsistent',false, ...
 'PositionNetDisplacementRad',NaN,'PositionRangeRad',NaN,'PositionVelocityRadS',NaN, ...
 'FirmwareTerminationReason',"UNKNOWN",'FirmwareReasonCode',NaN,'FirmwareStateAtEnd',NaN, ...
 'Accepted',false,'OfflineDecision',"REJECTED",'RejectionReason',"", ...
 'DynamicFrictionUsable',false,'DynamicFrictionEligibilityReason',"not evaluated", ...
 'CandidateWindowsEvaluated',0,'CandidateWindowsPassingFeedback',0, ...
 'CandidateWindowsFailingDuration',0,'CandidateWindowsFailingFeedback',0, ...
 'CandidateWindowsFailingEdgeCoverage',0,'CandidateWindowsFailingFeedbackGap',0, ...
 'CandidateWindowsFailingInvalidCluster',0,'CandidateWindowsFailingInvalidFeedback',0, ...
 'CandidateWindowsFailingAcceleration',0,'CandidateWindowsFailingSpeedStd',0, ...
 'CandidateWindowsFailingLowSpeedStd',0,'CandidateWindowsFailingMotionCheck',0, ...
 'CandidateWindowDurationS',0,'CandidateWindowRequestedDurationS',NaN, ...
 'NumStepStartEvents',0,'NumStepEndEvents',0);
steadyRows=repmat(base,0,1); diagRows=repmat(base,0,1); stepRows=repmat(base,0,1);
reasonNames={'OK','NOT_SETTLED','UNSTABLE','USER_ABORT','OVERSPEED','FEEDBACK_LOST','MOTOR_ERROR', ...
    'SD_ERROR','RING_FULL','TX_FAILED','CAN_ERROR','CONTROL_DEADLINE','TORQUE_RANGE','INVALID_FEEDBACK', ...
    'START_TIMEOUT','STOP_FAILED','RUN_LIMIT'};

for g=1:numel(groups)
    idx=groups{g}; s=base;
    s.Phase=double(T.phase(idx(1))); s.StepIndex=double(T.step_index(idx(1)));
    s.TargetTorqueNm=double(T.target_torque_Nm(idx(1)));
    ev=uint32(T.event(idx));
    startRows=idx(bitand(ev,1)~=0); endRows=idx(bitand(ev,2)~=0);
    s.NumStepStartEvents=numel(startRows); s.NumStepEndEvents=numel(endRows);
    if ~isempty(endRows)
        terminal=endRows(1); code=double(T.reason(terminal));
        s.FirmwareReasonCode=code; s.FirmwareStateAtEnd=double(T.settling_status(terminal));
        if isfinite(code) && code>=0 && code<numel(reasonNames), s.FirmwareTerminationReason=string(reasonNames{code+1}); end
        % The end-event row is valid for the old step only when firmware still
        % reports that target and has not already entered COASTING/changed target.
        includeEnd=abs(double(T.target_torque_Nm(terminal))-s.TargetTorqueNm)<=1e-6 && ...
            double(T.settling_status(terminal))<=3;
        finalRow=terminal-1+double(includeEnd);
    else
        finalRow=idx(end); % right-censored step: still truncate at command changes below
    end
    if ~isempty(startRows), firstRow=startRows(1)+1; else, firstRow=idx(1); end
    usable=idx(idx>=firstRow & idx<=finalRow);
    if isempty(usable), usable=idx(idx<=finalRow); end
    if ~isempty(usable)
        sameTarget=abs(double(T.target_torque_Nm(usable))-s.TargetTorqueNm)<=1e-6;
        firstChange=find(~sameTarget,1);
        if ~isempty(firstChange), usable=usable(1:firstChange-1); end
    end
    if isempty(usable), usable=idx(1); end

    % Search the newest end time first, then prefer the longest qualifying window.
    nDur=floor((cfg.steady.window_s-cfg.steady.min_valid_duration_s)/cfg.steady.duration_step_s+1e-9);
    durations=cfg.steady.window_s-(0:nDur)*cfg.steady.duration_step_s;
    latestEnd=t(usable(end)); earliestEnd=t(usable(1))+cfg.steady.min_valid_duration_s;
    available=min(cfg.steady.window_s,max(0,t(usable(end))-t(usable(1))));
    s.CandidateWindowDurationS=available;
    if available+cfg.steady.window_duration_tolerance_s<cfg.steady.min_valid_duration_s
        s.CandidateWindowsFailingDuration=1;
    else
        hasDiagnosticMetrics=false;
        % Half-period allowance handles a requested edge between two samples.
        earliestEnd=t(usable(1))+cfg.steady.min_valid_duration_s-cfg.steady.window_boundary_tolerance_s;
        span=latestEnd-earliestEnd;
        if span>=-cfg.steady.window_boundary_tolerance_s
            nEnds=floor(max(0,span)/cfg.steady.duration_step_s+1e-9);
            endTimes=latestEnd-(0:nEnds)*cfg.steady.duration_step_s;
        else
            endTimes=[];
        end
        for tEnd=endTimes
            for requested=durations
                tStart=tEnd-requested;
                wi=usable(t(usable)>=tStart-1e-9 & t(usable)<=tEnd+1e-9);
                s.CandidateWindowsEvaluated=s.CandidateWindowsEvaluated+1;
                if isempty(wi)
                    s.CandidateWindowsFailingDuration=s.CandidateWindowsFailingDuration+1;
                    continue
                end
                [m,passFeedback,feedbackReasons]=measure_window(T,wi,t,tStart,tEnd,requested,cfg);
                if ~hasDiagnosticMetrics
                    s.CandidateWindowRequestedDurationS=requested;
                    s.RequestedWindowDurationS=requested;
                    s.NumRawWindowSamples=numel(wi); s.ValidFeedbackFraction=m.validFraction;
                    s.FeedbackStartCoverageS=m.startCoverage; s.FeedbackEndCoverageS=m.endCoverage;
                    s.MaxFeedbackGapS=m.maxGap; s.MaxInvalidStreakS=m.maxInvalidStreak;
                    if ~isempty(m.indices)
                        dd=polyfit(m.rxTime-mean(m.rxTime),m.speed,1);
                        s.EstimatedAccelerationRadS2=dd(1); s.SpeedStdRadS=std(m.speed,0);
                        s.MeanSpeedRadS=mean(m.speed); s.MeanFeedbackTorqueNm=mean(m.torque);
                        s.TorqueStdNm=std(m.torque,0); s.DurationS=m.duration;
                        s.NumValidFeedbackSamples=numel(m.indices);
                        s.FeedbackUpdateHz=(numel(m.indices)-1)/max(eps,m.duration);
                        s.PositionNetDisplacementRad=m.position(end)-m.position(1);
                        s.PositionRangeRad=max(m.position)-min(m.position);
                    end
                    hasDiagnosticMetrics=true;
                end
                if passFeedback, s.CandidateWindowsPassingFeedback=s.CandidateWindowsPassingFeedback+1;
                else, s.CandidateWindowsFailingFeedback=s.CandidateWindowsFailingFeedback+1; end
                s.CandidateWindowsFailingEdgeCoverage=s.CandidateWindowsFailingEdgeCoverage+double(any(feedbackReasons=="edge_coverage"));
                s.CandidateWindowsFailingFeedbackGap=s.CandidateWindowsFailingFeedbackGap+double(any(feedbackReasons=="feedback_gap"));
                s.CandidateWindowsFailingInvalidCluster=s.CandidateWindowsFailingInvalidCluster+double(any(feedbackReasons=="invalid_cluster"));
                s.CandidateWindowsFailingInvalidFeedback=s.CandidateWindowsFailingInvalidFeedback+ ...
                    double(any(feedbackReasons=="invalid_feedback") || any(feedbackReasons=="insufficient_samples"));
                if ~passFeedback
                    s.CandidateWindowsFailingDuration=s.CandidateWindowsFailingDuration+double(any(feedbackReasons=="duration"));
                    continue
                end
                ix=m.indices; rx=m.rxTime; vv=m.speed; tq=m.torque; pp=m.position;
                accel=polyfit(rx-mean(rx),vv,1); accel=accel(1); speedStd=std(vv,0);
                motion=classify_motion(rx,pp,vv,cfg);
                s.EstimatedAccelerationRadS2=accel; s.SpeedStdRadS=speedStd;
                s.MeanSpeedRadS=mean(vv); s.MeanFeedbackTorqueNm=mean(tq); s.TorqueStdNm=std(tq,0);
                s.DurationS=rx(end)-rx(1); s.NumValidFeedbackSamples=numel(ix);
                s.FeedbackUpdateHz=(numel(ix)-1)/max(eps,s.DurationS); s.FeedbackValid=true;
                s.MotionClassification=motion.classification; s.LowSpeedBehavior=motion.behavior;
                s.MotionConsistent=motion.consistent; s.PositionNetDisplacementRad=motion.netDisplacement;
                s.PositionRangeRad=motion.positionRange; s.PositionVelocityRadS=motion.positionSlope;
                s.SelectedWindowStartTimeS=tStart; s.SelectedWindowEndTimeS=tEnd;
                if abs(accel)>cfg.steady.max_abs_accel_rad_s2
                    s.CandidateWindowsFailingAcceleration=s.CandidateWindowsFailingAcceleration+1;
                    continue
                end
                if speedStd>cfg.steady.max_speed_std_rad_s
                    s.CandidateWindowsFailingSpeedStd=s.CandidateWindowsFailingSpeedStd+1;
                    continue
                end
                if motion.behavior=="STICK_SLIP"
                    s.CandidateWindowsFailingMotionCheck=s.CandidateWindowsFailingMotionCheck+1;
                    continue
                end
                lowSpeed=abs(mean(vv))<cfg.steady.low_speed_threshold_rad_s;
                if lowSpeed && speedStd>cfg.steady.low_speed_max_speed_std_rad_s && motion.classification~="STATIONARY"
                    s.CandidateWindowsFailingLowSpeedStd=s.CandidateWindowsFailingLowSpeedStd+1;
                    continue
                end
                if lowSpeed && motion.classification=="MOVING" && ~motion.consistent
                    s.CandidateWindowsFailingMotionCheck=s.CandidateWindowsFailingMotionCheck+1;
                    continue
                end
                s.Accepted=true; s.OfflineDecision="ACCEPTED"; s.RejectionReason="";
                s.RequestedWindowDurationS=requested; s.CandidateWindowRequestedDurationS=requested;
                s.NumRawWindowSamples=numel(wi); s.ValidFeedbackFraction=m.validFraction;
                s.FeedbackStartCoverageS=m.startCoverage; s.FeedbackEndCoverageS=m.endCoverage;
                s.MaxFeedbackGapS=m.maxGap; s.MaxInvalidStreakS=m.maxInvalidStreak;
                if motion.classification=="MOVING" && motion.behavior=="CONTINUOUS" && ...
                        (~lowSpeed || speedStd<=cfg.steady.low_speed_max_speed_std_rad_s)
                    s.DynamicFrictionUsable=true; s.DynamicFrictionEligibilityReason="continuous measured motion; position and velocity agree";
                elseif motion.behavior=="STICK_SLIP"
                    s.DynamicFrictionEligibilityReason="stick-slip signature; exclude from steady dynamic friction";
                elseif motion.classification=="STATIONARY"
                    s.DynamicFrictionEligibilityReason="stationary point; retain for static analysis only";
                else
                    s.DynamicFrictionEligibilityReason="motion is uncertain or inconsistent; exclude from dynamic friction";
                end
                break
            end
            if s.Accepted, break; end
        end
    end
    if ~s.Accepted
        s.FeedbackValid=s.CandidateWindowsPassingFeedback>0;
        s.RejectionReason=summarize_failures(s);
    end
    diagRows(end+1)=s; stepRows(end+1)=s; %#ok<AGROW>
    if s.Accepted, steadyRows(end+1)=s; end %#ok<AGROW>
end
steady=struct2table(steadyRows); diagnostics=struct2table(diagRows); steps=stepRows;
end

function [m,ok,reasons]=measure_window(T,wi,t,tStart,tEnd,requested,cfg)
% Validate full raw-window coverage before deduplicating and measuring feedback.
rawN=numel(wi); rowTime=t(wi);
finite=isfinite(double(T.speed_rad_s(wi))) & isfinite(double(T.feedback_torque_estimate_Nm(wi))) & ...
    isfinite(double(T.position_deg(wi))) & isfinite(double(T.feedback_rx_us(wi)));
fresh=double(T.feedback_age_us(wi))>=0 & double(T.feedback_age_us(wi))<=cfg.steady.max_feedback_age_s*1e6;
healthy=double(T.motor_error(wi))==0;
sequenced=double(T.feedback_sequence(wi))>0 & double(T.feedback_rx_us(wi))>0;
valid=finite & fresh & healthy & sequenced;
invalidFraction=sum(~valid)/max(1,rawN);
maxInvalidStreak=invalid_run_duration(rowTime,valid,cfg.steady.expected_feedback_period_s);
reasons=strings(0,1);
edgeInvalid=(~valid(1) && rowTime(1)-tStart<=cfg.steady.window_boundary_tolerance_s) || ...
    (~valid(end) && tEnd-rowTime(end)<=cfg.steady.window_boundary_tolerance_s);
if rowTime(1)-tStart>cfg.steady.window_boundary_tolerance_s || ...
        tEnd-rowTime(end)>cfg.steady.window_boundary_tolerance_s || edgeInvalid
    reasons(end+1)="duration"; %#ok<AGROW>
end
if invalidFraction>cfg.steady.max_invalid_feedback_fraction || maxInvalidStreak>cfg.steady.max_invalid_feedback_streak_s
    reasons(end+1)="invalid_cluster"; %#ok<AGROW>
end
rawValid=wi(valid);
if isempty(rawValid)
    m=empty_measurement(rawN,invalidFraction,maxInvalidStreak); reasons(end+1)="invalid_feedback"; %#ok<AGROW>
    ok=false; return
end
seq=double(T.feedback_sequence(rawValid)); rxUs=double(T.feedback_rx_us(rawValid));
[~,ia]=unique(seq,'stable'); rawValid=rawValid(ia); rxUs=rxUs(ia);
[~,ia]=unique(rxUs,'stable'); rawValid=rawValid(ia); rxUs=rxUs(ia);
[rxUs,ord]=sort(rxUs); rawValid=rawValid(ord); rxTime=rxUs*1e-6;
if numel(rawValid)<3
    m=empty_measurement(rawN,invalidFraction,maxInvalidStreak); reasons(end+1)="insufficient_samples"; %#ok<AGROW>
    ok=false; return
end
gaps=diff(rxTime); maxGap=max([0;gaps]);
firstSampleTime=t(rawValid(1)); lastSampleTime=t(rawValid(end));
coverage=rxTime(end)-rxTime(1);
startCoverage=firstSampleTime-tStart; endCoverage=tEnd-lastSampleTime;
if startCoverage>cfg.steady.window_boundary_tolerance_s || endCoverage>cfg.steady.window_boundary_tolerance_s || ...
        coverage<requested-cfg.steady.window_duration_tolerance_s
    reasons(end+1)="edge_coverage"; %#ok<AGROW>
end
if maxGap>cfg.steady.max_feedback_gap_s, reasons(end+1)="feedback_gap"; end %#ok<AGROW>
if any(reasons=="duration") || coverage<cfg.steady.min_valid_duration_s-cfg.steady.window_duration_tolerance_s
    reasons(end+1)="duration"; %#ok<AGROW>
end
posRad=deg2rad(double(T.position_deg(rawValid)));
posRad=unwrap_position(posRad,cfg.motion.position_min_rad,cfg.motion.position_max_rad);
m=struct('indices',rawValid,'rxTime',rxTime,'speed',double(T.speed_rad_s(rawValid)), ...
 'torque',double(T.feedback_torque_estimate_Nm(rawValid)),'position',posRad, ...
 'validFraction',numel(rawValid)/max(1,rawN),'startCoverage',startCoverage, ...
 'endCoverage',endCoverage,'maxGap',maxGap,'maxInvalidStreak',maxInvalidStreak,'duration',coverage);
ok=isempty(reasons);
end

function m=empty_measurement(rawN,invalidFraction,maxInvalidStreak)
m=struct('indices',[],'rxTime',[],'speed',[],'torque',[],'position',[], ...
 'validFraction',1-invalidFraction,'startCoverage',NaN,'endCoverage',NaN, ...
 'maxGap',NaN,'maxInvalidStreak',maxInvalidStreak,'rawN',rawN,'duration',0);
end

function duration=invalid_run_duration(t,valid,period)
duration=0; run=[];
for k=1:numel(valid)
    if ~valid(k)
        if isempty(run), run=k; end
    elseif ~isempty(run)
        duration=max(duration,t(k-1)-t(run)+period); run=[];
    end
end
if ~isempty(run), duration=max(duration,t(end)-t(run)+period); end
end

function p=unwrap_position(p,minRad,maxRad)
% Correct only a confirmed crossing between opposite encoder range edges.
span=maxRad-minRad; if isempty(p) || ~isfinite(span) || span<=0, return; end
margin=0.05*span; offset=0; out=p;
for k=2:numel(p)
    d=p(k)-p(k-1);
    if d < -span/2 && p(k-1)>=maxRad-margin && p(k)<=minRad+margin
        offset=offset+span;
    elseif d > span/2 && p(k-1)<=minRad+margin && p(k)>=maxRad-margin
        offset=offset-span;
    end
    out(k)=p(k)+offset;
end
p=out;
end

function m=classify_motion(t,p,v,cfg)
net=p(end)-p(1); posRange=max(p)-min(p);
if numel(t)>1, fit=polyfit(t-mean(t),p,1); posSlope=fit(1); else, posSlope=0; end
meanSpeed=mean(v); lowSpeed=abs(meanSpeed)<cfg.steady.low_speed_threshold_rad_s;
tol=max(cfg.motion.velocity_consistency_tolerance_rad_s,2*cfg.motion.stationary_speed_resolution_rad_s);
consistent=abs(posSlope-meanSpeed)<=tol;
if posRange<=cfg.motion.stationary_position_range_rad && abs(meanSpeed)<=cfg.motion.stationary_speed_resolution_rad_s
    classification="STATIONARY"; behavior="STATIONARY";
elseif lowSpeed && detect_stick_slip(t,p,v,cfg)
    classification="UNCERTAIN"; behavior="STICK_SLIP"; consistent=false;
elseif abs(net)>=cfg.motion.moving_min_net_displacement_rad && ...
        abs(posSlope)>=cfg.motion.moving_min_position_slope_rad_s && consistent && ...
        (abs(meanSpeed)<=cfg.motion.stationary_speed_resolution_rad_s || sign(meanSpeed)==sign(posSlope))
    classification="MOVING"; behavior="CONTINUOUS";
elseif ~lowSpeed && abs(net)>=cfg.motion.moving_min_net_displacement_rad && ...
        sign(meanSpeed)==sign(posSlope) && consistent
    classification="MOVING"; behavior="CONTINUOUS";
else
    classification="UNCERTAIN";
    if lowSpeed && abs(net)<cfg.motion.moving_min_net_displacement_rad, behavior="NO_POSITION_MOTION";
    elseif lowSpeed && ~consistent, behavior="VELOCITY_POSITION_MISMATCH";
    else, behavior="UNRESOLVED"; end
end
m=struct('classification',classification,'behavior',behavior,'consistent',consistent, ...
 'netDisplacement',net,'positionRange',posRange,'positionSlope',posSlope);
end

function tf=detect_stick_slip(t,p,v,cfg)
% Require repeated position bursts separated by plateaus; a zero speed sample alone is irrelevant.
tf=false; if numel(t)<4 || t(end)-t(1)<0.2, return; end
edges=t(1):cfg.motion.slip_bin_s:t(end);
if numel(edges)<4, return; end
delta=NaN(1,numel(edges)-1); burstSpeed=NaN(size(delta));
for k=1:numel(delta)
    ix=find(t>=edges(k) & t<edges(k+1));
    if numel(ix)>=2
        delta(k)=p(ix(end))-p(ix(1)); burstSpeed(k)=mean(v(ix));
    end
end
moving=abs(delta)>=cfg.motion.slip_min_displacement_rad;
plateau=abs(delta)<=cfg.motion.position_quantization_rad;
if sum(moving)<2 || sum(plateau)<2, return; end
bursts=find(moving);
for ia=1:numel(bursts)
    a=bursts(ia);
    later=bursts(bursts>a);
    for ib=1:numel(later)
        b=later(ib);
        between=(a+1):(b-1);
        if isempty(between) || ~any(plateau(between)) || (b-a)*cfg.motion.slip_bin_s<cfg.motion.slip_min_pause_s, continue; end
        corroborated=true;
        for q=[a b]
            if abs(burstSpeed(q))<cfg.motion.stationary_speed_resolution_rad_s || ...
                    sign(burstSpeed(q))~=sign(delta(q)), corroborated=false; end
        end
        if corroborated, tf=true; return; end
    end
end
end

function reason=summarize_failures(s)
parts=strings(0,1);
if s.CandidateWindowsEvaluated==0
    parts(end+1)="no full-duration candidate window"; %#ok<AGROW>
end
if s.CandidateWindowsEvaluated>0 && s.CandidateWindowsFailingDuration>0
    parts(end+1)=string(sprintf('window duration/coverage failed in %d candidate(s)',s.CandidateWindowsFailingDuration)); %#ok<AGROW>
end
if s.CandidateWindowsFailingFeedback>0, parts(end+1)=string(sprintf('feedback coverage/quality failed in %d candidate(s)',s.CandidateWindowsFailingFeedback)); end %#ok<AGROW>
if s.CandidateWindowsFailingEdgeCoverage>0, parts(end+1)=string(sprintf('window edge coverage failed in %d candidate(s)',s.CandidateWindowsFailingEdgeCoverage)); end %#ok<AGROW>
if s.CandidateWindowsFailingFeedbackGap>0, parts(end+1)=string(sprintf('feedback receive gap failed in %d candidate(s)',s.CandidateWindowsFailingFeedbackGap)); end %#ok<AGROW>
if s.CandidateWindowsFailingInvalidCluster>0, parts(end+1)=string(sprintf('invalid feedback cluster failed in %d candidate(s)',s.CandidateWindowsFailingInvalidCluster)); end %#ok<AGROW>
if s.CandidateWindowsFailingInvalidFeedback>0, parts(end+1)=string(sprintf('invalid/insufficient feedback failed in %d candidate(s)',s.CandidateWindowsFailingInvalidFeedback)); end %#ok<AGROW>
if s.CandidateWindowsFailingAcceleration>0, parts(end+1)=string(sprintf('acceleration limit failed in %d candidate(s)',s.CandidateWindowsFailingAcceleration)); end %#ok<AGROW>
if s.CandidateWindowsFailingSpeedStd>0, parts(end+1)=string(sprintf('speed standard deviation limit failed in %d candidate(s)',s.CandidateWindowsFailingSpeedStd)); end %#ok<AGROW>
if s.CandidateWindowsFailingLowSpeedStd>0, parts(end+1)=string(sprintf('low-speed dispersion limit failed in %d candidate(s)',s.CandidateWindowsFailingLowSpeedStd)); end %#ok<AGROW>
if s.CandidateWindowsFailingMotionCheck>0, parts(end+1)=string(sprintf('low-speed motion consistency/stick-slip check failed in %d candidate(s)',s.CandidateWindowsFailingMotionCheck)); end %#ok<AGROW>
if isempty(parts), parts="no qualifying steady window"; end
reason=strjoin(parts,'; ');
end
