function allResults = analyze_remaining_motors()
% Remaining Motor2/Motor3: raw diagnostics, overlap, and common-model fits.
% Signal Processing Toolbox required. Original files are never modified.
SHOW_WRAPPED_PHASE = false;
MAX_GAP_S = 0.010; % AK60: never interpolate across gaps longer than 10 ms.
here=fileparts(mfilename('fullpath')); root=fileparts(here);
outRoot=fullfile(here,'remaining_motor_results');
if ~exist(outRoot,'dir'), mkdir(outRoot); end
fid=fopen(fullfile(outRoot,'execution_log.txt'),'w'); fclose(fid);
diary(fullfile(outRoot,'execution_log.txt')); cleanup=onCleanup(@() diary('off'));
labels={'1: Chirp cmd only input','2: PD + chirp cmd input','3: Measured torque input'};
cfg(1)=struct('motor','AK60_Motor3','files',{{'motor_data_20260807_111957.csv','motor_data_20260807_113045.csv'}}, ...
    'bands',[1 70;25 140],'join',40,'Fs',500,'regular',false);
cfg(2)=struct('motor','AK80_Motor2','files',{{'motor_data_20260804_173754.csv','motor_data_20260804_180214.csv'}}, ...
    'bands',[1 13;10 135],'join',12,'Fs',1/0.001998,'regular',true);
