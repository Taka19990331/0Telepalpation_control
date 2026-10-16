function analysis = analyze_overlap_and_fit()
% Independent overlap check and common-model fitting of three input FRFs.
% Same original Fs/preprocessing/bands/weights to isolate input definitions.
% This is an in-sample fit, NOT validation of physical J/D or a new controller.
SHOW_WRAPPED_PHASE = false;
here = fileparts(mfilename('fullpath'));
out = fullfile(here,'overlap_and_fit_results');
if ~exist(out,'dir'), mkdir(out); end
fid=fopen(fullfile(out,'execution_log.txt'),'w'); fclose(fid);
diary(fullfile(out,'execution_log.txt')); cleanup=onCleanup(@() diary('off'));
labels={'1: Chirp cmd only input','2: PD + chirp cmd input','3: Measured torque input'};
r=compare_identification_inputs(false);
assert(isequal(r(1).frequencyHz,r(2).frequencyHz));
f=r(1).frequencyHz; overlap=f>=10 & f<=20;
fo=f(overlap); rows=cell(1,3); overlapData=table(fo,'VariableNames',{'Frequency_Hz'});
fig=figure('Visible','off','Position',[30 30 1550 1050]);
tiledlayout(3+double(SHOW_WRAPPED_PHASE),3,'TileSpacing','compact');
phase=cell(1,3);
for j=1:3
    h=[r(1).H(overlap,j),r(2).H(overlap,j)];
    c=[r(1).coherence(overlap,j),r(2).coherence(overlap,j)];
    phase{j}=phaseBranch(h);
    deltaMag=20*log10(abs(h(:,2)./h(:,1)));
    deltaPhase=angle(h(:,2).*conj(h(:,1)))*180/pi;
    good=all(c>=0.9,2);
    assert(any(good),'No reliable overlap points for input %d',j);
    s.Input=string(labels{j}); s.Points=nnz(good);
    s.MagnitudeDifferenceMedian_dB=median(deltaMag(good));
    s.MagnitudeDifferenceRms_dB=rms(deltaMag(good));
    s.MagnitudeDifferenceMaxAbs_dB=max(abs(deltaMag(good)));
    s.PhaseDifferenceMedian_deg=median(deltaPhase(good));
    s.PhaseDifferenceRms_deg=rms(deltaPhase(good));
    s.PhaseDifferenceMaxAbs_deg=max(abs(deltaPhase(good)));
    s.MinCoherenceLow=min(c(:,1)); s.MinCoherenceHigh=min(c(:,2));
    rows{j}=s;
    p=sprintf('Input%d_',j);
    overlapData.([p 'LowMagnitude_dB'])=20*log10(abs(h(:,1)));
    overlapData.([p 'HighMagnitude_dB'])=20*log10(abs(h(:,2)));
    overlapData.([p 'LowPhase_deg'])=phase{j}(:,1);
    overlapData.([p 'HighPhase_deg'])=phase{j}(:,2);
    overlapData.([p 'DeltaMagnitude_dB'])=deltaMag;
    overlapData.([p 'DeltaPhase_deg'])=deltaPhase;
    overlapData.([p 'LowCoherence'])=c(:,1);
    overlapData.([p 'HighCoherence'])=c(:,2);
    nexttile(j); plot(fo,20*log10(abs(h)),'LineWidth',1.3); grid on;
    title(labels{j}); ylabel('Magnitude [dB rad/Nm]');
    legend('Low-band experiment (2 Nm)','High-band experiment (5 Nm)','Location','best');
    nexttile(3+j); plot(fo,phase{j},'LineWidth',1.3); grid on; ylabel('Phase [deg]');
    nexttile(6+j); plot(fo,c,'LineWidth',1.3); grid on; yline(0.9,'--');
    ylim([0.85 1.005]); ylabel('Coherence'); xlabel('Frequency [Hz]');
    if SHOW_WRAPPED_PHASE
        nexttile(9+j); plot(fo,angle(h)*180/pi); grid on; ylabel('Wrapped phase [deg]');
    end
end
sgtitle('Overlap: 10-20 Hz | original Fs=2000 Hz retained | no amplitude/phase alignment');
exportgraphics(fig,fullfile(out,'overlap_frf.png'),'Resolution',150); close(fig);
overlapSummary=struct2table([rows{:}]); disp(overlapSummary);
writetable(overlapSummary,fullfile(out,'overlap_summary.csv'));
writetable(overlapData,fullfile(out,'overlap_frf.csv'));
fig=figure('Visible','off','Position',[30 30 1450 700]); tiledlayout(2,3);
for j=1:3
    p=sprintf('Input%d_',j);
    nexttile(j); plot(fo,overlapData.([p 'DeltaMagnitude_dB'])); yline(0,'--'); grid on;
    title(labels{j}); ylabel('High - low [dB]');
    nexttile(3+j); plot(fo,overlapData.([p 'DeltaPhase_deg'])); yline(0,'--'); grid on;
    ylabel('High - low phase [deg]'); xlabel('Frequency [Hz]');
