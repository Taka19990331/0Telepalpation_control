function results = compare_identification_inputs(makePlots)
% Compare input definitions using the SAME preprocessing as the original.
% Run: results = compare_identification_inputs;
% Requires Signal Processing Toolbox. No Control System Toolbox required.
% Fs=2000 is deliberately retained for this first, isolated comparison.
% Frequency, phase and physical parameter accuracy remain provisional.
% Measured_Torque_Nm is driver feedback, NOT a verified shaft-torque sensor.
% Direct FRFs in feedback can be biased by correlated measurement noise.
if nargin < 1, makePlots = true; end
root = fileparts(fileparts(mfilename('fullpath')));
names = {'motor_data_20260703_172605.csv', ...
         'motor_data_20260703_170331.csv'};
bands = [1 10; 10 100];
columns = {'Chirp_Torque_Nm','Command_Torque_Nm','Measured_Torque_Nm'};
labels = {'Chirp -> position (closed loop)', ...
          'Total command -> position (direct estimate)', ...
          'Torque feedback -> position (direct estimate)'};
Fs = 2000;
results = struct([]);
for k = 1:numel(names)
    path = fullfile(root,'Data','AK80_Motor1',names{k});
    data = readtable(path);
    required = [columns, {'Position_deg','Time_ms','PD_Torque_Nm'}];
    assert(all(ismember(required,data.Properties.VariableNames)), ...
        'Missing required columns in %s',path);
    t = (0:height(data)-1)'/Fs;
    use = t >= 5;
    y = detrend(unwrap(deg2rad(data.Position_deg(use))));
    inputs = detrend(data{use,columns});
    assert(all(isfinite(y)) && all(isfinite(inputs(:))), ...
        'Nonfinite samples in %s',path);
    winLen = min(4096,floor(numel(y)/4));
    window = hann(winLen);
    overlap = floor(winLen/2);
    for j = 1:3
        [h,f] = tfestimate(inputs(:,j),y,window,overlap,32768,Fs);
        c = mscohere(inputs(:,j),y,window,overlap,32768,Fs);
        if j == 1
            H = complex(zeros(numel(f),3));
            coherence = zeros(numel(f),3);
        end
        H(:,j) = h;
        coherence(:,j) = c;
    end
    % No stitching, coherence rejection or model fit: expose differences.
    band = f >= bands(k,1) & f <= bands(k,2);
    elapsed = (data.Time_ms(end)-data.Time_ms(1))/1000;
    observedRate = (height(data)-1)/elapsed;
    mismatch = abs(data.Command_Torque_Nm-data.PD_Torque_Nm ...
        -data.Chirp_Torque_Nm) > 1e-3;
    fprintf('\n%s\n',names{k});
    fprintf('Assumed Fs: %.1f Hz; timestamp-average rate: %.3f Hz\n', ...
        Fs,observedRate);
    fprintf('Command != PD + chirp: %d / %d rows (tolerance 0.001 Nm)\n', ...
        nnz(mismatch),height(data));
    fprintf('Band %.1f--%.1f Hz; mean coherence [chirp command feedback]:\n', ...
        bands(k,1),bands(k,2));
    disp(mean(coherence(band,:),1));
    results(k).file = path;
    results(k).assumedFs = Fs;
    results(k).timestampAverageFs = observedRate;
    results(k).frequencyHz = f;
    results(k).H = H;
    results(k).coherence = coherence;
    results(k).inputColumns = columns;
    results(k).comparisonBandHz = bands(k,:);
    if makePlots
        figure('Name',names{k},'NumberTitle','off');
        subplot(3,1,1);
        semilogx(f(band),20*log10(abs(H(band,:)))); grid on;
        ylabel('Magnitude [dB, rad/Nm]');
        title([names{k}, ' | assumed Fs = 2000 Hz'],'Interpreter','none');
        legend(labels,'Location','best');
        subplot(3,1,2);
        semilogx(f(band),angle(H(band,:))*180/pi); grid on;
        ylabel('Wrapped phase [deg]');
        subplot(3,1,3);
        semilogx(f(band),coherence(band,:)); grid on;
        yline(0.9,'--'); ylim([0 1.05]);
        ylabel('Coherence'); xlabel('Frequency [Hz, provisional]');
    end
end
fprintf(['\nDiagnostic comparison only: no physical J/D estimates.\n', ...
    'Feedback torque scaling/timing and actual sampling remain unverified.\n']);
end
