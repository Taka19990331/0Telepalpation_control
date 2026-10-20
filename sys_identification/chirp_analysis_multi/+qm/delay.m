function D=delay(trials,p,o)
n=numel(trials); U=[]; Y=[]; A=[]; per=[];
bands=[1 10;10 30;30 60;60 100;30 100;1 100];
for k=1:n
    tr=trials{k}; pred=qm.predict(p(1),p(2),0,tr);
    F=qm.regression(tr,[tr.d.u tr.d.x pred(:,1)],o);
    U(:,k)=F.C(:,1); Y(:,k)=F.C(:,2); A(:,k)=F.C(:,3); %#ok<AGROW>
    for b=1:size(bands,1)
        per=[per; phasefit(F.f,F.C(:,2)./F.C(:,3),bands(b,:),k)]; %#ok<AGROW>
    end
end
s=qm.spectra(U,Y); a=qm.spectra(U,A); f=F.f;
pooled=[];
for b=1:size(bands,1), pooled=[pooled;phasefit(f,s.H1./a.H1,bands(b,:),0)]; end %#ok<AGROW>
% Full-band complex relative FRF error, every estimable frequency equal weight.
fun=@(td)frfcost(td,trials,p,o,s.H1);
[tdF,scanF]=qm.bounded(fun,o);
[tdT,scanT]=qm.bounded(@(td)qm.cost(p,td,trials),o);
D=struct('phaseByTrial',per,'phasePooled',pooled,'frf_s',tdF,'time_s',tdT, ...
    'frfScan',scanF,'timeScan',scanT,'f',f,'spectra',s,'modelA',a);
end
function v=frfcost(td,trials,p,o,E)
U=[]; Y=[];
for k=1:numel(trials)
    tr=trials{k}; y=qm.predict(p(1),p(2),td,tr);
    F=qm.regression(tr,[tr.d.u y(:,1)],o);
    U(:,k)=F.C(:,1); Y(:,k)=F.C(:,2); %#ok<AGROW>
end
s=qm.spectra(U,Y); z=abs((s.H1-E)./max(abs(E),1e-12)).^2;
v=mean(z,'omitnan');
end
function row=phasefit(f,ratio,band,trial)
ok=f>=band(1)&f<=band(2)&isfinite(ratio)&abs(ratio)>0;
ff=f(ok); phase=unwrap(angle(ratio(ok))); w=2*pi*ff;
td=NaN; r2=NaN; rmse=NaN; gain=NaN; intercept=NaN; freeTd=NaN;
if numel(ff)>=3
    td=-(w'*phase)/(w'*w); res=phase+w*td;
    r2=1-sum(res.^2)/max(sum((phase-mean(phase)).^2),eps);
    rmse=sqrt(mean(res.^2))*180/pi;
    gain=sqrt(mean((20*log10(abs(ratio(ok)))).^2));
    z=[ones(size(w)) -w]\phase; intercept=z(1)*180/pi; freeTd=z(2);
end
row=table(trial,band(1),band(2),td,r2,rmse,gain,intercept,freeTd,numel(ff), ...
    'VariableNames',{'trial','low_Hz','high_Hz','delay_s','R2','phaseResidual_deg', ...
    'gainRMSE_dB','freeIntercept_deg','freeSlopeDelay_s','points'});
% Signed phase slope is a diagnostic; Model B optimization is bounded >=0.
end