end
sgtitle('Overlap difference (not corrected to force agreement)');
exportgraphics(fig,fullfile(out,'overlap_difference.png'),'Resolution',150); close(fig);

% Use positive-going chirp zero crossings to locate actual 10-20 Hz samples.
timeStats=cell(1,2); spectralStats=cell(1,2);
fig=figure('Visible','off','Position',[30 30 1450 1100]); tiledlayout(4,2,'TileSpacing','compact');
for k=1:2
    d=readtable(r(k).file); t=(d.Time_ms-d.Time_ms(1))/1000;
    q=d.Chirp_Torque_Nm;
    ix=find(q(1:end-1)<0 & q(2:end)>=0);
    tz=t(ix)-q(ix).*(t(ix+1)-t(ix))./(q(ix+1)-q(ix));
    ok=diff(tz)>0;
    tm=(tz(1:end-1)+tz(2:end))/2; fc=1./diff(tz);
    tm=tm(ok); fc=fc(ok);
    ov=fc>=10 & fc<=20;
    assert(nnz(ov)>5,'Insufficient chirp cycles in overlap');
    % Smooth the estimated instantaneous frequency only for segment selection.
    fsmooth=movmedian(fc,9);
    ov=fsmooth>=10 & fsmooth<=20;
    range=[min(tm(ov)),max(tm(ov))];
    selected=t>=range(1) & t<=range(2);
    [~,mid]=min(abs(fsmooth-15)); midTime=tm(mid);
    zoom=t>=midTime-0.2 & t<=midTime+0.2;
    [~,name]=fileparts(r(k).file);
    st.File=string(name); st.OverlapStart_s=range(1); st.OverlapEnd_s=range(2);
    st.ZoomCenter_s=midTime; st.ZoomEstimatedFrequency_Hz=fsmooth(mid);
    st.OverlapRows=nnz(selected); st.PositionSpan_deg=rangeValue(d.Position_deg(selected));
    st.ChirpRms_Nm=rms(q(selected)); st.CommandRms_Nm=rms(d.Command_Torque_Nm(selected));
    st.FeedbackRms_Nm=rms(d.Measured_Torque_Nm(selected));
    st.PDRms_Nm=rms(d.PD_Torque_Nm(selected));
    ds=diff(t); st.MaxTimestampGap_ms=max(ds(find(selected,1):find(selected,1,'last')-1))*1000;
    st.NonzeroErrorRows=nnz(d.Error_Code(selected)~=0);
    st.SumResidualMax_Nm=max(abs(d.Command_Torque_Nm(selected)-d.PD_Torque_Nm(selected)-q(selected)));
    timeStats{k}=st;
    writetable(d(selected,:),fullfile(out,[name '_overlap_raw.csv']));
    writetable(table(tm,fc,fsmooth,'VariableNames',{'Time_s','CycleFrequency_Hz','Median9CycleFrequency_Hz'}), ...
        fullfile(out,[name '_chirp_frequency.csv']));
    nexttile(k); plot(tm,fc,'.','MarkerSize',2); hold on; plot(tm,fsmooth,'LineWidth',1);
    yline(10,'--'); yline(20,'--'); grid on; ylim([0 min(140,max(fsmooth)*1.1)]);
    xlabel('Elapsed timestamp [s]'); ylabel('Chirp frequency [Hz]'); title(name,'Interpreter','none');
    nexttile(2+k); plot(t(zoom)-midTime,d.Position_deg(zoom)-mean(d.Position_deg(zoom)),'.-','MarkerSize',3);
    grid on; ylabel('Position, mean removed [deg]'); title('Original samples near 15 Hz');
    nexttile(4+k); plot(t(zoom)-midTime,d.Speed_rad_s(zoom),'.-','MarkerSize',3); grid on; ylabel('Speed [rad/s]');
    nexttile(6+k); plot(t(zoom)-midTime,[q(zoom),d.PD_Torque_Nm(zoom),d.Command_Torque_Nm(zoom),d.Measured_Torque_Nm(zoom)]);
    grid on; ylabel('Torque [Nm]'); xlabel('Time relative to selected center [s]');
    legend('Chirp cmd','PD cmd','PD + chirp cmd','Measured torque','Location','best');
    % Actual overlap-segment FRFs: independent sensitivity check, same Fs/window.
    u=detrend(d{selected,r(k).inputColumns}); y=detrend(deg2rad(d.Position_deg(selected)));
    segH=complex(zeros(numel(f),3)); segC=zeros(numel(f),3);
    for j=1:3
        segH(:,j)=tfestimate(u(:,j),y,hann(4096),2048,32768,2000);
        segC(:,j)=mscohere(u(:,j),y,hann(4096),2048,32768,2000);
    end
    spectralStats{k}=struct('H',segH,'coherence',segC);
