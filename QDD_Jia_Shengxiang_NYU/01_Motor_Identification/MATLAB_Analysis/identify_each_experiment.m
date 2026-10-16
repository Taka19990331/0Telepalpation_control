function analysis = identify_each_experiment()
% Four stages: independent FRFs, overlap, individual fits, conditional merge.
SHOW_WRAPPED_PHASE = false;
% Explicit engineering screening limits; not statistical confidence limits.
MAG_RMS_LIMIT_DB = 0.5; PHASE_RMS_LIMIT_DEG = 2;
J_REL_LIMIT = 0.20; D_REL_LIMIT = 0.30;
FIT_MAG_RMS_LIMIT_DB = 1; FIT_PHASE_RMS_LIMIT_DEG = 3;
here=fileparts(mfilename('fullpath')); out=fullfile(here,'individual_identification_results');
if ~exist(out,'dir'), mkdir(out); end
fid=fopen(fullfile(out,'execution_log.txt'),'w'); fclose(fid);
diary(fullfile(out,'execution_log.txt')); cleaner=onCleanup(@() diary('off'));
conditions=inspect_identification_conditions();
labels={'1: Chirp cmd only input','2: PD + chirp cmd input','3: Measured torque input'};
columns={'Chirp_Torque_Nm','Command_Torque_Nm','Measured_Torque_Nm'};
plotBands=[1 20;10 120]; fitBands=[1.2 19;12 110]; commonBand=[12 19];
records=cell(1,2); summaries={}; fits=cell(2,3,2);
for k=1:2
    cs=conditions{k}.summary;
    writetable(conditions{k}.lagScan,fullfile(out,sprintf('file%d_pd_lag_scan.csv',k)));
    d=readtable(fullfile(fileparts(here),'Data','AK80_Motor1',cs.File));
    t=(d.Time_ms-d.Time_ms(1))/1000; Fs=cs.EstimatedFs;
    active=find(abs(d.Chirp_Torque_Nm)>1e-4);
    % Recorded time, not fixed row index. Trim stop transition and startup.
    use=t>=max(5,t(active(1))+0.5) & t<=t(active(end))-0.5;
    u=detrend(d{use,columns}); y=detrend(unwrap(deg2rad(d.Position_deg(use))));
    assert(all(isfinite(u(:))) && all(isfinite(y)));
    for j=1:3
        [h,f]=tfestimate(u(:,j),y,hann(4096),2048,32768,Fs);
        c=mscohere(u(:,j),y,hann(4096),2048,32768,Fs);
        if j==1, H=complex(zeros(numel(f),3)); C=zeros(numel(f),3); end
        H(:,j)=h; C(:,j)=c;
    end
    % In the logged coordinates, holding PD acts on the preceding row.
    % Input 1 uses an approximate continuous PD with one-row feedback delay.
    % This assumes logged speed approximates angular derivative.
    pd=[cs.InferredKp*180/pi,cs.InferredKd,cs.BestLagSamples/Fs];
    records{k}=struct('file',cs.File,'Fs',Fs,'frequency',f,'H',H,'coherence',C,'pd',pd);
    pb=f>=plotBands(k,1) & f<=plotBands(k,2);
    tab=table(f(pb),'VariableNames',{'Frequency_Hz'});
    fig=figure('Visible','off','Position',[20 20 1400 900]); tiledlayout(3+SHOW_WRAPPED_PHASE,1);
    nexttile; semilogx(f(pb),20*log10(abs(H(pb,:)))); grid on; ylabel('Magnitude [dB rad/Nm]');
    title({char(cs.File),sprintf('Estimated Fs=%.3f Hz | raw logged channel alignment retained',Fs)},'Interpreter','none');
    legend(labels,'Location','best');
    nexttile; semilogx(f(pb),phasePlot(H(pb,:))); grid on; ylabel('Phase [deg]');
    nexttile; semilogx(f(pb),C(pb,:)); grid on; yline(0.9,'--'); ylim([0 1.02]); ylabel('Coherence'); xlabel('Frequency [Hz]');
    if SHOW_WRAPPED_PHASE, nexttile; semilogx(f(pb),angle(H(pb,:))*180/pi); grid on; ylabel('Wrapped phase [deg]'); end
    exportgraphics(fig,fullfile(out,sprintf('file%d_independent_frf.png',k)),'Resolution',150); close(fig);
    for j=1:3
        tab.(sprintf('Input%d_Magnitude_dB',j))=20*log10(abs(H(pb,j)));
        tab.(sprintf('Input%d_Phase_deg',j))=phasePlot(H(pb,j));
        tab.(sprintf('Input%d_Coherence',j))=C(pb,j);
        for bandType=1:2
            if bandType==1, band=fitBands(k,:); else, band=commonBand; end
            sel=f>=band(1) & f<=band(2) & C(:,j)>=0.9;
            assert(nnz(sel)>10);
            ff=f(sel); hh=H(sel,j);
            fit=fitOne(ff,hh,j,pd);
            fit.fileIndex=k; fit.inputIndex=j; fit.bandType=bandType;
            fit.frequency=ff; fit.measured=hh; fits{k,j,bandType}=fit;
            ss.FileIndex=k; ss.Input=string(labels{j}); ss.BandType=bandType;
            ss.Fmin_Hz=band(1); ss.Fmax_Hz=band(2); ss.Points=nnz(sel);
            ss.J_apparent=exp(fit.x(1)); ss.D_apparent=exp(fit.x(2)); ss.Td_ms=fit.x(3);
            ss.MagnitudeRMSE_dB=rms(20*log10(abs(fit.model./hh)));
            ss.PhaseRMSE_deg=rms(angle(fit.model.*conj(hh))*180/pi);
            ss.Cost=fit.cost; ss.ExitFlag=fit.flag; ss.NearBoundary=fit.boundary;
            ss.DhalfCost=fit.DhalfCost; ss.DdoubleCost=fit.DdoubleCost;
            summaries{end+1}=ss;
        end
    end
    writetable(tab,fullfile(out,sprintf('file%d_independent_frf.csv',k)));
