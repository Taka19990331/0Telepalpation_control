function result = check_sampling_consistency()
% PD+chirp only: clock diagnostics and timing sensitivity, not exact recovery.
% Millisecond RX/log timestamps cannot recover sub-ms sampling or CAN latency.
SHOW_WRAPPED_PHASE=false;
here=fileparts(mfilename('fullpath')); out=fullfile(here,'sampling_consistency_results');
if ~exist(out,'dir'), mkdir(out); end
fid=fopen(fullfile(out,'execution_log.txt'),'w'); fclose(fid);
diary(fullfile(out,'execution_log.txt')); clean=onCleanup(@() diary('off'));
names={'motor_data_20260703_172605.csv','motor_data_20260703_170331.csv'};
bands=[1.2 19;12 110]; diagnostics=cell(1,2); records=cell(1,2); differences={};
labels={'Fixed 2000 Hz','Mean-rate row clock','Timestamp grid 1000 Hz (approx.)'};
for k=1:2
    d=readtable(fullfile(fileparts(here),'Data','AK80_Motor1',names{k}));
    t=(double(d.Time_ms)-double(d.Time_ms(1)))/1000; n=height(d);
    assert(all(diff(t)>=0)&&all(isfinite(d{:,:}),'all'));
    fmean=(n-1)/t(end); index=(0:n-1)';
    residualMean=t-index/fmean; residualFixed=t-index/2000;
    % Cumulative timing residual is centered for visualization only.
    counts=histcounts(t,0:floor(t(end)));
    gaps=find(diff(t)>0.0015);
    gapTable=table(gaps+1,t(gaps),t(gaps+1),1000*(t(gaps+1)-t(gaps)), ...
        'VariableNames',{'NextRow','Before_s','After_s','Gap_ms'});
    writetable(gapTable,fullfile(out,sprintf('file%d_timestamp_gaps.csv',k)));
    s.File=string(names{k}); s.Rows=n; s.Duration_s=t(end); s.MeanRate_Hz=fmean;
    s.MeanPeriod_us=1e6/fmean; s.FixedClockEndError_s=residualFixed(end);
    s.FrequencyScaleError_pct=(2000/fmean-1)*100;
    s.MaxMeanClockResidual_ms=max(abs(residualMean))*1000;
    s.MeanClockResidualRange_ms=(max(residualMean)-min(residualMean))*1000;
    s.DuplicateTimestampRows=nnz(diff(t)==0); s.GapsOver1ms=height(gapTable);
    s.MaxGap_ms=max(diff(t))*1000;
    s.OneSecondCountMin=min(counts(2:end-1)); s.OneSecondCountMax=max(counts(2:end-1));
    diagnostics{k}=s;
    fig=figure('Visible','off','Position',[30 30 1300 1050]); tiledlayout(4,1);
    nexttile; plot(t,residualFixed); grid on; ylabel('t - row/2000 [s]');
    title(names{k},'Interpreter','none');
    nexttile; plot(t,residualMean*1000); grid on; ylabel('t - row/Fmean [ms]');
    nexttile; stairs(0:numel(counts)-1,counts); grid on; ylabel('Rows / 1 s');
    nexttile; plot(t(2:end),diff(t)*1000,'.','MarkerSize',2); grid on;
    xlabel('Elapsed logged time [s]'); ylabel('Timestamp step [ms]');
    exportgraphics(fig,fullfile(out,sprintf('file%d_timing.png',k)),'Resolution',150); close(fig);

    active=find(abs(d.Chirp_Torque_Nm)>1e-4);
    use=t>=5 & t<=t(active(end))-0.5;
    u=d.Command_Torque_Nm; y=deg2rad(d.Position_deg);
    % Full raw-row baseline, matching preceding independent identification.
    [h1,f1,c1]=estimate({[u(use),y(use)]},2000,4096);
    [h2,f2,c2]=estimate({[u(use),y(use)]},fmean,4096);
    % Combine duplicate integer-ms time tags by averaging both channels alike.
    % This is a 1 ms bin average, not a validated simultaneous hardware sample.
    [tu,~,g]=unique(t);
    uu=accumarray(g,u,[],@mean); yy=accumarray(g,y,[],@mean);
    % Never interpolate through a missing millisecond: split into runs first.
    boundaries=[1;find(diff(tu)>0.0015)+1;numel(tu)+1];
    segments={}; segmentRows={}; ns=0;
    winMs=round(4096/fmean*1000); % match physical window duration (~2.07 s)
    for z=1:numel(boundaries)-1
        rows=boundaries(z):boundaries(z+1)-1;
        rows=rows(tu(rows)>=5 & tu(rows)<=t(active(end))-0.5);
        if numel(rows)<winMs, continue; end
        tg=(ceil(tu(rows(1))*1000):floor(tu(rows(end))*1000))'/1000;
        if numel(tg)<winMs, continue; end
        us=interp1(tu(rows),uu(rows),tg); ys=interp1(tu(rows),yy(rows),tg);
        assert(all(isfinite(us))&&all(isfinite(ys)));
        ns=ns+1; segments{ns}=[us,ys];
        st.Segment=ns; st.Start_s=tg(1); st.End_s=tg(end); st.Samples=numel(tg); segmentRows{ns}=st;
    end
    assert(~isempty(segments));
    [h3,f3,c3]=estimate(segments,1000,winMs);
    writetable(struct2table([segmentRows{:}]),fullfile(out,sprintf('file%d_uniform_segments.csv',k)));
    f=logspace(log10(bands(k,1)),log10(bands(k,2)),350)';
    H=[interp1(f1,h1,f),interp1(f2,h2,f),interp1(f3,h3,f)];
    C=[interp1(f1,c1,f),interp1(f2,c2,f),interp1(f3,c3,f)];
    for method=[1 3]
        good=C(:,method)>=0.9 & C(:,2)>=0.9;
        dm=20*log10(abs(H(:,method)./H(:,2)));
        dp=angle(H(:,method).*conj(H(:,2)))*180/pi;
        ds.FileIndex=k; ds.Method=string(labels{method}); ds.Points=nnz(good);
        ds.MagnitudeDifferenceRMS_dB=rms(dm(good)); ds.PhaseDifferenceRMS_deg=rms(dp(good));
        ds.MagnitudeDifferenceMax_dB=max(abs(dm(good))); ds.PhaseDifferenceMax_deg=max(abs(dp(good)));
        differences{end+1}=ds;
    end
    tab=table(f,'VariableNames',{'Frequency_Hz'});
    for method=1:3
        tab.(sprintf('Method%d_Magnitude_dB',method))=20*log10(abs(H(:,method)));
        tab.(sprintf('Method%d_Phase_deg',method))=phase(H(:,method));
        tab.(sprintf('Method%d_Coherence',method))=C(:,method);
    end
    writetable(tab,fullfile(out,sprintf('file%d_frf_comparison.csv',k)));
    fig=figure('Visible','off','Position',[30 30 1300 1050]); tiledlayout(3+SHOW_WRAPPED_PHASE,1);
    nexttile; semilogx(f,20*log10(abs(H)),'LineWidth',1.1); grid on;
    ylabel('Magnitude [dB rad/Nm]'); title([names{k} ' | PD + chirp input'],'Interpreter','none'); legend(labels,'Location','best');
    nexttile; semilogx(f,phase(H),'LineWidth',1.1); grid on; ylabel('Phase [deg]');
    nexttile; semilogx(f,C,'LineWidth',1.1); grid on; ylabel('Coherence'); ylim([0.8 1.005]); xlabel('Frequency [Hz]');
    if SHOW_WRAPPED_PHASE, nexttile; semilogx(f,angle(H)*180/pi); grid on; ylabel('Wrapped phase [deg]'); end
    exportgraphics(fig,fullfile(out,sprintf('file%d_frf_comparison.png',k)),'Resolution',150); close(fig);
    records{k}=struct('frequency',f,'H',H,'coherence',C,'meanRate',fmean, ...
        'uniformSegments',struct2table([segmentRows{:}]));
