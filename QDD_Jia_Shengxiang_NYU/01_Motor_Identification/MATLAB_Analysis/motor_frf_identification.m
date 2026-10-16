clear; clc; close all;

files = {
    'C:\Users\123\Desktop\data\data\motor_data_20260703_172605.csv', [1 20],   [1 10];
    'C:\Users\123\Desktop\data\data\motor_data_20260703_170331.csv', [10 120], [10 100];
};

cohLimit = 0.9;

f_all = [];
P_all = [];
coh_all = [];

f_coh_all = [];
f_raw_all = [];
H_raw_all = [];
coh_raw_all = [];

%% FRF calculation for each file
for k = 1:size(files, 1)
    csvFile = files{k, 1};
    measuredBand = files{k, 2};
    fitBand = files{k, 3};

    data = readtable(csvFile);

    Fs = 2000;
    Ts = 1 / Fs;
    t = (0:height(data)-1)' * Ts;

    t_start = 5;
    t_end = t(end) ;

    idx = t >= t_start & t <= t_end;

    u = data.Chirp_Torque_Nm(idx);
    y = unwrap(deg2rad(data.Position_deg(idx)));

    u = detrend(u);
    y = detrend(y);

    N = length(u);
    winLen = min(4096, floor(N/4));
    window = hann(winLen);
    noverlap = floor(winLen/2);
    nfft = 32768;

    [H, f] = tfestimate(u, y, window, noverlap, nfft, Fs);
    coh = mscohere(u, y, window, noverlap, nfft, Fs);

    fitIdx = f >= fitBand(1) & f <= fitBand(2);

    % Remove splice area around 10 Hz
    badBand = f >= 9.5 & f <= 10.5;

    reliable = fitIdx & coh >= cohLimit & ~badBand;

    f_raw_all = [f_raw_all; f(fitIdx)];
    H_raw_all = [H_raw_all; H(fitIdx)];
   
    fprintf("\n===== File %d =====\n", k);
    fprintf("File: %s\n", csvFile);
    fprintf("Measured band: %.1f - %.1f Hz\n", measuredBand(1), measuredBand(2));
    fprintf("Fitting band: %.1f - %.1f Hz\n", fitBand(1), fitBand(2));
    fprintf("Selected data length: %.2f s\n", t(find(idx,1,'last')) - t(find(idx,1,'first')));
    fprintf("Mean coherence in fitting band = %.4f\n", mean(coh(fitIdx)));
    fprintf("Reliable points = %d\n", sum(reliable));

    f_all = [f_all; f(reliable)];
    P_all = [P_all; H(reliable)];
    coh_all = [coh_all; coh(reliable)];
    f_coh_all = [f_coh_all; f(fitIdx)];
    coh_raw_all = [coh_raw_all; coh(fitIdx)];
end
%% Plot combined FRF: magnitude, phase, coherence

[f_sort, sortIdx] = sort(f_raw_all);
H_sort = H_raw_all(sortIdx);
coh_sort = coh_raw_all(sortIdx);

plotRange = f_sort >= 1 & f_sort <= 100;

mag_db = 20 * log10(abs(H_sort));
phase_deg = angle(H_sort) * 180/pi;   

% keep wrapped phase like single FRF plot

figure;

subplot(3,1,1);
semilogx(f_sort(plotRange), mag_db(plotRange), "LineWidth", 1.4);
grid on;
ylabel("Magnitude [dB]");
title("FRF: position / chirp torque");

subplot(3,1,2);
semilogx(f_sort(plotRange), phase_deg(plotRange), "LineWidth", 1.4);
grid on;
ylabel("Phase [deg]");

subplot(3,1,3);
semilogx(f_sort(plotRange), coh_sort(plotRange), "LineWidth", 1.4);
hold on;
yline(0.9, "--r");
grid on;
ylabel("Coherence");
xlabel("Frequency [Hz]");
ylim([0 1.05]);

%% Fit equivalent plant P(s) = b / (s^2 + a*s)
% This treats inner PD + motor as one equivalent open-loop plant.
% J = 1/b, D = a/b

s = tf("s");
%% Identify plant with equivalent delay
% P(s) = b / (s^2 + a*s) * exp(-Td*s)

w_all = 2*pi*f_all;
jw_all = 1i*w_all;

fitUse = (f_all >= 1 & f_all <= 100);
ff = f_all(fitUse);
ww = 2*pi*ff;
jw = 1i*ww;
P_meas = P_all(fitUse);

mag_meas = 20*log10(abs(P_meas));
phase_meas = angle(P_meas) * 180/pi;
phase_meas(phase_meas > 90) = phase_meas(phase_meas > 90) - 360;

weight = ones(size(ff));
weight(ff < 10) = 0.2;     % low frequency less important
weight(ff >= 10) = 1.0;    % high frequency more important

fitCost = @(x) fitPlantDelayCost(x, jw, P_meas, mag_meas, phase_meas, weight);

% x = [log(b), log(a), log(Td)]
x0 = [log(200), log(50), log(0.003)];
x = fminsearch(fitCost, x0);

b = exp(x(1));
a = exp(x(2));
Td = exp(x(3));

s = tf("s");
P_no_delay = b / (s^2 + a*s);
P = P_no_delay;
P.InputDelay = Td;

J = 1 / b;
D = a / b;

