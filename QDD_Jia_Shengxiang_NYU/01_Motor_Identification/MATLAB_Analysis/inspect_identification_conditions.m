function conditions = inspect_identification_conditions()
% Read-only diagnosis of sampling and logged holding-PD consistency.
here=fileparts(mfilename('fullpath'));
names={'motor_data_20260703_172605.csv','motor_data_20260703_170331.csv'};
conditions=cell(1,2);
for k=1:2
    d=readtable(fullfile(fileparts(here),'Data','AK80_Motor1',names{k}));
    t=(d.Time_ms-d.Time_ms(1))/1000; n=height(d);
    active=find(abs(d.Chirp_Torque_Nm)>1e-4);
    use=t>t(active(1))+0.1 & t<t(active(end))-0.1;
    s.File=string(names{k}); s.EstimatedFs=(n-1)/t(end);
    s.MaxTimestampGap_ms=max(diff(t))*1000;
    s.Duration_s=t(end); s.NonzeroErrors=nnz(d.Error_Code~=0);
    s.NonmonotonicTimes=nnz(diff(t)<0);
    s.ChirpAmplitude_Nm=max(abs(d.Chirp_Torque_Nm));
    % Count records in complete 1-second bins: independent of ms duplicates.
    counts=histcounts(t,0:floor(t(end)));
    s.OneSecondCountMin=min(counts(2:end-1));
    s.OneSecondCountMedian=median(counts(2:end-1));
    s.OneSecondCountMax=max(counts(2:end-1));
    logs=cell(1,7);
    for lag=0:6
        rows=find(use); rows=rows(rows>lag);
        A=[ones(numel(rows),1),d.Position_deg(rows-lag),d.Speed_rad_s(rows-lag)];
        beta=A\d.PD_Torque_Nm(rows);
        v.LagSamples=lag; v.Kp_Nm_per_deg=-beta(2); v.Kd=-beta(3);
        v.Target_deg=-beta(1)/beta(2);
        v.RMSE_Nm=rms(A*beta-d.PD_Torque_Nm(rows));
        % Also test nominal gains without changing their values.
        inferredTarget=d.Position_deg(rows-lag)+(d.PD_Torque_Nm(rows)+0.2*d.Speed_rad_s(rows-lag))/0.04;
        v.NominalTargetMedian_deg=median(inferredTarget);
        v.NominalRMSE_Nm=rms(0.04*(median(inferredTarget)-d.Position_deg(rows-lag))-0.2*d.Speed_rad_s(rows-lag)-d.PD_Torque_Nm(rows));
        logs{lag+1}=v;
    end
    log=struct2table([logs{:}]); [~,best]=min(log.RMSE_Nm);
    s.BestLagSamples=log.LagSamples(best);
    s.InferredKp=log.Kp_Nm_per_deg(best); s.InferredKd=log.Kd(best);
    s.PDRMSE_Nm=log.RMSE_Nm(best); s.NominalPDRMSE_Nm=log.NominalRMSE_Nm(best);
    s.InferredTarget_deg=log.Target_deg(best);
    conditions{k}=struct('summary',s,'lagScan',log);
    disp(struct2table(s)); disp(log);
end
end
