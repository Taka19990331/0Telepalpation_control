
%% vel_polePlace_test1.m
% Speed PI control for friction identification
% MIT torque mode / 2 kHz / 5 Hz sine tracking
%
% Design:
%   Continuous-time pole placement
%   Discrete-time PI implementation
%   Dead-time approximation with Pade
%
% Requires Control System Toolbox

clear; clc; close all;

%% =========================================================
% 1. Identified motor parameters
% ==========================================================

J  = 0.0116806;       % [kg*m^2]
B  = 0.0222278;       % [Nm*s/rad]
Td = 0.001477;        % [s]

% Controller sampling
fs = 2000;            % [Hz]
Ts = 1/fs;            % [s]

%% =========================================================
% 2. PI design parameters
% ==========================================================

% Desired closed-loop natural frequency
fn = 10.0;            % [Hz]

% Damping ratio
zeta = 1.0;

% Dead-time approximation
pade_order = 5;

%% =========================================================
% 3. Reference signal
% ==========================================================

% Target tracking frequency
f_ref = 5.0;          % [Hz]

% Sinusoidal velocity amplitude
omega_amp = 1.0;      % [rad/s]

% Velocity offset
omega_offset = 0.0;   % [rad/s]

% Smooth amplitude ramp
T_ramp = 0.5;         % [s]

% Total simulation time
T_end = 3.0;          % [s]

% Evaluation starts after transient
T_eval = 1.0;         % [s]

% Unknown torque limit: linear model
tau_max = Inf;

%% =========================================================
% 4. Pole placement
% ==========================================================

wn = 2*pi*fn;

Kp = 2*zeta*wn*J - B;
Ki = J*wn^2;

if Kp <= 0 || Ki <= 0
    error('Invalid PI gains.');
end

% Integral gain per control step
KiTs = Ki*Ts;

fprintf('\n=== PI Gain Design ===\n');
fprintf('Sampling frequency: %.1f Hz\n',fs);
fprintf('Sampling period: %.6f s\n',Ts);

fprintf('Design fn: %.3f Hz\n',fn);
fprintf('Damping ratio: %.3f\n',zeta);

fprintf('Kp: %.9f\n',Kp);
fprintf('Ki: %.9f\n',Ki);
fprintf('Ki*Ts: %.9f\n',KiTs);

fprintf('\nDesired nominal poles:\n');
disp(roots([1 2*zeta*wn wn^2]));

%% =========================================================
% 5. Continuous-time plant
% ==========================================================

s = tf('s');

G0 = 1/(J*s+B);

% Pade approximation of dead time
[numD,denD] = pade(Td,pade_order);

Delay = tf(numD,denD);

G_pade = G0*Delay;

%% =========================================================
% 6. Discrete-time model
% ==========================================================

% ZOH discretization of plant with Pade delay
Gd = c2d(G_pade,Ts,'zoh');

% Discrete PI:
% I[k] = I[k-1] + Ki*Ts*e[k]
% tau[k] = Kp*e[k] + I[k]

Cd = tf([Kp+KiTs, -Kp], ...
        [1, -1], Ts);

% Open-loop transfer function
Ld = Cd*Gd;

% Closed-loop velocity/reference
T_vel = feedback(Ld,1);

% Torque command/reference
T_tau = feedback(Cd,Gd);

%% =========================================================
% 7. Stability evaluation
% ==========================================================

fprintf('\n=== Stability Analysis ===\n');

stable = isstable(T_vel);

fprintf('Closed-loop stable: %d\n',stable);

[GM,PM,Wcg,Wcp] = margin(Ld);

fprintf('Phase margin: %.3f deg\n',PM);

if isfinite(GM)
    fprintf('Gain margin: %.3f dB\n', ...
        20*log10(GM));
else
    fprintf('Gain margin: Inf\n');
end

if isfinite(Wcp)
    fprintf('Gain crossover: %.3f Hz\n', ...
        Wcp/(2*pi));
end

if isfinite(Wcg)
    fprintf('Phase crossover: %.3f Hz\n', ...
        Wcg/(2*pi));
end

if ~stable
    error('Closed-loop unstable. Stop simulation.');
end

if isnan(PM) || PM < 45
    warning('Phase margin is below 45 deg or undefined.');
end

%% =========================================================
% 8. Generate 5 Hz reference
% ==========================================================

t = (0:Ts:T_end)';
N = length(t);

ref = zeros(N,1);

for k = 1:N

    r = min(t(k)/T_ramp,1);

    % Quintic smooth transition
    h = 10*r^3 - 15*r^4 + 6*r^5;

    ref(k) = omega_offset + ...
        omega_amp*h*sin(2*pi*f_ref*t(k));

end

