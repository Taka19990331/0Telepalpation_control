function result = merge_pd_chirp_frf()
% PD+chirp only. Cosine crossfade of complex FRFs on a log-frequency grid.
SHOW_WRAPPED_PHASE = false;
here=fileparts(mfilename('fullpath'));
src=fullfile(here,'individual_identification_results','individual_analysis.mat');
assert(isfile(src),'Run identify_each_experiment first.');
loaded=load(src,'analysis'); records=loaded.analysis.records;
out=fullfile(here,'merged_pd_chirp_results');
if ~exist(out,'dir'), mkdir(out); end
f=unique(sort([logspace(log10(1.2),log10(110),500)';12;19]));
h=complex(nan(numel(f),2)); c=nan(numel(f),2);
for k=1:2
    h(:,k)=interp1(records{k}.frequency,records{k}.H(:,2),f);
    c(:,k)=interp1(records{k}.frequency,records{k}.coherence(:,2),f);
end
blend=f>=12 & f<=19;
w=ones(size(f)); w(f>=19)=0;
x=(log(f(blend))-log(12))/(log(19)-log(12));
w(blend)=0.5*(1+cos(pi*x));
merged=w.*h(:,1)+(1-w).*h(:,2);
half=w; half(blend)=0.5;
average=half.*h(:,1)+(1-half).*h(:,2);
quality=c(:,1); quality(f>19)=c(f>19,2);
quality(blend)=min(c(blend,:),[],2);
% Quality is a source-coherence screen, NOT coherence of the blended FRF.
used=quality>=0.9 & isfinite(merged) & isfinite(average);
assert(all(isfinite(merged)) && all(abs(merged)>0));
assert(abs(w(f==12)-1)<1e-12 && abs(w(f==19))<1e-12);
assert(max(abs(merged(f<12)-h(f<12,1)))<1e-12);
assert(max(abs(merged(f>19)-h(f>19,2)))<1e-12);
fitSmooth=fitModel(f(used),merged(used));
fitAverage=fitModel(f(used),average(used));
model=plant(fitSmooth.x,f);
tableOut=table(f,h(:,1),h(:,2),w,merged,average,c(:,1),c(:,2),used,model, ...
    'VariableNames',{'Frequency_Hz','LowFRF','HighFRF','LowWeight','MergedFRF', ...
    'EqualAverageFRF','LowSourceCoherence','HighSourceCoherence','UsedForFit','ModelFRF'});
writetable(tableOut,fullfile(out,'merged_frf.csv'));
fig=figure('Visible','off','Position',[30 30 1250 1050]); tiledlayout(3+SHOW_WRAPPED_PHASE,1);
lo=f<=19; hi=f>=12;
nexttile; semilogx(f(lo),20*log10(abs(h(lo,1))),'--',f(hi),20*log10(abs(h(hi,2))),'--',f,20*log10(abs(merged)),'k','LineWidth',1.2);
grid on; ylabel('Magnitude [dB rad/Nm]'); legend('2 Nm experiment','5 Nm experiment','Merged PD + chirp input','Location','best');
title('Complex FRF merge | 12-19 Hz cosine crossfade | no gain or phase alignment');
nexttile; semilogx(f(lo),phase(h(lo,1)),'--',f(hi),phase(h(hi,2)),'--',f,phase(merged),'k','LineWidth',1.2);
grid on; ylabel('Phase [deg]');
nexttile; semilogx(f,w,f,1-w,'LineWidth',1.2); grid on; ylim([0 1.05]);
ylabel('Merge weight'); xlabel('Frequency [Hz]'); legend('2 Nm weight','5 Nm weight');
if SHOW_WRAPPED_PHASE, nexttile; semilogx(f,angle(merged)*180/pi); grid on; ylabel('Wrapped phase [deg]'); end
exportgraphics(fig,fullfile(out,'merged_frf.png'),'Resolution',150); close(fig);
fig=figure('Visible','off','Position',[30 30 1300 900]); tiledlayout(2,2);
nexttile; plot(f(blend),20*log10(abs([h(blend,:),merged(blend),average(blend)])),'LineWidth',1.2); grid on;
ylabel('Magnitude [dB]'); title('Overlap detail'); legend('2 Nm','5 Nm','Cosine merge','Equal average');
nexttile; plot(f(blend),20*log10(abs(average(blend)./merged(blend)))); grid on;
ylabel('Equal average / cosine [dB]'); title('Sensitivity to merge rule');
nexttile; plot(f(blend),phase([h(blend,:),merged(blend),average(blend)]),'LineWidth',1.2); grid on;
ylabel('Phase [deg]'); xlabel('Frequency [Hz]');
nexttile; plot(f(blend),angle(average(blend).*conj(merged(blend)))*180/pi); grid on;
ylabel('Equal average - cosine [deg]'); xlabel('Frequency [Hz]');
exportgraphics(fig,fullfile(out,'merge_overlap_detail.png'),'Resolution',150); close(fig);
fig=figure('Visible','off','Position',[30 30 1250 850]); tiledlayout(2,1);
nexttile; semilogx(f,20*log10(abs(merged)),'.',f,20*log10(abs(model)),'LineWidth',1.2); grid on;
ylabel('Magnitude [dB rad/Nm]'); legend('Merged experimental FRF','Fitted model');
title('PD + chirp input | provisional inertia + viscosity + delay model');
nexttile; semilogx(f,phase(merged),'.',f,phase(model),'LineWidth',1.2); grid on;
ylabel('Phase [deg]'); xlabel('Frequency [Hz]');
exportgraphics(fig,fullfile(out,'merged_model_fit.png'),'Resolution',150); close(fig);
stats=table(["Cosine crossfade";"Equal average"], ...
    exp([fitSmooth.x(1);fitAverage.x(1)]),exp([fitSmooth.x(2);fitAverage.x(2)]), ...
    [fitSmooth.x(3);fitAverage.x(3)],[fitSmooth.cost;fitAverage.cost], ...
    [fitSmooth.flag;fitAverage.flag], ...
    'VariableNames',{'Method','J_apparent','D_apparent','Td_ms','Cost','ExitFlag'});
writetable(stats,fullfile(out,'fit_parameters.csv')); disp(stats);
bands=[1.2 12;12 19;19 110]; errors=cell(1,3);
for k=1:3
    sel=f>=bands(k,1)&f<=bands(k,2)&used;
    s.Fmin_Hz=bands(k,1); s.Fmax_Hz=bands(k,2);
    s.MagnitudeRMSE_dB=rms(20*log10(abs(model(sel)./merged(sel))));
    s.PhaseRMSE_deg=rms(angle(model(sel).*conj(merged(sel)))*180/pi);
    errors{k}=s;
end
metrics=struct2table([errors{:}]); writetable(metrics,fullfile(out,'fit_errors.csv')); disp(metrics);
deltaMag=max(abs(20*log10(abs(average(blend)./merged(blend)))));
deltaPhase=max(abs(angle(average(blend).*conj(merged(blend)))*180/pi));
fprintf('Max merge-rule difference: %.6f dB, %.6f deg\n',deltaMag,deltaPhase);
result=struct('frequency',f,'merged',merged,'lowWeight',w,'sources',h,'sourceCoherence',c, ...
    'fit',fitSmooth,'equalAverageFit',fitAverage,'parameters',stats,'errors',metrics, ...
    'maxMergeDifference_dB',deltaMag,'maxMergeDifference_deg',deltaPhase);
save(fullfile(out,'merged_result.mat'),'result');
end
function p=phase(h)
p=unwrap(angle(h),[],1)*180/pi; p=p-360*ceil(p(1,:)/360);
end
function h=plant(x,f)
s=1i*2*pi*f; h=exp(-s*x(3)/1000)./(exp(x(1))*s.^2+exp(x(2))*s);
end
function v=objective(x,f,h)
if any(~isfinite(x)) || any(x(1:2)<-18) || any(x(1:2)>8) || x(3)<0 || x(3)>20
    v=1e10+sum(x.^2); return;
end
m=plant(x,f); v=mean((20*log10(abs(m./h))).^2+0.05*(angle(m.*conj(h))*180/pi).^2);
end
function fit=fitModel(f,h)
opts=optimset('Display','off','MaxIter',2500,'MaxFunEvals',7500,'TolX',1e-9,'TolFun',1e-9);
best=Inf;
for d=[0.003 0.03 0.3]
    for td=[0.05 1.5 5]
        [x,v,flag]=fminsearch(@(xx) objective(xx,f,h),[log(0.012),log(d),td],opts);
        if v<best, best=v; bx=x; bf=flag; end
    end
end
assert(isfinite(best)&&bf>0,'Model fit failed to converge');
fit=struct('x',bx,'cost',best,'flag',bf);
end
