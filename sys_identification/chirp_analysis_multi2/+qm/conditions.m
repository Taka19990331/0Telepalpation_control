% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
function c = conditions(M)
assert(qm.textValue(M,'FORMAT')=="CHIRPSI1_CSV_WITH_METADATA_V1", ...
    'Unsupported metadata format; no experiment defaults will be substituted.');
assert(qm.textValue(M,'CHIRP_TYPE')=="logarithmic",'Only documented logarithmic sweeps supported.');
assert(qm.textValue(M,'REFERENCE_VALID_FOR_PD')=="True",'CSV has no valid PD reference.');
assert(qm.textValue(M,'RESULT.valid')=="True" && qm.textValue(M,'RESULT.complete')=="True", ...
    'Capture invalid/incomplete. Inspect CSV result metadata; no automatic fit.');
assert(qm.number(M,'CONFIG.MotorProtocol')==0,'Only verified AK80 standard MIT coordinate convention supported.');
assert(qm.number(M,'CONFIG.NoMotor')==0,'Synthetic/no-motor capture is not physical identification.');
assert(qm.number(M,'CONFIG.PositionPolicy')==0,'Unknown position policy; cannot unwrap safely.');
assert(qm.textValue(M,'PD_TORQUE_FORMULA')=="tau_PD = Kp*(theta_ref-theta) - Kd*omega", ...
    'Unrecognized PD law.');
assert(qm.textValue(M,'POSITION_DEFINITION')=="theta_ref and theta [deg]; omega [rad/s]", ...
    'Unrecognized angle/speed units.');
assert(qm.textValue(M,'CHIRP_TORQUE_FORMULA')=="tau_chirp(t) = A(t)*sin(2*pi*f0*(exp(k*t)-1)/k)" ...
    && qm.textValue(M,'AMPLITUDE_FORMULA')=="A(t) = A0 + (A1-A0)*x(t)^a", ...
    'Unrecognized chirp phase/amplitude definition; cannot reconstruct or demodulate safely.');
c.fs=qm.number(M,'SAMPLING_FREQUENCY_HZ (fs)');
c.f0=qm.number(M,'START_FREQUENCY_HZ (f0)');
c.f1=qm.number(M,'END_FREQUENCY_HZ (f1)');
c.T=qm.number(M,'SWEEP_DURATION_S (T)');
c.A0=qm.number(M,'START_AMPLITUDE_NM (A0)');
c.A1=qm.number(M,'END_AMPLITUDE_NM (A1)');
c.a=qm.number(M,'AMPLITUDE_EXPONENT (a)');
c.Kp=qm.number(M,'KP_PD (Kp) [Nm/deg]')*180/pi;
c.Kd=qm.number(M,'KD_PD (Kd) [Nm/(rad/s)]');
c.q0=qm.number(M,'REFERENCE_POSITION_DEG (theta_ref)')*pi/180;
c.start=qm.number(M,'CHIRP_START_RECORD_TIME_S (t_start)');
c.startIndex=qm.number(M,'SWEEP_START_INDEX (sweep_start_index)');
c.limit=qm.number(M,'TORQUE_LIMIT_NM (tau_limit)');
c.feedbackTimeout=qm.number(M,'CONFIG.FeedbackTimeout_us')*1e-6;
c.maxSpeed=qm.number(M,'CONFIG.VelocityMax_rad_s');
c.positionMin=qm.number(M,'CONFIG.PositionMin_rad');
c.positionMax=qm.number(M,'CONFIG.PositionMax_rad');
c.torqueEncodingMax=qm.number(M,'CONFIG.MITTorqueMax_Nm');
c.preDuration=qm.number(M,'PRE_HOLD_DURATION_S (T_pre)');
c.postDuration=qm.number(M,'POST_HOLD_DURATION_S (T_post)');
assert(c.fs>0 && c.f1>c.f0 && c.f0>0 && c.f1<c.fs/2 && c.T>0 && c.Kp>0 && c.Kd>=0, ...
    'Invalid sampling, sweep or gain conditions.');
c.k=log(c.f1/c.f0)/c.T;
c.axisConvention = 'AK80 MIT position/velocity and Nm command used directly; no additional gear conversion.';
c.torqueCalibration = 'Firmware performs no physical torque calibration; output-shaft Nm interpretation is conditional.';
end