fprintf("\n===== Identified plant with delay =====\n");
fprintf("P(s) = %.6g / (s^2 + %.6g s) * exp(-%.6g s)\n", b, a, Td);
fprintf("J = %.10f\n", J);
fprintf("D = %.10f\n", D);
fprintf("Td = %.10f s\n", Td);

%% Plot fitting result
[f_plot, sortIdx] = sort(ff);
P_plot = P_meas(sortIdx);

P_fit = b ./ ((1i*2*pi*f_plot).^2 + a*(1i*2*pi*f_plot)) .* exp(-(1i*2*pi*f_plot)*Td);

figure;

subplot(2,1,1);
semilogx(f_plot, 20*log10(abs(P_plot)), ".b"); hold on;
semilogx(f_plot, 20*log10(abs(P_fit)), ".r");
grid on;
ylabel("Magnitude [dB]");
title("Plant fitting result");
legend("Recovered plant FRF", "Fitted P(s)");

subplot(2,1,2);
phase_plot = angle(P_plot) * 180/pi;
phase_fit = angle(P_fit) * 180/pi;

phase_plot(phase_plot > 90) = phase_plot(phase_plot > 90) - 360;
phase_fit(phase_fit > 90) = phase_fit(phase_fit > 90) - 360;

semilogx(f_plot, phase_plot, ".b"); hold on;
semilogx(f_plot, phase_fit, ".r");
grid on;
ylabel("Phase [deg]");
xlabel("Frequency [Hz]");
legend("Recovered plant FRF", "Fitted P(s)");

function cost = fitPlantDelayCost(x, jw, P_meas, mag_meas, phase_meas, weight)
    b = exp(x(1));
    a = exp(x(2));
    Td = exp(x(3));

    P_model = b ./ (jw.^2 + a*jw) .* exp(-jw*Td);

    mag_model = 20*log10(abs(P_model));
    phase_model = angle(P_model) * 180/pi;
    phase_model(phase_model > 90) = phase_model(phase_model > 90) - 360;

    magErr = mag_model - mag_meas;
    phaseErr = phase_model - phase_meas;

    cost = sum(weight .* (magErr.^2 + 0.05*phaseErr.^2));
end

%% Design position controller C(s) by pole placement
% Desired poles:
% (s^2 + 2*zeta1*wa*s + wa^2)(s^2 + 2*zeta2*wb*s + wb^2)

M = J;
C_damp = D;
K = 0;

% Try pole placement settings
f1 = 4;      % [Hz], omega alpha
f2 = 20;      % [Hz], omega beta
zeta1 = 0.8;
zeta2 = 0.8;

wa = 2*pi*f1;
wb = 2*pi*f2;

A3 = 2*zeta1*wa + 2*zeta2*wb;
A2 = wa^2 + wb^2 + 4*zeta1*zeta2*wa*wb;
A1 = 2*zeta1*wa*wb^2 + 2*zeta2*wb*wa^2;
A0 = wa^2 * wb^2;

ac1 = A3 - C_damp/M;

bc2 = M*A2 - ac1*C_damp - K;
bc1 = M*A1 - ac1*K;
bc0 = M*A0;

Cpos = (bc2*s^2 + bc1*s + bc0) / (s^2 + ac1*s);

fprintf("\n===== Position controller C(s) =====\n");
fprintf("f1 = %.2f Hz, f2 = %.2f Hz\n", f1, f2);
fprintf("zeta1 = %.2f, zeta2 = %.2f\n", zeta1, zeta2);
fprintf("ac1 = %.8g\n", ac1);
fprintf("bc2 = %.8g\n", bc2);
fprintf("bc1 = %.8g\n", bc1);
fprintf("bc0 = %.8g\n", bc0);

disp("Cpos(s):");
Cpos

%% Evaluation with delay approximation
% P has internal delay, so use Pade approximation for pole/step/bode/nyquist/margin/Ms

P_eval = pade(P, 4);

L_eval = P_eval * Cpos;
S_eval = 1 / (1 + L_eval);
T_eval = 45*L_eval / (1 + L_eval);

disp("Closed-loop poles with delay approximation:");
pole(T_eval)

figure;
step(T_eval);
grid on;
title("Step response");

figure;
bode(S_eval);
grid on;
title("Sensitivity function S(s)");

figure;
w_nyq = 2*pi*logspace(0, 2, 1000);   % 1-100 Hz
nyquist(L_eval, w_nyq);
grid on;
title("Nyquist plot of L(s)=P(s)C(s), 1-100 Hz");

info = stepinfo(T_eval);
[GM, PM, Wcg, Wcp] = margin(L_eval);
Ms = getPeakGain(S_eval);

fprintf("Overshoot = %.2f %%\n", info.Overshoot);
fprintf("Settling time = %.4f s\n", info.SettlingTime);
fprintf("Sensitivity peak Ms = %.4f\n", Ms);
fprintf("Phase margin = %.2f deg\n", PM);

%% Convert C(s) to discrete controller for Teensy

Ts_ctrl = 1/3000;   % 2 kHz control
Cd = c2d(Cpos, Ts_ctrl, "tustin");

[num, den] = tfdata(Cd, "v");

num = num / den(1);
den = den / den(1);

fprintf("\n===== Discrete controller C(z) =====\n");
fprintf("B0 = %.10g\n", num(1));
fprintf("B1 = %.10g\n", num(2));
fprintf("B2 = %.10g\n", num(3));
fprintf("A1 = %.10g\n", den(2));
fprintf("A2 = %.10g\n", den(3));