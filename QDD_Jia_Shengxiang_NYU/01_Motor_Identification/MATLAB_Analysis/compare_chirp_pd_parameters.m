function result = compare_chirp_pd_parameters()
% Same bare-plant model for all six conditions: input-definition audit.
SHOW_WRAPPED_PHASE = false;
here=fileparts(mfilename('fullpath'));
out=fullfile(here,'chirp_pd_parameter_results');
if ~exist(out,'dir'), mkdir(out); end
diary(fullfile(out,'execution_log.txt')); cleanup=onCleanup(@() diary('off'));
a=load(fullfile(here,'individual_identification_results','individual_analysis.mat'),'analysis');
records=a.analysis.records;
labels={'Chirp cmd only input','PD + chirp cmd input'};
stages={'Low band (1.2-19 Hz)','High band (12-110 Hz)','Merged (1.2-110 Hz)'};
f=unique(sort([logspace(log10(1.2),log10(110),500)';12;19]));
blend=f>=12 & f<=19; w=ones(size(f)); w(f>=19)=0;
w(blend)=(1+cos(pi*log(f(blend)/12)/log(19/12)))/2;
H=complex(zeros(numel(f),2,2)); C=zeros(size(H));
for j=1:2
    for k=1:2
        H(:,k,j)=interp1(records{k}.frequency,records{k}.H(:,j),f);
        C(:,k,j)=interp1(records{k}.frequency,records{k}.coherence(:,j),f);
    end
end
merged=squeeze(w.*H(:,1,:)+(1-w).*H(:,2,:));
quality=squeeze(C(:,1,:)); quality(f>19,:)=squeeze(C(f>19,2,:));
quality(blend,:)=squeeze(min(C(blend,:,:),[],2));
rows={}; fits=cell(3,2); J=zeros(3,2); D=J;
fig=figure('Visible','off','Position',[20 20 1600 950]); tiledlayout(2+SHOW_WRAPPED_PHASE,3);
for stage=1:3
    if stage==1
        band=f<=19; hs=squeeze(H(:,1,:)); cs=squeeze(C(:,1,:));
    elseif stage==2
        band=f>=12; hs=squeeze(H(:,2,:)); cs=squeeze(C(:,2,:));
    else
        band=true(size(f)); hs=merged; cs=quality;
    end
    % Identical frequency points for both input definitions in each stage.
    used=band & all(cs>=0.9,2) & all(isfinite(hs),2);
    assert(nnz(used)>10);
    models=complex(zeros(numel(f),2));
    for j=1:2
        fit=fitModel(f(used),hs(used,j)); fits{stage,j}=fit;
        models(:,j)=plant(fit.x,f);
        J(stage,j)=exp(fit.x(1)); D(stage,j)=exp(fit.x(2));
        s.Stage=string(stages{stage}); s.Input=string(labels{j});
        s.J_apparent=J(stage,j); s.D_apparent=D(stage,j); s.Td_ms=fit.x(3);
        s.MagnitudeRMSE_dB=rms(20*log10(abs(models(used,j)./hs(used,j))));
        s.PhaseRMSE_deg=rms(angle(models(used,j).*conj(hs(used,j)))*180/pi);
        s.Cost=fit.cost; s.Points=nnz(used); s.ExitFlag=fit.flag;
        s.DelayAtBoundary=fit.x(3)<1e-4 || fit.x(3)>19.9999;
        rows{end+1}=s;
    end
    tab=table(f,band,used,hs(:,1),hs(:,2),models(:,1),models(:,2),cs(:,1),cs(:,2), ...
        'VariableNames',{'Frequency_Hz','InBand','UsedForFit','ChirpFRF','PDChirpFRF','ChirpModel','PDChirpModel','ChirpSourceQuality','PDChirpSourceQuality'});
    writetable(tab,fullfile(out,sprintf('stage%d_frf.csv',stage)));
    ff=f(band); hh=hs(band,:); mm=models(band,:);
    nexttile(stage); semilogx(ff,20*log10(abs(hh(:,1))),'b.',ff,20*log10(abs(mm(:,1))),'b-',ff,20*log10(abs(hh(:,2))),'r.',ff,20*log10(abs(mm(:,2))),'r-');
    grid on; title(stages{stage}); ylabel('Magnitude [dB rad/Nm]');
    if stage==1, legend('Chirp only: FRF','Chirp only: model','PD + chirp: FRF','PD + chirp: model','Location','southwest'); end
    nexttile(stage+3); semilogx(ff,phase(hh(:,1)),'b.',ff,phase(mm(:,1)),'b-',ff,phase(hh(:,2)),'r.',ff,phase(mm(:,2)),'r-');
    grid on; ylabel('Phase [deg]'); xlabel('Frequency [Hz]');
    if SHOW_WRAPPED_PHASE
        nexttile(stage+6); semilogx(ff,angle([hh mm])*180/pi); grid on; ylabel('Wrapped phase [deg]');
    end
end
sgtitle({'Same fitted model: exp(-Td s) / (J s^2 + D s)','Chirp-only parameters are apparent values: holding PD is omitted from this model'});
exportgraphics(fig,fullfile(out,'all_frf_model_fits.png'),'Resolution',150); close(fig);
parameters=struct2table([rows{:}]); disp(parameters); writetable(parameters,fullfile(out,'parameters.csv'));
fig=figure('Visible','off','Position',[20 20 1350 700]); tiledlayout(1,2);
for p=1:2
    nexttile; if p==1, values=J; unit='J [kg m^2]'; else, values=D; unit='D [Nm s/rad]'; end
    b=bar(values,'grouped'); b(1).FaceColor=[0 0.35 0.85]; b(2).FaceColor=[0.85 0.15 0.1];
    xticks(1:3); xticklabels({'Low band','High band','Merged'}); ylabel(unit); grid on;
    ylim([0 max(values(:))*1.25]);
    for j=1:2
        text(b(j).XEndPoints,b(j).YEndPoints,compose('%.5g',values(:,j)),'HorizontalAlignment','center','VerticalAlignment','bottom','FontSize',11);
    end
    legend(labels,'Location','northwest');
end
sgtitle({'J and D estimates | identical model and fitting rule','Chirp-only values include bias from omitted holding PD; no confidence intervals estimated'});
exportgraphics(fig,fullfile(out,'jd_bar_comparison.png'),'Resolution',150); close(fig);
fig=figure('Visible','off','Position',[20 20 1350 950]); tiledlayout(3,2);
for j=1:2
    nexttile(j); semilogx(f(f<=19),20*log10(abs(H(f<=19,1,j))),'--',f(f>=12),20*log10(abs(H(f>=12,2,j))),'--',f,20*log10(abs(merged(:,j))),'k-'); grid on; title(labels{j}); ylabel('Magnitude [dB rad/Nm]'); legend('Low experiment (2 Nm)','High experiment (5 Nm)','Cosine merge');
    nexttile(j+2); semilogx(f(f<=19),phase(H(f<=19,1,j)),'--',f(f>=12),phase(H(f>=12,2,j)),'--',f,phase(merged(:,j)),'k-'); grid on; ylabel('Phase [deg]');
    nexttile(j+4); semilogx(f,w,f,1-w); grid on; ylabel('Merge weight'); xlabel('Frequency [Hz]'); legend('Low experiment','High experiment');
end
exportgraphics(fig,fullfile(out,'both_merged_frf.png'),'Resolution',150); close(fig);
change=table(string(stages'),100*(J(:,1)./J(:,2)-1),100*(D(:,1)./D(:,2)-1), ...
    'VariableNames',{'Stage','ChirpVsPD_J_percent','ChirpVsPD_D_percent'});
writetable(change,fullfile(out,'input_definition_difference.csv')); disp(change);
result=struct('parameters',parameters,'change',change,'frequency',f,'merged',merged,'lowWeight',w,'fits',{fits});
save(fullfile(out,'comparison.mat'),'result');
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