end
sgtitle('Time-domain overlap check | unequal excitation amplitudes are preserved');
exportgraphics(fig,fullfile(out,'overlap_timeseries.png'),'Resolution',150); close(fig);
timeSummary=struct2table([timeStats{:}]); disp(timeSummary);
writetable(timeSummary,fullfile(out,'overlap_time_summary.csv'));
fig=figure('Visible','off','Position',[30 30 1450 900]); tiledlayout(3,3);
for j=1:3
    hh=[spectralStats{1}.H(overlap,j),spectralStats{2}.H(overlap,j)];
    cc=[spectralStats{1}.coherence(overlap,j),spectralStats{2}.coherence(overlap,j)];
    nexttile(j); plot(fo,20*log10(abs(hh))); grid on; title(labels{j}); ylabel('Magnitude [dB]');
    legend('Low-band experiment','High-band experiment');
    nexttile(3+j); plot(fo,phaseBranch(hh)); grid on; ylabel('Phase [deg]');
    nexttile(6+j); plot(fo,cc); grid on; ylim([0 1.01]); ylabel('Coherence'); xlabel('Frequency [Hz]');
end
sgtitle('Sensitivity check: FRF from only the time intervals exciting 10-20 Hz');
exportgraphics(fig,fullfile(out,'overlap_segment_frf.png'),'Resolution',150); close(fig);

% Fit ALL THREE regardless of overlap outcome. Same model for fair comparison:
% H=b/(s^2+a*s)*exp(-Td*s). Input 1 omits holding stiffness: baseline ONLY.
% Positive b,a; 0<=Td<=20 ms. Circular phase residual removes wrap artifacts.
fits=cell(1,3); metricRows={}; paramRows=cell(1,3);
figAll=figure('Visible','off','Position',[30 30 1550 900]); tiledlayout(2,3);
for j=1:3
    ff=[]; hh=[]; source=[]; coherence=[];
    for k=1:2
        band=f>=r(k).comparisonBandHz(1) & f<=r(k).comparisonBandHz(2);
        ff=[ff;f(band)]; hh=[hh;r(k).H(band,j)];
        source=[source;repmat(k,nnz(band),1)]; coherence=[coherence;r(k).coherence(band,j)];
    end
    reliable=coherence>=0.9 & ~(ff>=9.5 & ff<=10.5);
    weight=ones(size(ff)); weight(ff<10)=0.2;
    fun=@(x) objective(x,ff(reliable),hh(reliable),weight(reliable));
    opts=optimset('Display','off','MaxIter',3000,'MaxFunEvals',9000,'TolX',1e-9,'TolFun',1e-9);
    best=Inf; bestx=[]; bestFlag=0;
    for aa=[1 20 100]
        for td=[0.05 1 3 8]
            [x,cost,flag]=fminsearch(fun,[log(200),log(aa),td],opts);
            if cost<best, best=cost; bestx=x; bestFlag=flag; end
        end
    end
    assert(isfinite(best) && best<1e8,'Fit failed');
    b=exp(bestx(1)); a=exp(bestx(2)); Td=bestx(3)/1000;
    model=modelFrf(bestx,ff);
    pp.Input=string(labels{j}); pp.b=b; pp.a=a; pp.Td_ms=Td*1000;
    pp.ApparentJ=1/b; pp.ApparentD=a/b; pp.WeightedCost=best; pp.ExitFlag=bestFlag;
    pp.DelayNearBoundary=Td<1e-6 || Td>0.019999;
    pp.FitPoints=nnz(reliable); paramRows{j}=pp;
    for k=1:3
        sel=reliable; if k<=2, sel=sel & source==k; end
        magErr=20*log10(abs(model(sel)./hh(sel)));
        phaseErr=angle(model(sel).*conj(hh(sel)))*180/pi;
        mm.Input=string(labels{j}); mm.BandIndex=k; mm.Points=nnz(sel);
        mm.MagnitudeRMSE_dB=rms(magErr); mm.PhaseRMSE_deg=rms(phaseErr);
        mm.MaxMagnitudeError_dB=max(abs(magErr)); mm.MaxPhaseError_deg=max(abs(phaseErr));
        metricRows{end+1}=mm;
    end
    phaseData=phaseBranch(hh); phaseModel=phaseBranch(model);
    ft=table(ff,source,coherence,reliable,weight,20*log10(abs(hh)),phaseData, ...
        20*log10(abs(model)),phaseModel, ...
        'VariableNames',{'Frequency_Hz','SourceFileIndex','Coherence','UsedForFit','Weight', ...
        'MeasuredMagnitude_dB','MeasuredPhase_deg','ModelMagnitude_dB','ModelPhase_deg'});
    writetable(ft,fullfile(out,sprintf('input%d_fit_curve.csv',j)));
    fits{j}=struct('parameters',pp,'frequencyHz',ff,'measured',hh,'model',model,'used',reliable);
    figure(figAll);
    nexttile(j); drawFit(ff,hh,model,source,reliable,false); title(labels{j});
    nexttile(3+j); drawFit(ff,hh,model,source,reliable,true);
    fig=figure('Visible','off','Position',[30 30 1250 850]); tiledlayout(2,1);
    nexttile; drawFit(ff,hh,model,source,reliable,false);
    title({labels{j},sprintf('Common model: b/(s^2+a*s) exp(-Td*s) | b=%.4g, a=%.4g, Td=%.3f ms',b,a,Td*1000)});
    nexttile; drawFit(ff,hh,model,source,reliable,true);
    exportgraphics(fig,fullfile(out,sprintf('input%d_model_overlay.png',j)),'Resolution',150); close(fig);