allResults=cell(1,2);
for m=1:2
    c=cfg(m); out=fullfile(outRoot,c.motor); if ~exist(out,'dir'), mkdir(out); end
    raw=cell(1,2); est=cell(1,2); summary=cell(1,2); sensitivity=cell(1,2);
    for k=1:2
        d=readtable(fullfile(root,'Data',c.motor,c.files{k}));
        n=height(d); assert(all(isfinite(d{:,:}),'all'),'Nonfinite raw data');
        if c.regular
            t=(d.Control_Time_us-d.Control_Time_us(1))*1e-6;
            torque=d.Measured_Torque_Nm; velocity=d.Velocity_rad_s;
        else
            t=(d.Time_ms-d.Time_ms(1))*1e-3;
            torque=d.Estimated_Torque_Nm; velocity=d.Speed_rad_s;
        end
        dt=diff(t); assert(all(dt>=0),'Timestamp reversal');
        q=d.Chirp_Torque_Nm; cmd=d.Command_Torque_Nm; pd=d.PD_Torque_Nm;
        y=deg2rad(d.Position_deg); x=[q cmd torque y];
        active=find(abs(q)>1e-4); assert(~isempty(active));
        activeMask=t>=t(active(1)) & t<=t(active(end));
        mismatch=abs(cmd-pd-q)>1e-3;
        valid=d.Error_Code==0 & ~mismatch;
        if c.regular
            valid=valid & d.Feedback_Valid~=0 & d.Safety_OK~=0;
            assert(max(abs(dt-1/c.Fs))<1e-8,'Motor2 timestamps not uniform');
        end
        % Exclude first 3 s of excitation and non-chirp tails, consistently.
        valid=valid & activeMask & t>=t(active(1))+3;
        [runs,tu,xu]=makeRuns(t,x,valid,c.Fs,c.regular,MAX_GAP_S,'linear');
        e=spectral(runs,c.Fs); e.band=c.bands(k,:); e.file=c.files{k};
        assert(e.windows>=4,'Too few accepted Welch windows');
        est{k}=e;
        if c.regular
            near=e;
        else
            nearRuns=makeRuns(t,x,valid,c.Fs,false,MAX_GAP_S,'nearest');
            near=spectral(nearRuns,c.Fs);
        end
        band=e.f>=c.bands(k,1) & e.f<=c.bands(k,2);
        sense=table(e.f(band),'VariableNames',{'Frequency_Hz'});
        for j=1:3
            prefix=sprintf('Input%d_',j);
            sense.([prefix 'NearestMinusLinear_dB'])=20*log10(abs(near.H(band,j)./e.H(band,j)));
            sense.([prefix 'NearestMinusLinear_deg'])=angle(near.H(band,j).*conj(e.H(band,j)))*180/pi;
            sense.([prefix 'LinearCoherence'])=e.coh(band,j);
            sense.([prefix 'NearestCoherence'])=near.coh(band,j);
        end
        sensitivity{k}=sense;
        [~,name]=fileparts(c.files{k}); writetable(sense,fullfile(out,[name '_resampling_sensitivity.csv']));
        s.File=string(c.files{k}); s.Rows=n; s.Duration_s=t(end); s.AverageRate_Hz=(n-1)/t(end);
        s.AnalysisFs_Hz=c.Fs; s.DuplicateTimestampRows=nnz(dt==0);
        s.MaxGap_ms=max(dt)*1000; s.GapsOver10ms=nnz(dt>MAX_GAP_S+1e-9);
        activeDt=dt(activeMask(1:end-1)&activeMask(2:end));
        s.ActiveMaxGap_ms=max(activeDt)*1000;
        s.ActiveGapsOver10ms=nnz(activeDt>MAX_GAP_S+1e-9);
        s.SumMismatchRows=nnz(mismatch); s.NonzeroErrorRows=nnz(d.Error_Code~=0);
        s.ActiveStart_s=t(active(1)); s.ActiveEnd_s=t(active(end));
        s.ChirpAmplitude_Nm=max(abs(q)); s.MaxCommand_Nm=max(abs(cmd));
        s.PositionMin_deg=min(d.Position_deg); s.PositionMax_deg=max(d.Position_deg);
        s.MaxPositionStep_deg=max(abs(diff(d.Position_deg)));
        s.WelchWindows=e.windows; s.WindowDuration_s=e.winLen/c.Fs;
        s.AcceptedUniformSamples=sum(cellfun(@(v)size(v,1),runs));
        s.AcceptedRuns=numel(runs); s.FeedbackInvalidRows=0; s.SafetyNotOKRows=0;
        s.FeedbackAgeMax_us=NaN; s.FeedbackAgeMedian_us=NaN; s.TorqueIqMaxResidual_Nm=NaN;
        if c.regular
            s.FeedbackInvalidRows=nnz(d.Feedback_Valid==0); s.SafetyNotOKRows=nnz(d.Safety_OK==0);
            s.FeedbackAgeMax_us=max(d.Feedback_Age_us); s.FeedbackAgeMedian_us=median(d.Feedback_Age_us);
            freq=d.Chirp_Frequency_Hz; tm=t;
        else
            s.TorqueIqMaxResidual_Nm=max(abs(torque-0.5994*d.Iq_A));
            ix=find(q(1:end-1)<0 & q(2:end)>=0);
            tz=t(ix)-q(ix).*(t(ix+1)-t(ix))./(q(ix+1)-q(ix));
            tm=(tz(1:end-1)+tz(2:end))/2; freq=1./diff(tz);
            keep=isfinite(freq) & freq>0 & freq<c.Fs/2;
            tm=tm(keep); freq=movmedian(freq(keep),15);
        end
        s.FrequencyFirst_Hz=median(freq(find(freq>0,15,'first')));
        s.FrequencyLast_Hz=median(freq(find(freq>0,15,'last')));
        summary{k}=s;
        raw{k}=struct('table',d,'t',t,'x',x,'velocity',velocity,'frequencyTime',tm,'frequency',freq, ...
            'valid',valid,'uniqueTime',tu,'uniqueValues',xu);
        writetable(table(tm,freq,'VariableNames',{'Time_s','Frequency_Hz'}),fullfile(out,[name '_frequency.csv']));
        events=find([false;dt>MAX_GAP_S+1e-9] | mismatch | d.Error_Code~=0);
        previousGap=[NaN;dt];
        ev=table(events,t(events),previousGap(events));
        ev.Properties.VariableNames={'SampleRow','Time_s','PreviousGap_s'};
        ev.SumMismatch=mismatch(events); ev.ErrorCode=d.Error_Code(events);
        writetable(ev,fullfile(out,[name '_events.csv']));
        % Raw overview; all samples shown, no interpolation in these plots.
        fig=figure('Visible','off','Position',[30 30 1300 1050]); tiledlayout(5,1);
        nexttile; plot(t,d.Position_deg); grid on; ylabel('Position [deg]'); title([c.motor ' / ' name],'Interpreter','none');
        nexttile; plot(t,velocity); grid on; ylabel('Speed [rad/s]');
        nexttile; plot(t,[q pd cmd torque]); grid on; ylabel('Torque [Nm]');
        legend('Chirp cmd','PD cmd','PD + chirp cmd','Measured torque');
        nexttile; plot(t(2:end),dt*1000,'.'); grid on; ylabel('Timestamp step [ms]');
        if c.regular, ylim([1.9 2.1]); end % Do not magnify floating-point roundoff.
        nexttile; plot(tm,freq); grid on; ylabel('Excitation [Hz]'); xlabel('Elapsed timestamp [s]');
        exportgraphics(fig,fullfile(out,[name '_timeseries.png']),'Resolution',140); close(fig);
        fig=figure('Visible','off','Position',[30 30 1250 900]); tiledlayout(3+double(SHOW_WRAPPED_PHASE),1);
        nexttile; semilogx(e.f(band),20*log10(abs(e.H(band,:)))); grid on; ylabel('Magnitude [dB rad/Nm]'); legend(labels,'Location','best');
        title([c.motor ' / ' name ' | timestamp-based analysis'],'Interpreter','none');
        nexttile; semilogx(e.f(band),phaseBranch(e.H(band,:))); grid on; ylabel('Phase [deg]');
        nexttile; semilogx(e.f(band),e.coh(band,:)); grid on; ylim([0 1.02]); yline(.9,'--'); ylabel('Coherence'); xlabel('Frequency [Hz]');
        if SHOW_WRAPPED_PHASE, nexttile; semilogx(e.f(band),angle(e.H(band,:))*180/pi); ylabel('Wrapped phase [deg]'); grid on; end
        exportgraphics(fig,fullfile(out,[name '_frf.png']),'Resolution',140); close(fig);
        ft=table(e.f(band),'VariableNames',{'Frequency_Hz'});
        ph=phaseBranch(e.H(band,:));
        for j=1:3
            p=sprintf('Input%d_',j); ft.([p 'Magnitude_dB'])=20*log10(abs(e.H(band,j)));
            ft.([p 'Phase_deg'])=ph(:,j); ft.([p 'Coherence'])=e.coh(band,j);
        end
        writetable(ft,fullfile(out,[name '_frf.csv']));
    end
    diagnostics=struct2table([summary{:}]); disp(diagnostics); writetable(diagnostics,fullfile(out,'diagnostics.csv'));
    f=est{1}.f; assert(isequal(f,est{2}.f));
    ob=[max(c.bands(:,1)),min(c.bands(:,2))]; in=f>=ob(1)&f<=ob(2); fo=f(in);
    overlapRows=cell(1,3); ot=table(fo,'VariableNames',{'Frequency_Hz'});
    fig=figure('Visible','off','Position',[30 30 1500 1000]); tiledlayout(3,3);
    for j=1:3
        h=[est{1}.H(in,j),est{2}.H(in,j)]; co=[est{1}.coh(in,j),est{2}.coh(in,j)];
        dm=20*log10(abs(h(:,2)./h(:,1))); dp=angle(h(:,2).*conj(h(:,1)))*180/pi;
        good=all(co>=.9,2); z.Input=string(labels{j}); z.Points=nnz(good);
        z.MagRms_dB=rms(dm(good)); z.PhaseRms_deg=rms(dp(good));
        z.MagMedian_dB=median(dm(good)); z.PhaseMedian_deg=median(dp(good));
        z.AllPointsMagRms_dB=rms(dm); z.AllPointsPhaseRms_deg=rms(dp);
        overlapRows{j}=z;
        p=sprintf('Input%d_',j); ot.([p 'Difference_dB'])=dm; ot.([p 'Difference_deg'])=dp;
        ot.([p 'LowCoherence'])=co(:,1); ot.([p 'HighCoherence'])=co(:,2);
        nexttile(j); plot(fo,20*log10(abs(h))); grid on; title(labels{j}); ylabel('Magnitude [dB]'); legend('Low-band file','High-band file');
        nexttile(j+3); plot(fo,phaseBranch(h)); grid on; ylabel('Phase [deg]');
        nexttile(j+6); plot(fo,co); grid on; ylim([0 1.02]); yline(.9,'--'); ylabel('Coherence'); xlabel('Frequency [Hz]');
    end
    sgtitle(sprintf('%s | overlap %.1f-%.1f Hz | no forced alignment',c.motor,ob),'Interpreter','none');
    exportgraphics(fig,fullfile(out,'overlap_frf.png'),'Resolution',150); close(fig);
    overlapSummary=struct2table([overlapRows{:}]); disp(overlapSummary);
    writetable(overlapSummary,fullfile(out,'overlap_summary.csv')); writetable(ot,fullfile(out,'overlap_difference.csv'));
    fig=figure('Visible','off','Position',[30 30 1450 650]); tiledlayout(2,3);
    for j=1:3
        p=sprintf('Input%d_',j); nexttile(j); plot(fo,ot.([p 'Difference_dB'])); grid on; yline(0,'--'); title(labels{j}); ylabel('High - low [dB]');
        nexttile(j+3); plot(fo,ot.([p 'Difference_deg'])); grid on; yline(0,'--'); ylabel('High - low [deg]'); xlabel('Frequency [Hz]');
    end
    exportgraphics(fig,fullfile(out,'overlap_difference.png'),'Resolution',150); close(fig);
    % Raw zoom at a common excitation frequency plus the worst gap.
    fig=figure('Visible','off','Position',[30 30 1450 1000]); tiledlayout(4,2);
    for k=1:2
        zoomHz=mean(ob); if ~c.regular, zoomHz=35; end
        rr=raw{k}; [~,ix]=min(abs(rr.frequency-zoomHz)); center=rr.frequencyTime(ix);
        sel=abs(rr.t-center)<=3/zoomHz;
        nexttile(k); plot(rr.t(sel)-center,rr.x(sel,4)*180/pi-mean(rr.x(sel,4))*180/pi,'.-'); grid on; ylabel('Position, centered [deg]');
        title(sprintf('%s near %.1f Hz',c.files{k},zoomHz),'Interpreter','none');
        nexttile(k+2); plot(rr.t(sel)-center,rr.x(sel,1:3),'.-'); grid on; ylabel('Torque [Nm]'); legend(labels,'Location','best');
        [~,ig]=max(diff(rr.t)); gc=rr.t(ig+1); sel=rr.t>=rr.t(ig)-.1 & rr.t<=gc+.1;
        nexttile(k+4); plot(rr.t(sel),rr.x(sel,4)*180/pi,'.-'); grid on; ylabel('Position [deg]');
        if c.regular, title('Uniform 1.998 ms spacing; startup detail');
        else, title('Largest timestamp gap (lines connect recorded samples)'); end
        nexttile(k+6); plot(rr.t(sel),rr.x(sel,1:3),'.-'); grid on; ylabel('Torque [Nm]'); xlabel('Elapsed timestamp [s]');
    end
    exportgraphics(fig,fullfile(out,'overlap_and_gap_timeseries.png'),'Resolution',150); close(fig);
    [fits,parameters,errors]=fitAll(est,c,labels,out);
    disp(parameters); disp(errors);
    allResults{m}=struct('config',c,'spectra',{est},'diagnostics',diagnostics,'overlap',overlapSummary, ...
        'sensitivity',{sensitivity},'fits',{fits},'parameters',parameters,'errors',errors);
    analysis=allResults{m}; save(fullfile(out,'analysis_results.mat'),'analysis');
