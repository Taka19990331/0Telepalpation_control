function results = diagnose_identification_data()
SHOW_WRAPPED_PHASE = false; % true: add the wrapped-phase diagnostic panel.
% Diagnose the existing two Motor1 experiments; no model identification.
% Run from this directory: results = diagnose_identification_data;
% Uses compare_identification_inputs unchanged for a reproducible baseline.
% Requires Signal Processing Toolbox. Writes PNG/CSV/MAT/log to diagnostics.
here = fileparts(mfilename('fullpath'));
out = fullfile(here,'diagnostics');
if ~exist(out,'dir'), mkdir(out); end
diary(fullfile(out,'execution_log.txt'));
cleanup = onCleanup(@() diary('off'));
results = compare_identification_inputs(false);
stats = cell(1,numel(results)); jumps = struct([]); nj = 0;
labels = {'1: Chirp cmd only input','2: PD + chirp cmd input', ...
          '3: Measured torque input'};
for k = 1:numel(results)
    r = results(k); d = readtable(r.file);
    [~,name] = fileparts(r.file);
    t = (double(d.Time_ms)-double(d.Time_ms(1)))/1000;
    dt = diff(t); n = height(d);
    values = d{:,:};
    assert(all(isfinite(values(:))), 'Nonfinite CSV data: %s',r.file);
    assert(all(dt >= 0),'Nonmonotonic time: %s',r.file);
    b = r.frequencyHz >= r.comparisonBandHz(1) & r.frequencyHz <= r.comparisonBandHz(2);
    f = r.frequencyHz(b); H = r.H(b,:); coh = r.coherence(b,:);
    wrapped = angle(H)*180/pi;
    continuous = unwrap(angle(H),[],1)*180/pi;
    % Choose an initial branch in (-360,0] for visual comparison only.
    % A constant 360-degree shift leaves the complex FRF unchanged.
    continuous = continuous-360*ceil(continuous(1,:)/360);
    % Unwrap each file/input independently within the plotted band only.
    % It selects a continuous branch, not an absolute phase calibration.
    assert(max(abs(exp(1i*continuous*pi/180)-exp(1i*wrapped*pi/180)),[],'all') < 1e-10);
    for j = 1:3
        ix = find(abs(diff(wrapped(:,j))) > 180);
        for q = ix'
            nj = nj+1;
            jumps(nj).File = string(name);
            jumps(nj).Input = string(labels{j});
            jumps(nj).From_Hz = f(q);
            jumps(nj).To_Hz = f(q+1);
            jumps(nj).RawJump_deg = wrapped(q+1,j)-wrapped(q,j);
            jumps(nj).ContinuousStep_deg = continuous(q+1,j)-continuous(q,j);
            jumps(nj).MinCoherence = min(coh(q:q+1,j));
        end
    end
    tab = table(f,'VariableNames',{'Frequency_Hz'});
    for j = 1:3
        prefix = sprintf('Input%d_',j);
        tab.([prefix 'Magnitude_dB']) = 20*log10(abs(H(:,j)));
        tab.([prefix 'WrappedPhase_deg']) = wrapped(:,j);
        tab.([prefix 'ContinuousPhase_deg']) = continuous(:,j);
        tab.([prefix 'Coherence']) = coh(:,j);
    end
    writetable(tab,fullfile(out,[name '_frf.csv']));
    fig = figure('Visible','off','Position',[50 50 1300 1000]);
    tiledlayout(3+double(SHOW_WRAPPED_PHASE),1,'TileSpacing','compact');
    nexttile; semilogx(f,20*log10(abs(H)),'LineWidth',1); grid on;
    ylabel('Magnitude [dB rad/Nm]'); legend(labels,'Location','southwest');
    title({[name ' | baseline Fs=2000 Hz (provisional)'], ...
        'Measured torque: driver feedback; scaling/timing unverified'},'Interpreter','none');
    if SHOW_WRAPPED_PHASE
        nexttile; semilogx(f,wrapped); grid on; ylabel('Wrapped phase [deg]');
    end
    nexttile; semilogx(f,continuous); grid on; ylabel('Phase [deg]');
    nexttile; semilogx(f,coh); grid on; yline(0.9,'--'); ylim([0 1.05]);
    ylabel('Coherence'); xlabel('Frequency [Hz]');
    exportgraphics(fig,fullfile(out,[name '_frf.png']),'Resolution',140); close(fig);

    active = find(abs(d.Chirp_Torque_Nm)>1e-4);
    residual = d.Command_Torque_Nm-d.PD_Torque_Nm-d.Chirp_Torque_Nm;
    posStep = diff(d.Position_deg);
    [maxStep,stepIdx] = max(abs(posStep));
    [maxGap,gapIdx] = max(dt);
    s = struct;
    s.File = string(name); s.Rows = n; s.Duration_s = t(end);
    s.TimestampAverage_Hz = (n-1)/t(end);
    s.Assumed2000HzDuration_s = (n-1)/2000;
    s.ZeroDtRows = nnz(dt==0); s.NegativeDtRows = nnz(dt<0);
    s.MaxGap_ms = maxGap*1000; s.MaxGapAt_s = t(gapIdx);
    s.ChirpStart_s = t(active(1)); s.ChirpEnd_s = t(active(end));
    s.ChirpAmplitude_Nm = max(abs(d.Chirp_Torque_Nm));
    s.CommandMaxAbs_Nm = max(abs(d.Command_Torque_Nm));
    s.TorqueFeedbackMaxAbs_Nm = max(abs(d.Measured_Torque_Nm));
    s.SumMismatchRows = nnz(abs(residual)>1e-3);
    s.SumResidualMax_Nm = max(abs(residual));
    s.NonzeroErrorRows = nnz(d.Error_Code~=0);
    s.PositionMin_deg = min(d.Position_deg); s.PositionMax_deg = max(d.Position_deg);
    s.MaxPositionStep_deg = maxStep; s.MaxPositionStepAt_s = t(stepIdx);
    s.MaxSpeed_rad_s = max(abs(d.Speed_rad_s));
    s.MaxTemperature_C = max(d.Temperature_C);
    % Infer logged PD coefficients as a consistency check, not plant fitting.
    % Exclude startup and no-chirp intervals, where logging may show zero torque.
    fitRows = t >= t(active(1))+0.1 & t <= t(active(end));
    A = [ones(nnz(fitRows),1),d.Position_deg(fitRows),d.Speed_rad_s(fitRows)];
    beta = A\d.PD_Torque_Nm(fitRows);
    s.InferredKp_Nm_per_deg = -beta(2); s.InferredKd_Nm_per_rad_s = -beta(3);
    s.PDRegressionRms_Nm = rms(A*beta-d.PD_Torque_Nm(fitRows));
    fprintf('\nDIAGNOSTICS %s\n',name); disp(struct2table(s));

    % Preserve extrema in overview: plot per-time-bin min/max, not decimation.
    edges = linspace(0,t(end)+eps(t(end)),1801);
    group = discretize(t,edges); centers = (edges(1:end-1)+edges(2:end))/2;
    series = {d.Position_deg,d.Speed_rad_s, ...
        [d.Chirp_Torque_Nm,d.PD_Torque_Nm,d.Command_Torque_Nm,d.Measured_Torque_Nm]};
    fig = figure('Visible','off','Position',[50 50 1300 1000]);
    tiledlayout(4,1,'TileSpacing','compact');
    units = {'Position [deg]','Speed [rad/s]','Torque [Nm]'};
    for p = 1:3
        nexttile; hold on;
        colors = lines(size(series{p},2));
        for j = 1:size(series{p},2)
            lo = accumarray(group,series{p}(:,j),[1800 1],@min,NaN);
            hi = accumarray(group,series{p}(:,j),[1800 1],@max,NaN);
            plot(centers,lo,'Color',colors(j,:));
            plot(centers,hi,'Color',colors(j,:),'HandleVisibility','off');
        end
        grid on; ylabel(units{p});
        if p==1, title([name ' | raw time; min/max envelopes'],'Interpreter','none'); end
        if p==3, legend('Chirp','PD','Total command','Torque feedback'); end
    end
    nexttile; plot(t(2:end),dt*1000,'.'); grid on;
    ylabel('Timestamp step [ms]'); xlabel('Elapsed timestamp [s]');
    exportgraphics(fig,fullfile(out,[name '_timeseries.png']),'Resolution',140); close(fig);

    % Full-resolution early-chirp and largest-position-step windows.
    starts = [t(active(1))+1,max(0,t(stepIdx)-0.1)];
    lengths = [2,0.2];
    fig = figure('Visible','off','Position',[50 50 1400 900]);
    tiledlayout(3,2,'TileSpacing','compact');
    for p=1:3
        for z=1:2
            nexttile; sel=t>=starts(z) & t<=starts(z)+lengths(z);
            plot(t(sel),series{p}(sel,:),'.-','MarkerSize',3); grid on;
            ylabel(units{p}); xlabel('Elapsed timestamp [s]');
            if p==1
                if z==1, title('Early chirp: original samples');
                else, title('Largest position step: original samples'); end
            end
            if p==3, legend('Chirp','PD','Total command','Torque feedback'); end
        end
    end
    exportgraphics(fig,fullfile(out,[name '_zoom.png']),'Resolution',140); close(fig);
    % Kinematic consistency: average duplicate millisecond stamps, integrate
    % on ALL unique timestamps. Do not downsample before integration.
    [tu,~,g] = unique(t);
    theta = accumarray(g,deg2rad(d.Position_deg),[],@mean);
    vel = accumarray(g,d.Speed_rad_s,[],@mean);
    tg = tu; pg = theta; vg = vel;
    integrated = cumtrapz(tg,vg);
    fig = figure('Visible','off','Position',[50 50 1200 650]);
    tiledlayout(2,1);
    nexttile; plot(tg,[pg-pg(1),integrated]); grid on;
    legend('Position change','Integrated logged velocity'); ylabel('Angle [rad]');
    title([name ' | unique timestamps; consistency diagnostic only'],'Interpreter','none');
    nexttile; plot(tg,pg-pg(1)-integrated); grid on;
    xlabel('Elapsed timestamp [s]'); ylabel('Difference [rad]');
    exportgraphics(fig,fullfile(out,[name '_kinematics.png']),'Resolution',140); close(fig);
    s.IntegratedVelocityResidualEnd_rad = pg(end)-pg(1)-integrated(end);
    s.IntegratedVelocityResidualMax_rad = max(abs(pg-pg(1)-integrated));
    % Direct diagnostic of logged torque feedback versus total command.
    use = (0:n-1)'/2000 >= 5;
    u = detrend(d.Command_Torque_Nm(use));
    v = detrend(d.Measured_Torque_Nm(use));
    w = hann(4096);
    [ht,ft] = tfestimate(u,v,w,2048,32768,2000);
    ct = mscohere(u,v,w,2048,32768,2000);
    bt = ft>=r.comparisonBandHz(1) & ft<=r.comparisonBandHz(2);
    torqueTable = table(ft(bt),abs(ht(bt)),angle(ht(bt))*180/pi,ct(bt), ...
        'VariableNames',{'Frequency_Hz','FeedbackPerCommand','Phase_deg','Coherence'});
    writetable(torqueTable,fullfile(out,[name '_torque_transfer.csv']));
    s.TorqueFeedbackPerCommandMedian = median(abs(ht(bt)));
    s.TorqueFeedbackPerCommandMin = min(abs(ht(bt)));
    s.TorqueFeedbackPerCommandMax = max(abs(ht(bt)));
    fig = figure('Visible','off','Position',[50 50 1200 800]);
    tiledlayout(3,1);
    nexttile; semilogx(ft(bt),abs(ht(bt))); grid on; ylabel('Feedback / command');
    title([name ' | logged torque transfer (not calibration)'],'Interpreter','none');
    nexttile; semilogx(ft(bt),angle(ht(bt))*180/pi); grid on; ylabel('Phase [deg]');
    nexttile; semilogx(ft(bt),ct(bt)); grid on; ylim([0 1.05]);
    ylabel('Coherence'); xlabel('Frequency [Hz; assumed Fs=2000]');
    exportgraphics(fig,fullfile(out,[name '_torque_transfer.png']),'Resolution',140); close(fig);
    stats{k} = s;
end
stats = [stats{:}];
writetable(struct2table(stats),fullfile(out,'summary.csv'));
if ~isempty(jumps)
    jumpTable = struct2table(jumps); disp(jumpTable);
    writetable(jumpTable,fullfile(out,'phase_jumps.csv'));
else
    fprintf('No >180 degree wrapped steps in the comparison bands.\n');
end
save(fullfile(out,'diagnostic_results.mat'),'results','stats','jumps');
fprintf('Outputs: %s\nNo physical model fitting performed.\n',out);
end