end
summary=struct2table([diagnostics{:}]); comparison=struct2table([differences{:}]);
disp(summary); disp(comparison);
writetable(summary,fullfile(out,'timing_summary.csv')); writetable(comparison,fullfile(out,'frf_sensitivity.csv'));
result=struct('timing',summary,'comparison',comparison,'records',{records});
save(fullfile(out,'sampling_results.mat'),'result');
end

function [H,f,C]=estimate(segments,Fs,winLen)
% Pool Welch spectra by number of contributing windows; no cross-gap windows.
overlap=floor(winLen/2); hop=winLen-overlap; w=hann(winLen); nfft=32768;
total=0;
for k=1:numel(segments)
    u=detrend(segments{k}(:,1)); y=detrend(segments{k}(:,2));
    count=floor((numel(u)-winLen)/hop)+1;
    if count<1, continue; end
    % Derive the cross spectrum from tfestimate to preserve its input/output
    % phase convention when pooling disconnected segments.
    [hs,f]=tfestimate(u,y,w,overlap,nfft,Fs);
    xx=pwelch(u,w,overlap,nfft,Fs); yy=pwelch(y,w,overlap,nfft,Fs);
    xy=hs.*xx;
    if total==0, Sxy=0*xy; Sxx=0*xx; Syy=0*yy; end
    Sxy=Sxy+count*xy; Sxx=Sxx+count*xx; Syy=Syy+count*yy; total=total+count;
end
assert(total>0); H=Sxy./Sxx; C=abs(Sxy).^2./(Sxx.*Syy);
if numel(segments)==1
    assert(max(abs(H-hs)./max(abs(hs),eps))<1e-10,'FRF convention mismatch');
end
end
function p=phase(h)
p=unwrap(angle(h),[],1)*180/pi; p=p-360*ceil(p(1,:)/360);
end
