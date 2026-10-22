% MAIN Run the FR0007 RFOB-style friction estimate.
clear; clc;
cfg = config();
if ~isfile(cfg.input_csv), error('Input CSV not found: %s', cfg.input_csv); end
if ~isfolder(cfg.output_dir), mkdir(cfg.output_dir); end

[T, meta] = read_fric_csv(cfg.input_csv);
required = {'actual_us','phase','step_index','settling_status','event', ...
    'target_torque_Nm','position_deg','speed_rad_s', ...
    'feedback_torque_estimate_Nm','feedback_rx_us','feedback_age_us', ...
    'feedback_sequence','motor_error','can_tx_accepted','period_us'};
if ~all(ismember(required,T.Properties.VariableNames))
    error('FR0007 is missing required columns: %s', strjoin(setdiff(required,T.Properties.VariableNames), ', '));
end

% Feedback torque and speed are decoded from the same latest CAN snapshot.
% Filter them on their CAN receive timestamps; command rows retain actual_us.
rowTime = (double(T.actual_us)-double(T.actual_us(1))) * 1e-6;
rxRaw = double(T.feedback_rx_us);
t = (rxRaw-rxRaw(1))*1e-6;
target = double(T.target_torque_Nm);
tau = double(T.feedback_torque_estimate_Nm);
omega = double(T.speed_rad_s);
age = double(T.feedback_age_us)*1e-6;
seq = double(T.feedback_sequence);
n = height(T);
valid = isfinite(t) & isfinite(tau) & isfinite(omega) & isfinite(age) & ...
    age >= 0 & age <= cfg.feedback_timeout_s & seq > 0 & ...
    double(T.motor_error)==0 & double(T.can_tx_accepted)~=0;
% A duplicate sequence is not a fresh feedback observation.
valid([false; diff(seq)==0]) = false;
timingOK = true(n,1); lastFeedback = NaN;
for k=1:n
    if valid(k)
        if isfinite(lastFeedback) && (t(k)<=t(lastFeedback) || ...
                t(k)-t(lastFeedback)>cfg.max_feedback_gap_s)
            timingOK(k)=false;
        end
        lastFeedback=k;
    end
end

nFc = numel(cfg.rfob.cutoff_hz);
estimated = NaN(n,nFc);
filterState = NaN(n,nFc);
filterSegment = zeros(n,1);
for b = 1:nFc
    fc = cfg.rfob.cutoff_hz(b);
    g = 2*pi*fc; % Hz -> rad/s
    initialized = false; lastIdx = NaN; segment = 0; y = NaN;
    for k = 1:n
        if ~valid(k), continue; end
        u = tau(k) + cfg.inertia_J_kg_m2*g*omega(k);
        if ~initialized || (t(k)-t(lastIdx) > cfg.max_filter_gap_s) || t(k)<=t(lastIdx)
            % Initialize Q output to its input so the algebraic cancellation
            % gives tau_hat_f(t0)=tau_m(t0), avoiding an artificial startup impulse.
            y = u; initialized = true; segment = segment+1;
        else
            dt = t(k)-t(lastIdx);
            alpha = -expm1(-g*dt); % exact ZOH discretization of 1st-order LPF
            y = alpha*u + (1-alpha)*y;
        end
        estimated(k,b) = y - cfg.inertia_J_kg_m2*g*omega(k);
        filterState(k,b) = y;
        lastIdx = k;
        if b==1, filterSegment(k)=segment; end
    end
end

% Only moving samples in the two recorded torque sweeps are the target data.
% State 1=SETTLING and 2=ACQUIRING are retained to capture acceleration after
% breakaway; state 4=COASTING and terminal/invalid rows are excluded.
state = double(T.settling_status);
phase = double(T.phase);
step = double(T.step_index);
stepStart = bitand(uint32(T.event),uint32(1))~=0;
stepElapsed = NaN(n,1);
stepStartTime = NaN; activePhase = NaN; activeStep = NaN;
for k=1:n
    if stepStart(k) || phase(k)~=activePhase || step(k)~=activeStep
        if stepStart(k), stepStartTime=rowTime(k); else, stepStartTime=NaN; end
        activePhase=phase(k); activeStep=step(k);
    end
    if isfinite(stepStartTime), stepElapsed(k)=rowTime(k)-stepStartTime; end
end
interval = valid & ismember(phase,[1 3]) & ismember(state,[1 2]) & ...
    abs(omega)>=cfg.minimum_moving_speed_rad_s & abs(target)>0 & ...
    stepElapsed>=cfg.step_start_guard_s & timingOK;
if cfg.exclude_coasting, interval = interval & state~=4; end