end
figure(figAll); sgtitle({'Experimental FRF and fitted common model | assumed Fs=2000 Hz', ...
    'Input 1: baseline model omits PD stiffness. All fits are in-sample; torque feedback is uncalibrated.'});
exportgraphics(figAll,fullfile(out,'three_model_overlays.png'),'Resolution',150); close(figAll);
parameters=struct2table([paramRows{:}]); metrics=struct2table([metricRows{:}]);
disp(parameters); disp(metrics);
writetable(parameters,fullfile(out,'fit_parameters.csv'));
writetable(metrics,fullfile(out,'fit_errors.csv'));
analysis=struct('baseline',r,'overlap',overlapSummary,'time',timeSummary,'fits',{fits}, ...
    'segmentFRFs',{spectralStats},'parameters',parameters,'metrics',metrics);
save(fullfile(out,'analysis_results.mat'),'analysis');
fprintf('Completed. Output: %s\n',out);
end

function p=phaseBranch(h)
p=unwrap(angle(h),[],1)*180/pi;
p=p-360*ceil(p(1,:)/360);
end
function v=rangeValue(x)
v=max(x)-min(x);
end
function h=modelFrf(x,f)
s=1i*2*pi*f;
h=exp(x(1))./(s.^2+exp(x(2))*s).*exp(-s*x(3)/1000);
end
function cost=objective(x,f,h,w)
if any(~isfinite(x)) || any(abs(x(1:2))>25) || x(3)<0 || x(3)>20
    cost=1e10+sum(x.^2); return;
end
m=modelFrf(x,f);
mag=20*log10(abs(m./h)); phase=angle(m.*conj(h))*180/pi;
cost=sum(w.*(mag.^2+0.05*phase.^2))/sum(w);
end
function drawFit(f,h,m,source,used,isPhase)
if isPhase, y=phaseBranch(h); ym=phaseBranch(m); else, y=20*log10(abs(h)); ym=20*log10(abs(m)); end
semilogx(f(source==1),y(source==1),'.','Color',[0 .35 .8],'MarkerSize',7); hold on;
semilogx(f(source==2),y(source==2),'.','Color',[.2 .65 .3],'MarkerSize',7);
semilogx(f,ym,'-','Color',[.9 .25 .05],'LineWidth',1.4);
semilogx(f(~used),y(~used),'kx','MarkerSize',4);
grid on; xlabel('Frequency [Hz]'); xlim([1 100]);
if isPhase, ylabel('Phase [deg]'); else, ylabel('Magnitude [dB rad/Nm]'); end
legend('Experimental FRF: low-band file','Experimental FRF: high-band file', ...
    'Fitted model','Excluded from fit','Location','best');
end
