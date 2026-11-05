function r=evaluate(r)
o=r.options; out=r.output; n=numel(r.trials);
metrics=table; frequencyMetrics=table; diagnostics=table;
U=[]; Y=[]; modelCoefficients=cell(1,3); H=[]; predictions=cell(n,2,3);
for scope=1:2
    if scope==1, p=r.pooled; D=r.delay; label="all-fit";
    else, p=r.training; D=r.trainingDelay; label="training-fixed"; end
    for i=1:n
        tr=r.trials{i}; d=tr.d;
        td=[0 D.frf_s D.time_s]; names=["A","B_FRF","B_time"];
        subset="training"; if r.split.validation(i), subset="validation"; end
        if scope==1, subset="descriptive-all-fit"; end
        for m=1:3
            y=qm.predict(p(1),p(2),td(m),tr); predictions{i,scope,m}=y;
            F=qm.regression(tr,[d.u d.x y(:,1)],o);
            E=F.C(:,2)./F.C(:,1); P=F.C(:,3)./F.C(:,1);
            gain=20*log10(abs(P./E)); phase=angle(P./E)*180/pi;
            err=[d.x d.v]-y;
            row=table(i,label,subset,names(m), ...
                100*(1-norm(err(:,1))/norm(d.x-mean(d.x))), ...
                sqrt(mean(err(:,1).^2)),sqrt(mean(err(:,2).^2)), ...
                sqrt(mean(gain.^2,'omitnan')),sqrt(mean(phase.^2,'omitnan')), ...
                'VariableNames',{'trial','scope','subset','model','angleFit_percent', ...
                'angleRMSE_rad','velocityRMSE_rad_s','gainRMSE_dB','phaseRMSE_deg'});
            metrics=[metrics;row]; %#ok<AGROW>
            bands=[1 10;10 30;30 60;60 100];
            for b=1:4
                mask=F.f>=bands(b,1)&F.f<=bands(b,2);
                fr=table(i,label,subset,names(m),bands(b,1),bands(b,2), ...
                    sqrt(mean(gain(mask).^2,'omitnan')),sqrt(mean(phase(mask).^2,'omitnan')), ...
                    'VariableNames',{'trial','scope','subset','model','low_Hz','high_Hz','gainRMSE_dB','phaseRMSE_deg'});
                frequencyMetrics=[frequencyMetrics;fr]; %#ok<AGROW>
            end
            if scope==1
                modelCoefficients{m}(:,i)=F.C(:,3);
                if m==1
                    U(:,i)=F.C(:,1); Y(:,i)=F.C(:,2); H(:,i)=E; %#ok<AGROW>
                    diag=table(repmat(i,numel(F.f),1),F.f,F.status,F.diagnostics(:,1), ...
                        F.diagnostics(:,2),F.diagnostics(:,3),F.diagnostics(:,4),F.R2(:,2), ...
                        real(F.C(:,1)),imag(F.C(:,1)),real(F.C(:,2)),imag(F.C(:,2)), ...
                        real(E),imag(E), ...
                        'VariableNames',{'trial','frequency_Hz','status','samples','rcond','cycles', ...
                        'relativeBandwidth','R2','U_real','U_imag','Y_real','Y_imag','H_real','H_imag'});
                    diagnostics=[diagnostics;diag]; %#ok<AGROW>
                end
            end
        end
        P=predictions(i,scope,:);
        ts=table(d.t,d.u,d.x,d.v,P{1}(:,1),P{1}(:,2),P{2}(:,1),P{2}(:,2), ...
            P{3}(:,1),P{3}(:,2),r.residuals{i}(:,1),r.residuals{i}(:,2), ...
            'VariableNames',{'time_s','chirp_Nm','x_rad','v_rad_s','A_x','A_v','B_FRF_x', ...
            'B_FRF_v','B_time_x','B_time_v','individual_x_residual','individual_v_residual'});
        writetable(ts,fullfile(out,sprintf('response_%02d_%s.csv',i,label)));
    end
end
s=qm.spectra(U,Y); f=F.f; models=zeros(numel(f),3);
for m=1:3
    z=qm.spectra(U,modelCoefficients{m}); models(:,m)=z.H1;
end
frf=table(f,real(s.Hmean),imag(s.Hmean),real(s.H1),imag(s.H1),s.coherence,s.count, ...
    real(models(:,1)),imag(models(:,1)),real(models(:,2)),imag(models(:,2)), ...
    real(models(:,3)),imag(models(:,3)), ...
    'VariableNames',{'frequency_Hz','mean_real','mean_imag','H1_real','H1_imag', ...
    'coherence','validTrials','A_local_real','A_local_imag','B_FRF_local_real', ...
    'B_FRF_local_imag','B_time_local_real','B_time_local_imag'});
writetable(frf,fullfile(out,'frf_ensemble.csv'));
writetable(diagnostics,fullfile(out,'frf_trials_diagnostics.csv'));
writetable(metrics,fullfile(out,'model_metrics.csv'));
writetable(frequencyMetrics,fullfile(out,'model_metrics_by_band.csv'));
low=s.coherence<.9; edges=diff([false;low;false]);
lo=f(find(edges==1)); hi=f(find(edges==-1)-1);
lowBands=table(lo,hi,'VariableNames',{'first_grid_Hz','last_grid_Hz'});
writetable(lowBands,fullfile(out,'low_coherence_bands.csv'));
r.metrics=metrics; r.frequencyMetrics=frequencyMetrics; r.diagnostics=diagnostics;
r.frf=frf; r.H=H; r.modelLocal=models; r.predictions=predictions; r.lowCoherenceBands=lowBands;
% Descriptive Pearson associations, not causal evidence. No p-value fishing.
dp=r.delay.phaseByTrial;
td=dp.delay_s(dp.low_Hz==30 & dp.high_Hz==100);
values=[r.individual.J r.individual.B td];
labels=["J";"B";"Td_30_100"];
correlations=nan(3,1);
for k=1:3
    if n>=3 && std(values(:,k))>0 && std(r.individual.q0_rad)>0
        z=corrcoef(r.individual.q0_rad,values(:,k)); correlations(k)=z(1,2);
    end
end
r.positionAssociation=table(labels,correlations,'VariableNames',{'parameter','Pearson_r_with_q0'});
writetable(r.positionAssociation,fullfile(out,'initial_position_associations.csv'));
freqCorr=nan(numel(f),2);
for k=1:numel(f)
    vals=[20*log10(abs(H(k,:)))' unwrap(angle(H(k,:)))'];
    for j=1:2
        ok=isfinite(vals(:,j));
        if sum(ok)>=3 && std(vals(ok,j))>0 && std(r.individual.q0_rad(ok))>0
            z=corrcoef(r.individual.q0_rad(ok),vals(ok,j)); freqCorr(k,j)=z(1,2);
        end
    end
end
writetable(table(f,freqCorr(:,1),freqCorr(:,2),'VariableNames', ...
    {'frequency_Hz','q0_gain_correlation','q0_phase_correlation'}),fullfile(out,'initial_position_frf.csv'));
end