% Long-form export: each valid sample appears once per cutoff frequency.
rows = n*nFc;
Time_s = repmat(t,nFc,1);
CommandRowTime_s = repmat(rowTime,nFc,1);
Phase = repmat(phase,nFc,1);
StepIndex = repmat(step,nFc,1);
TargetTorqueNm = repmat(target,nFc,1);
FeedbackTorqueNm = repmat(tau,nFc,1);
SpeedRadS = repmat(omega,nFc,1);
EstimatedFrictionTorqueNm = reshape(estimated,rows,1);
QFilterCutoffHz = repelem(cfg.rfob.cutoff_hz(:),n);
AdoptedInertiaJ_kg_m2 = repmat(cfg.inertia_J_kg_m2,rows,1);
ValidFeedbackFlag = repmat(valid,nFc,1);
AnalysisIntervalFlag = repmat(interval,nFc,1);
SettlingStatus = repmat(state,nFc,1);
FeedbackAge_s = repmat(age,nFc,1);
FeedbackSequence = repmat(seq,nFc,1);
TimeFromStepStart_s = repmat(stepElapsed,nFc,1);
results = table(Time_s,Phase,StepIndex,TargetTorqueNm,FeedbackTorqueNm,SpeedRadS, ...
    EstimatedFrictionTorqueNm,QFilterCutoffHz,AdoptedInertiaJ_kg_m2, ...
    ValidFeedbackFlag,AnalysisIntervalFlag,SettlingStatus,FeedbackAge_s, ...
    FeedbackSequence,TimeFromStepStart_s,CommandRowTime_s);
writetable(results,fullfile(cfg.output_dir,'rfob_friction_estimates.csv'));

% Summaries quantify bandwidth sensitivity on the selected moving intervals.
summary = table('Size',[0 9], 'VariableTypes',repmat({'double'},1,9), ...
    'VariableNames',{'CutoffHz','Phase','Direction','SampleCount','MeanNm', ...
    'MedianNm','StdNm','MinNm','MaxNm'});
for b=1:nFc
    for ph=[1 3]
      ix=interval & phase==ph & isfinite(estimated(:,b));
      for dir=[1 -1]
        d=ix & sign(omega)==dir;
        if any(d)
            vals=estimated(d,b);
            summary=[summary; table(cfg.rfob.cutoff_hz(b),ph,dir,sum(d), ...
                mean(vals),median(vals),std(vals),min(vals),max(vals), ...
                'VariableNames',{'CutoffHz','Phase','Direction','SampleCount','MeanNm', ...
                'MedianNm','StdNm','MinNm','MaxNm'})]; %#ok<AGROW>
        end
      end
    end
end
writetable(summary,fullfile(cfg.output_dir,'rfob_bandwidth_summary.csv'));

dt=diff(rowTime); rx=rxRaw*1e-6;
% valid excludes duplicate feedback_sequence rows; diff the remaining unique
% observations so the interval across a repeated sequence is not lost.
rxdt=diff(rx(valid)); rxdt=rxdt(isfinite(rxdt) & rxdt>0);
report=struct();
report.input=cfg.input_csv;
report.inertia_J_kg_m2=cfg.inertia_J_kg_m2;
report.inertia_B_Nm_s_rad=cfg.inertia_B_Nm_s_rad;
report.inertia_source=cfg.inertia_source;
report.motor_model=cfg.motor_model;
report.axis_convention=cfg.axis_convention;
report.no_gear_conversion=true;
report.row_count=n;
report.valid_feedback_count=sum(valid);
report.analysis_interval_count=sum(interval);
report.analysis_interval_time_s=[min(t(interval)),max(t(interval))];
report.feedback_interval_median_s=median(rxdt);
report.feedback_interval_mean_s=mean(rxdt);
report.feedback_interval_max_s=max(rxdt);
report.csv_nominal_interval_median_s=median(dt(isfinite(dt)&dt>0));
activeValid=valid & ismember(phase,[1 3]);
report.target_feedback_difference_median_Nm=median(target(activeValid)-tau(activeValid));
report.target_feedback_difference_median_absolute_Nm=median(abs(target(activeValid)-tau(activeValid)));
report.target_torque_change_count=sum(diff(target)~=0);
report.cutoff_hz=cfg.rfob.cutoff_hz;
report.discretization='exact ZOH: alpha=1-exp(-2*pi*fc*dt), variable feedback_rx_us timestamp dt';
report.analysis_rule='phase 1 or 3; state 1 SETTLING or 2 ACQUIRING; valid fresh feedback; abs(speed)>=0.1 rad/s; nonzero target; 10 ms after command step start; COASTING excluded';
report.time_alignment='Feedback torque and speed share feedback_rx_us; Q-filter dt is the difference in CAN feedback receive timestamps. Target torque is the command logged at actual_us and is retained separately.';
report.torque_note='feedback_torque_estimate_Nm is an internal motor estimate, not an external torque sensor; logged immediately before the command update';
report.physical_caveat='Chirp J is conditional on uncalibrated command torque gain; absolute friction torque inherits this scale uncertainty and possible feedback/command timing and motor-estimate errors.';
report.settings=cfg;
report.source_fric_config=meta.config;
fid=fopen(fullfile(cfg.output_dir,'analysis_metadata.json'),'w');
if fid<0, error('Could not write metadata JSON.'); end
fwrite(fid,jsonencode(report,'PrettyPrint',true),'char'); fclose(fid);
save(fullfile(cfg.output_dir,'analysis_workspace.mat'),'cfg','meta','T','valid', ...
    'interval','estimated','filterState','summary','report');