%% =========================================================
% 9. Closed-loop simulation
% ==========================================================

omega = lsim(T_vel,ref,t);

tau = lsim(T_tau,ref,t);

% Velocity tracking error
error_vel = ref-omega;

% Control torque difference
dtau = diff(tau);

%% =========================================================
% 10. Steady-state evaluation
% ==========================================================

idx = t >= T_eval;

t_eval = t(idx);
ref_eval = ref(idx);
omega_eval = omega(idx);
tau_eval = tau(idx);

err_eval = ref_eval-omega_eval;

rms_err = sqrt(mean(err_eval.^2));

max_err = max(abs(err_eval));

rms_tau = sqrt(mean(tau_eval.^2));

max_tau = max(abs(tau_eval));

% Frequency-response evaluation at 5 Hz
H = freqresp(T_vel,2*pi*f_ref);
H = H(1);

gain_ratio = abs(H);
phase_deg = angle(H)*180/pi;

fprintf('\n=== 5 Hz Tracking Performance ===\n');

fprintf('Reference frequency: %.3f Hz\n',f_ref);
fprintf('Reference amplitude: %.3f rad/s\n', ...
    omega_amp);

fprintf('Gain ratio: %.5f\n',gain_ratio);
fprintf('Phase shift: %.3f deg\n',phase_deg);

fprintf('RMS velocity error: %.6f rad/s\n', ...
    rms_err);

fprintf('Maximum velocity error: %.6f rad/s\n', ...
    max_err);

fprintf('RMS torque: %.6f Nm\n',rms_tau);
fprintf('Maximum torque: %.6f Nm\n',max_tau);

if isfinite(tau_max) && max(abs(tau)) > tau_max
    warning('Torque command exceeds selected limit.');
end

%% =========================================================
% 11. Plot velocity tracking
% ==========================================================

figure('Name','5 Hz Velocity Tracking', ...
       'Color','w');

plot(t,ref,'k--','LineWidth',1.2);
hold on;

plot(t,omega,'b','LineWidth',1.5);

grid on;

xlabel('Time [s]');
ylabel('Angular velocity [rad/s]');

legend('Reference','Motor response', ...
       'Location','best');

title(sprintf( ...
    '5 Hz Tracking: fn = %.1f Hz, zeta = %.2f', ...
    fn,zeta));

%% =========================================================
% 12. Plot tracking error
% ==========================================================

figure('Name','Velocity Tracking Error', ...
       'Color','w');

plot(t,error_vel,'LineWidth',1.4);

grid on;

xlabel('Time [s]');
ylabel('Velocity error [rad/s]');

title(sprintf( ...
    'Steady RMS Error = %.5f rad/s',rms_err));

%% =========================================================
% 13. Plot torque command
% ==========================================================

figure('Name','PI Torque Command', ...
       'Color','w');

plot(t,tau,'LineWidth',1.4);

grid on;

xlabel('Time [s]');
ylabel('Torque command [Nm]');

title('MIT Mode Torque Command');

%% =========================================================
% 14. Frequency response of closed-loop
% ==========================================================

f = logspace(-1,2.5,800);

Hf = squeeze(freqresp(T_vel,2*pi*f));

mag_db = 20*log10(abs(Hf));

phase = unwrap(angle(Hf))*180/pi;

figure('Name','Closed-loop Frequency Response', ...
       'Color','w');

subplot(2,1,1);

semilogx(f,mag_db,'LineWidth',1.5);
hold on;
xline(f_ref,'r--','5 Hz');

grid on;
ylabel('Magnitude [dB]');
title('Closed-loop Frequency Response');

subplot(2,1,2);

semilogx(f,phase,'LineWidth',1.5);
hold on;
xline(f_ref,'r--','5 Hz');

grid on;
xlabel('Frequency [Hz]');
ylabel('Phase [deg]');

%% =========================================================
% 15. Open-loop stability margins
% ==========================================================

figure('Name','Open-loop Stability Margins');

margin(Ld);
grid on;

%% =========================================================
% 16. Discrete closed-loop poles
% ==========================================================

figure('Name','Discrete Closed-loop Poles');

pzmap(T_vel);
grid on;

%% =========================================================
% 17. Export PI parameters
% ==========================================================

PI.Kp = Kp;
PI.Ki = Ki;
PI.KiTs = KiTs;
PI.fs = fs;
PI.Ts = Ts;
PI.fn = fn;
PI.zeta = zeta;

PI.phase_margin_deg = PM;
PI.gain_ratio_5Hz = gain_ratio;
PI.phase_5Hz_deg = phase_deg;

save('vel_PI_design.mat','PI');

fprintf('\n=== Design Completed ===\n');
fprintf('Parameters saved: vel_PI_design.mat\n');