end
fprintf('Completed all remaining motors: %s\n',outRoot);
end

function [runs,tu,xu]=makeRuns(t,x,valid,Fs,regular,maxGap,method)
if regular
    tu=t; xu=x; good=valid;
else
    [tu,~,g]=unique(t); xu=zeros(numel(tu),size(x,2));
    for j=1:size(x,2), xu(:,j)=accumarray(g,x(:,j),[],@mean); end
    good=accumarray(g,double(valid),[],@min)>0;
end
breaks=[true;diff(tu)>maxGap+1e-9]; goodidx=find(good);
runs={}; if isempty(goodidx), return; end
cuts=[1;find(diff(goodidx)>1 | breaks(goodidx(2:end)))+1;numel(goodidx)+1];
minN=2^nextpow2(2*Fs);
for k=1:numel(cuts)-1
    ix=goodidx(cuts(k):cuts(k+1)-1);
    if numel(ix)<2, continue; end
    if regular, v=xu(ix,:); else
        grid=(tu(ix(1)):1/Fs:tu(ix(end)))'; v=interp1(tu(ix),xu(ix,:),grid,method);
    end
    if size(v,1)>=minN, runs{end+1}=v; end
end
end

function e=spectral(runs,Fs)
nw=2^nextpow2(2*Fs); no=nw/2; nfft=8192; w=hann(nw);
Puu=zeros(nfft/2+1,3); Puy=complex(Puu); Pyy=zeros(nfft/2+1,1); windows=0;
for k=1:numel(runs)
    v=detrend(runs{k}); count=1+floor((size(v,1)-nw)/(nw-no));
    [yy,f]=pwelch(v(:,4),w,no,nfft,Fs); Pyy=Pyy+count*yy;
    for j=1:3
        uu=pwelch(v(:,j),w,no,nfft,Fs);
        % MATLAB cpsd(y,u) is Y*conj(U): H1 = P_yu/P_uu.
        uy=cpsd(v(:,4),v(:,j),w,no,nfft,Fs);
        direct=tfestimate(v(:,j),v(:,4),w,no,nfft,Fs);
        assert(max(abs(uy./uu-direct))<1e-10*max(1,max(abs(direct))), ...
            'Pooled estimator cross-spectrum orientation differs from tfestimate');
        Puu(:,j)=Puu(:,j)+count*uu; Puy(:,j)=Puy(:,j)+count*uy;
    end
    windows=windows+count;