if cfg.plot.fig1
    f=figure('Name','RFOB friction torque vs speed','Color','w'); hold on; grid on;
    colors=lines(nFc);
    for b=1:nFc
        for dir=[1 -1]
            ix=interval & sign(omega)==dir & isfinite(estimated(:,b));
            scatter(omega(ix),estimated(ix,b),8,colors(b,:), ...
                'filled','MarkerFaceAlpha',0.35, ...
                'DisplayName',sprintf('%g Hz, %s',cfg.rfob.cutoff_hz(b),direction_name(dir)));
        end
    end
    xlabel('Speed [rad/s]'); ylabel('Estimated friction torque [Nm]');
    title('RFOB-style estimate on moving acceleration/settling samples');
    legend('Location','best'); saveas(f,fullfile(cfg.output_dir,'fig1_rfob_torque_vs_speed.png'));
end
if cfg.plot.fig2
    f=figure('Name','RFOB friction time series','Color','w');
    tiledlayout(2,1);
    nexttile; plot(t,tau,'Color',[.25 .25 .25],'DisplayName','Feedback torque'); hold on;
    for b=1:nFc, plot(t,estimated(:,b),'DisplayName',sprintf('RFOB %g Hz',cfg.rfob.cutoff_hz(b))); end
    grid on; ylabel('Torque [Nm]'); legend('Location','best');
    nexttile; plot(t,omega,'b'); grid on; ylabel('Speed [rad/s]'); xlabel('Time [s]');
    saveas(f,fullfile(cfg.output_dir,'fig2_rfob_time_series.png'));
end
if cfg.plot.diag_bandwidth
    f=figure('Name','Q bandwidth comparison','Color','w');
    plot(t,estimated); grid on; xlabel('Time [s]'); ylabel('Estimated friction torque [Nm]');
    legend(compose('%g Hz',cfg.rfob.cutoff_hz),'Location','best');
    saveas(f,fullfile(cfg.output_dir,'diag_bandwidth.png'));
end
if cfg.plot.diag_update_interval
    f=figure('Name','Feedback update interval','Color','w');
    plot(t(2:end),diff(rx)*1e3); grid on; xlabel('Time [s]'); ylabel('Feedback update interval [ms]');
    saveas(f,fullfile(cfg.output_dir,'diag_feedback_update_interval.png'));
end
if cfg.plot.diag_sync
    f=figure('Name','Torque-speed timestamp diagnostics','Color','w');
    plot(rowTime,target,'DisplayName','Target torque at command row'); hold on;
    plot(t,tau,'DisplayName','Feedback torque at CAN receive time');
    yyaxis right; plot(t,omega,'DisplayName','Speed at CAN receive time'); grid on; xlabel('Time [s]');
    legend('Location','best'); saveas(f,fullfile(cfg.output_dir,'diag_torque_speed_sync.png'));
end
if cfg.plot.diag_filter_transient
    f=figure('Name','Filter initialization and segment breaks','Color','w');
    plot(t,estimated); xline(t(find(filterSegment>0,1)),'k--','first valid'); grid on;
    xlabel('Time [s]'); ylabel('Estimated friction torque [Nm]');
    saveas(f,fullfile(cfg.output_dir,'diag_filter_initial_transient.png'));
end
fprintf('Wrote RFOB results to %s\n',cfg.output_dir);
fprintf('Rows %d; valid feedback %d; selected moving samples %d; feedback median dt %.6g s\n', ...
    n,sum(valid),sum(interval),median(rxdt));
disp(summary);

function [T,meta]=read_fric_csv(path)
fid=fopen(path,'r'); if fid<0, error('Cannot open input CSV: %s',path); end
c=onCleanup(@()fclose(fid)); %#ok<NASGU>
headerLine=0; configLine='';
while true
    line=fgetl(fid); if ~ischar(line), error('Data header missing.'); end
    headerLine=headerLine+1;
    if startsWith(strtrim(line),'# config = '), configLine=extractAfter(line,'# config = '); end
    if ~startsWith(strtrim(line),'#'), names=strsplit(strtrim(line),','); break; end
end
frewind(fid);
opts=delimitedTextImportOptions('NumVariables',numel(names));
opts.DataLines=[headerLine+1 Inf]; opts.Delimiter=','; opts.VariableNames=names;
opts.VariableTypes=repmat({'double'},1,numel(names));
opts.ExtraColumnsRule='ignore'; opts.EmptyLineRule='read';
T=readtable(path,opts);
T.Properties.VariableNames=matlab.lang.makeValidName(T.Properties.VariableNames,'ReplacementStyle','delete');
meta=struct('config',jsondecode(configLine),'header_line',headerLine);
end
function s=direction_name(d)
if d>0, s='positive'; else, s='negative'; end
end
