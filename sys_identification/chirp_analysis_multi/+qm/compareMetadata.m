function reasons = compareMetadata(M,R)
% Compare acquisition/control settings, not result strings or reference position.
ignored=["CONFIG.Hold_deg","CONFIG.ReferenceMin_rad","CONFIG.ReferenceMax_rad", ...
    "CONFIG.ReferenceSamples","CONFIG.AverageRows","CONFIG.ExpectedRows","CONFIG.RowBytes"];
keys=R.Key(startsWith(R.Key,"CONFIG.") & ~ismember(R.Key,ignored));
keys=unique([keys;R.Key(contains(R.Key,"FORMULA") | contains(R.Key,"POLICY") | ...
    contains(R.Key,"DEFINITION")); ...
    "CHIRP_TYPE";"CHIRP_INTERVAL";"TORQUE_LIMIT_ACTION"; ...
    "START_FREQUENCY_HZ (f0)";"END_FREQUENCY_HZ (f1)"; ...
    "START_AMPLITUDE_NM (A0)";"END_AMPLITUDE_NM (A1)"; ...
    "AMPLITUDE_EXPONENT (a)";"SWEEP_DURATION_S (T)"; ...
    "KP_PD (Kp) [Nm/deg]";"KD_PD (Kd) [Nm/(rad/s)]"; ...
    "SAMPLING_FREQUENCY_HZ (fs)";"PRE_HOLD_DURATION_S (T_pre)"; ...
    "POST_HOLD_DURATION_S (T_post)";"TORQUE_LIMIT_NM (tau_limit)"]);
% A newly introduced control setting is ambiguous and must not be ignored.
extra=M.Key(startsWith(M.Key,"CONFIG.") & ~ismember(M.Key,[keys;ignored']));
keys=unique([keys;extra]);
reasons=strings(0,1);
for k=keys'
    a=M.Value(M.Key==k); b=R.Value(R.Key==k);
    if numel(a)~=1 || numel(b)~=1
        reasons(end+1)="missing/ambiguous condition: "+k; continue
    end
    x=str2double(a); y=str2double(b);
    if isfinite(x) && isfinite(y)
        same=abs(x-y)<=1e-8+1e-6*max(abs([x y]));
    else
        same=a==b;
    end
    if ~same, reasons(end+1)="condition mismatch: "+k+" ("+a+" vs "+b+")"; end
end
end