end
assert(windows>0,'No sufficiently long continuous data');
e=struct('f',f,'H',Puy./Puu,'coh',abs(Puy).^2./(Puu.*Pyy),'windows',windows,'winLen',nw);
assert(all(isfinite(e.H),'all') && all(e.coh<=1+1e-8,'all'));
end

function [fits,parameters,errors]=fitAll(est,c,labels,out)
fits=cell(1,3); pr=cell(1,3); er={}; fig=figure('Visible','off','Position',[30 30 1500 900]); tiledlayout(2,3);
for j=1:3
    f=[]; h=[]; coh=[]; src=[];
    for k=1:2
        e=est{k}; band=e.f>=c.bands(k,1)&e.f<=c.bands(k,2);
        if k==1, band=band&e.f<=c.join; else, band=band&e.f>c.join; end
        f=[f;e.f(band)]; h=[h;e.H(band,j)]; coh=[coh;e.coh(band,j)]; src=[src;repmat(k,nnz(band),1)];
    end
    good=coh>=.9 & abs(f-c.join)>.5; weight=ones(size(f)); weight(src==1)=.2;
    pp.Input=string(labels{j}); pp.Points=nnz(good); pp.b=NaN; pp.a=NaN; pp.Td_ms=NaN;
    pp.ApparentJ=NaN; pp.ApparentD=NaN; pp.Cost=NaN; pp.ExitFlag=0; pp.DelayAtBoundary=false;
    model=NaN(size(h));
    if nnz(good)>=10
        best=Inf; opts=optimset('Display','off','MaxIter',3000,'MaxFunEvals',9000,'TolX',1e-8,'TolFun',1e-8);
        for a=[1 20 100]
            for td=[.05 1 3 8]
                [x,v,flag]=fminsearch(@(x)cost(x,f(good),h(good),weight(good)),[log(200) log(a) td],opts);
                if v<best, best=v; bx=x; bf=flag; end
            end
        end
        model=modelFrf(bx,f); pp.b=exp(bx(1)); pp.a=exp(bx(2)); pp.Td_ms=bx(3);
        pp.ApparentJ=1/pp.b; pp.ApparentD=pp.a/pp.b; pp.Cost=best; pp.ExitFlag=bf;
        pp.DelayAtBoundary=pp.Td_ms<.001||pp.Td_ms>19.999;
    end
    pr{j}=pp;
    for k=1:3
        sel=good; if k<3, sel=sel&src==k; end
        z.Input=string(labels{j}); z.BandIndex=k; z.Points=nnz(sel);
        z.MagnitudeRMSE_dB=rms(20*log10(abs(model(sel)./h(sel))));
        z.PhaseRMSE_deg=rms(angle(model(sel).*conj(h(sel)))*180/pi); er{end+1}=z;
    end
    curve=table(f,src,coh,good,20*log10(abs(h)),phaseBranch(h),20*log10(abs(model)),phaseBranch(model), ...
        'VariableNames',{'Frequency_Hz','SourceIndex','Coherence','UsedForFit','MeasuredMagnitude_dB','MeasuredPhase_deg','ModelMagnitude_dB','ModelPhase_deg'});
    writetable(curve,fullfile(out,sprintf('input%d_fit_curve.csv',j)));
    fits{j}=struct('f',f,'H',h,'model',model,'used',good,'coherence',coh);
    figure(fig); nexttile(j); drawFit(f,h,model,src,good,false); title(labels{j});
    nexttile(j+3); drawFit(f,h,model,src,good,true);
    one=figure('Visible','off','Position',[30 30 1200 800]); tiledlayout(2,1);
    nexttile; drawFit(f,h,model,src,good,false); title([c.motor ' | ' labels{j}]);
    nexttile; drawFit(f,h,model,src,good,true);
    exportgraphics(one,fullfile(out,sprintf('input%d_model_overlay.png',j)),'Resolution',150); close(one);
