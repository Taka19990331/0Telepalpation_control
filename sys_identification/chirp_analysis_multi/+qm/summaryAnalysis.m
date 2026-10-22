function s=summaryAnalysis(trials,o,out)
% Minimal analysis path for the two summary figures.
n=numel(trials);
[p,~]=qm.fit(trials,o);
D=qm.delay(trials,p,o,true);
f=D.f; U=[]; Y=[]; A=[]; B=[]; names=strings(n,1);
for k=1:n
    tr=trials{k}; names(k)=string(tr.file);
    yA=qm.predict(p(1),p(2),0,tr);
    yB=qm.predict(p(1),p(2),D.frf_s,tr);
    F=qm.regression(tr,[tr.d.u tr.d.x yA(:,1) yB(:,1)],o);
    U(:,k)=F.C(:,1); Y(:,k)=F.C(:,2); A(:,k)=F.C(:,3); B(:,k)=F.C(:,4); %#ok<AGROW>
end
e=qm.spectra(U,Y); a=qm.spectra(U,A); b=qm.spectra(U,B);
s=struct('output',out,'J',p(1),'B',p(2),'Td_FRF_s',D.frf_s, ...
    'frequency_Hz',f,'experimentalH1',e.H1,'modelA',a.H1,'modelB_FRF',b.H1, ...
    'coherence',e.coherence,'validTrials',e.count, ...
    'phaseDelayByTrial_s',D.phaseDelayByTrial_s,'trialNames',names);

frf=table(f,real(e.H1),imag(e.H1),e.coherence,e.count, ...
    real(a.H1),imag(a.H1),real(b.H1),imag(b.H1), ...
    'VariableNames',{'frequency_Hz','H1_real','H1_imag','coherence','validTrials', ...
    'A_real','A_imag','B_FRF_real','B_FRF_imag'});
writetable(frf,fullfile(out,'frf_ensemble_summary.csv'));
writetable(table(p(1),p(2),D.frf_s, ...
    'VariableNames',{'J','B','Td_FRF_s'}),fullfile(out,'summary_parameters.csv'));

[freqGrid,trialGrid]=ndgrid(f,(1:n)');
phaseDelayMs=D.phaseDelayByTrial_s*1000;
writetable(table(trialGrid(:),freqGrid(:),phaseDelayMs(:), ...
    'VariableNames',{'trial','frequency_Hz','phaseDelay_ms'}), ...
    fullfile(out,'phase_delay_by_trial_frequency.csv'));
inBand=f>=30&f<=100;
mu=nan(n,1); sigma=nan(n,1); nPoints=zeros(n,1);
for k=1:n
    vals=phaseDelayMs(inBand,k); vals=vals(isfinite(vals)); nPoints(k)=numel(vals);
    if ~isempty(vals), mu(k)=mean(vals); end
    if numel(vals)>=2, sigma(k)=std(vals,0); end
end
bandStats=table((1:n)',names,mu,sigma,nPoints, ...
    'VariableNames',{'trial','file','meanPhaseDelay_ms','stdPhaseDelay_ms','frequencyPoints'});
s.bandStats=bandStats;
writetable(bandStats,fullfile(out,'trial_phase_delay_30_100Hz.csv'));
save(fullfile(out,'analysis_summary.mat'),'s','-v7.3');
end