end
parameters=struct2table([summaries{:}]); disp(parameters);
writetable(parameters,fullfile(out,'individual_parameters.csv'));
writetable(struct2table([conditions{1}.summary,conditions{2}.summary]),fullfile(out,'conditions.csv'));

% Common physical frequency grid after per-file Fs estimation.
fg=(10:0.05:20)'; overlap=cell(1,3); gates={}; blended=cell(1,3);
fig=figure('Visible','off','Position',[20 20 1500 1000]); tiledlayout(3,3);
for j=1:3
    ho=complex(zeros(numel(fg),2)); co=zeros(numel(fg),2);
    for k=1:2
        rr=records{k}; ho(:,k)=interp1(rr.frequency,rr.H(:,j),fg);
        co(:,k)=interp1(rr.frequency,rr.coherence(:,j),fg);
    end
    dm=20*log10(abs(ho(:,2)./ho(:,1))); dp=angle(ho(:,2).*conj(ho(:,1)))*180/pi;
    % Gate in the interior overlap, avoiding excitation end points.
    good=fg>=12 & fg<=19 & all(co>=0.9,2);
    ov.Input=string(labels{j}); ov.Points=nnz(good);
    ov.MagnitudeRMS_dB=rms(dm(good)); ov.PhaseRMS_deg=rms(dp(good));
    ov.MagnitudeMedian_dB=median(dm(good)); ov.PhaseMedian_deg=median(dp(good));
    overlap{j}=ov;
    nexttile(j); plot(fg,20*log10(abs(ho))); grid on; title(labels{j}); ylabel('Magnitude [dB]');
    xline(12,':'); xline(19,':'); legend('2 Nm experiment','5 Nm experiment');
    nexttile(3+j); plot(fg,phasePlot(ho)); grid on; ylabel('Phase [deg]');
    nexttile(6+j); plot(fg,co); grid on; yline(0.9,'--'); ylim([0.8 1.01]); ylabel('Coherence'); xlabel('Frequency [Hz]');
    writetable(table(fg,ho(:,1),ho(:,2),dm,dp,co(:,1),co(:,2),good, ...
        'VariableNames',{'Frequency_Hz','LowFRF','HighFRF','DeltaMagnitude_dB','DeltaPhase_deg','LowCoherence','HighCoherence','GatePoint'}), ...
        fullfile(out,sprintf('input%d_overlap.csv',j)));
    a=fits{1,j,1}; b=fits{2,j,1};
    J=exp([a.x(1),b.x(1)]); D=exp([a.x(2),b.x(2)]);
    jr=abs(diff(J))/mean(J); dr=abs(diff(D))/mean(D);
    fitGood=true;
    for k=1:2
        fl=fits{k,j,1};
        fitGood=fitGood && rms(20*log10(abs(fl.model./fl.measured)))<=FIT_MAG_RMS_LIMIT_DB ...
            && rms(angle(fl.model.*conj(fl.measured))*180/pi)<=FIT_PHASE_RMS_LIMIT_DEG ...
            && fl.flag>0 && ~fl.boundary;
    end
    % A flat viscosity profile does not establish parameter agreement.
    viscosityResolved=true;
    for k=1:2
        fl=fits{k,j,1};
        viscosityResolved=viscosityResolved && min(fl.DhalfCost,fl.DdoubleCost)>fl.cost+0.05;
    end
    pass=ov.MagnitudeRMS_dB<=MAG_RMS_LIMIT_DB && ov.PhaseRMS_deg<=PHASE_RMS_LIMIT_DEG ...
        && jr<=J_REL_LIMIT && dr<=D_REL_LIMIT && fitGood && viscosityResolved;
    gt.Input=string(labels{j}); gt.OverlapPass=ov.MagnitudeRMS_dB<=MAG_RMS_LIMIT_DB && ov.PhaseRMS_deg<=PHASE_RMS_LIMIT_DEG;
    gt.JRelativeDifference=jr; gt.DRelativeDifference=dr; gt.FitQualityPass=fitGood;
    gt.ViscosityResolved=viscosityResolved; gt.MergeAllowed=pass; gates{j}=gt;
    if pass
        % Equal complex averaging in overlap, never concatenating duplicates.
        fJoint=logspace(log10(fitBands(1,1)),log10(fitBands(2,2)),400)';
        joint=complex(nan(size(fJoint))); counts=zeros(size(fJoint));
        for k=1:2
            rr=records{k}; allowed=fJoint>=fitBands(k,1) & fJoint<=fitBands(k,2);
            hc=interp1(rr.frequency,rr.H(:,j),fJoint(allowed));
            cc=interp1(rr.frequency,rr.coherence(:,j),fJoint(allowed));
            inds=find(allowed); inds=inds(cc>=0.9); hc=hc(cc>=0.9);
            empty=counts(inds)==0; joint(inds(empty))=0;
            joint(inds)=joint(inds)+hc; counts(inds)=counts(inds)+1;
        end
        ok=counts>0; joint=joint(ok)./counts(ok); fJoint=fJoint(ok);
        % Already log spaced: equal weights per grid point.
        blended{j}=fitOne(fJoint,joint,j,mean([records{1}.pd;records{2}.pd],1),true);
        writetable(table(fJoint,joint,blended{j}.model,'VariableNames',{'Frequency_Hz','AveragedFRF','Model'}), ...
            fullfile(out,sprintf('input%d_conditional_merged.csv',j)));
    end