end
figure(fig); sgtitle({[c.motor ' | common model b/(s^2+a*s) exp(-Td*s)'], ...
    'In-sample exploratory fits; input 1 omits PD stiffness; torque feedback uncalibrated'},'Interpreter','none');
exportgraphics(fig,fullfile(out,'three_model_overlays.png'),'Resolution',150); close(fig);
parameters=struct2table([pr{:}]); errors=struct2table([er{:}]);
writetable(parameters,fullfile(out,'fit_parameters.csv')); writetable(errors,fullfile(out,'fit_errors.csv'));
end
function h=modelFrf(x,f)
s=1i*2*pi*f; h=exp(x(1))./(s.^2+exp(x(2))*s).*exp(-s*x(3)/1000);
end
function v=cost(x,f,h,w)
if any(~isfinite(x))||any(abs(x(1:2))>25)||x(3)<0||x(3)>20, v=1e10+sum(x.^2); return; end
m=modelFrf(x,f); v=sum(w.*((20*log10(abs(m./h))).^2+.05*(angle(m.*conj(h))*180/pi).^2))/sum(w);
end
function p=phaseBranch(h)
p=unwrap(angle(h),[],1)*180/pi; p=p-360*ceil(p(1,:)/360);
end
function drawFit(f,h,m,src,used,isPhase)
if isPhase, y=phaseBranch(h); ym=phaseBranch(m); else, y=20*log10(abs(h)); ym=20*log10(abs(m)); end
semilogx(f(src==1),y(src==1),'.','Color',[0 .35 .8]); hold on;
semilogx(f(src==2),y(src==2),'.','Color',[.2 .65 .3]);
semilogx(f,ym,'-','Color',[.9 .25 .05],'LineWidth',1.3);
semilogx(f(~used),y(~used),'kx','MarkerSize',3); grid on; xlabel('Frequency [Hz]');
if isPhase, ylabel('Phase [deg]'); else, ylabel('Magnitude [dB rad/Nm]'); end
legend('Experimental FRF: low-band file','Experimental FRF: high-band file','Fitted model','Excluded from fit','Location','best');
end