end
sgtitle('Per-file timestamp-estimated Fs | overlap check (dotted bounds: decision band 12-19 Hz)');
exportgraphics(fig,fullfile(out,'overlap_corrected_fs.png'),'Resolution',150); close(fig);
overlapSummary=struct2table([overlap{:}]); gateSummary=struct2table([gates{:}]);
disp(overlapSummary); disp(gateSummary);
writetable(overlapSummary,fullfile(out,'overlap_summary.csv')); writetable(gateSummary,fullfile(out,'merge_decision.csv'));

for k=1:2
    fig=figure('Visible','off','Position',[20 20 1550 900]); tiledlayout(2,3);
    for j=1:3
        fl=fits{k,j,1};
        nexttile(j); semilogx(fl.frequency,20*log10(abs(fl.measured)),'.',fl.frequency,20*log10(abs(fl.model)),'-');
        grid on; title(labels{j}); ylabel('Magnitude [dB rad/Nm]'); legend('Experimental FRF','Individual fit');
        nexttile(3+j); semilogx(fl.frequency,phasePlot(fl.measured),'.',fl.frequency,phasePlot(fl.model),'-');
        grid on; ylabel('Phase [deg]'); xlabel('Frequency [Hz]');
        writetable(table(fl.frequency,fl.measured,fl.model,'VariableNames',{'Frequency_Hz','ExperimentalFRF','ModelFRF'}), ...
            fullfile(out,sprintf('file%d_input%d_fit.csv',k,j)));
    end
    sgtitle({sprintf('Independent identification: file %d (%g Nm chirp)',k,conditions{k}.summary.ChirpAmplitude_Nm), ...
        'Input 1 includes approximate holding PD; inputs 2/3 use inertia + viscosity + delay'});
    exportgraphics(fig,fullfile(out,sprintf('file%d_individual_models.png',k)),'Resolution',150); close(fig);
end
fig=figure('Visible','off','Position',[20 20 1400 800]); tiledlayout(2,3);
for j=1:3
    for p=1:2
        values=zeros(2,2);
        for k=1:2
            for bt=1:2, values(k,bt)=exp(fits{k,j,bt}.x(p)); end
        end
        nexttile((p-1)*3+j); bar(values); grid on;
        xticks([1 2]); xticklabels({'2 Nm','5 Nm'});
        if p==1, ylabel('Apparent J'); title(labels{j}); else, ylabel('Apparent D'); end
        legend('Own reliable band','Common 12-19 Hz','Location','best');
    end
end
sgtitle('Parameter dependence on experiment AND fitted band | not proof of amplitude causality');
exportgraphics(fig,fullfile(out,'individual_parameter_comparison.png'),'Resolution',150); close(fig);
analysis=struct('conditions',{conditions},'records',{records},'fits',{fits}, ...
    'parameters',parameters,'overlap',overlapSummary,'gate',gateSummary,'merged',{blended});
save(fullfile(out,'individual_analysis.mat'),'analysis');
fprintf('Complete. Files were merged ONLY for inputs with MergeAllowed=true.\n');
end

function p=phasePlot(h)
p=unwrap(angle(h),[],1)*180/pi; p=p-360*ceil(p(1,:)/360);
end
function h=response(x,f,j,pd)
s=1i*2*pi*f; G=exp(-s*x(3)/1000)./(exp(x(1))*s.^2+exp(x(2))*s);
if j==1
    C=(pd(1)+pd(2)*s).*exp(-s*pd(3)); h=G./(1+G.*C);
else, h=G; end
end
function v=cost(x,f,h,j,pd,w)
if any(~isfinite(x)) || any(x(1:2)<-18) || any(x(1:2)>8) || x(3)<0 || x(3)>20
    v=1e10+sum(x.^2); return;
end
m=response(x,f,j,pd);
v=sum(w.*((20*log10(abs(m./h))).^2+0.05*(angle(m.*conj(h))*180/pi).^2))/sum(w);
end
function fit=fitOne(f,h,j,pd,equalWeight)
if nargin<5, equalWeight=false; end
w=1./f; if equalWeight, w=ones(size(f)); end
opts=optimset('Display','off','MaxIter',2200,'MaxFunEvals',6500,'TolX',1e-8,'TolFun',1e-8);
best=Inf;
for d=[0.003 0.03 0.3]
    for td=[0.05 1.5 5]
        [x,v,flag]=fminsearch(@(xx) cost(xx,f,h,j,pd,w),[log(0.012),log(d),td],opts);
        if v<best, best=v; bx=x; bf=flag; end
    end
end
fit=struct('x',bx,'cost',best,'flag',bf,'model',response(bx,f,j,pd), ...
    'boundary',bx(3)<1e-4 || bx(3)>19.999 || any(bx(1:2)<-17.9));
% Reoptimize J and delay at fixed half/double D: sensitivity, not CI.
profile=zeros(1,2);
for factor=1:2
    fixedD=bx(2)+log([0.5 2]); fd=fixedD(factor);
    [~,profile(factor)]=fminsearch(@(z) cost([z(1),fd,z(2)],f,h,j,pd,w),[bx(1),bx(3)],opts);
end
fit.DhalfCost=profile(1); fit.DdoubleCost=profile(2);
end
