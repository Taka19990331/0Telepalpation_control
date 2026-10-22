function figures(r)
f=r.frf.frequency_Hz; E=r.frf.H1_real+1i*r.frf.H1_imag;
fig=gobjects(8,1); n=numel(r.trials);
for k=1:8, fig(k)=figure('Visible','on','Color','w','Position',[60 60 1150 850]); end
figure(fig(1)); tiledlayout(3,1);
labels={'Chirp torque [Nm]','Relative angle [rad]','Velocity [rad/s]'};
for j=1:3
    nexttile; hold on;
    for i=1:n
        d=r.trials{i}.d; v={d.u,d.x,d.v}; plot(d.t,v{j});
    end
    ylabel(labels{j}); grid on;
end
xlabel('Time from PD activation [s]'); sgtitle('1: Independent experimental trials');
figure(fig(2)); tiledlayout(2,1);
for j=1:2
    nexttile; plot(1:n,r.individual{:,j+2},'o-'); hold on;
    yline(r.pooled(j),'r--','All-trial common'); grid on;
    labs={'J [kg m^2]','B [Nm s/rad]'}; ylabel(labs{j});
end
xlabel('Trial'); sgtitle('2: Individual and common parameters');
figure(fig(3)); tiledlayout(2,1);
c=r.trials{1}.c; w=2*pi*f;
analytic=1./(c.Kp-r.pooled(1)*w.^2+1i*(r.pooled(2)+c.Kd)*w);
for j=1:2
    nexttile; hold on;
    for i=1:n, semilogx(f,viewH(r.H(:,i),j),'Color',[.7 .7 .7]); end
    h1=semilogx(f,viewH(E,j),'k','LineWidth',1.7);
    hm=semilogx(f,viewH(r.frf.mean_real+1i*r.frf.mean_imag,j),'b--');
    ha=semilogx(f,viewH(analytic,j),'r:','LineWidth',1.5);
    hl=semilogx(f,viewH(r.modelLocal(:,1),j),'m','LineWidth',1.3);
    set(gca,'XScale','log'); grid on; axislabel(j);
    legend([h1 hm ha hl],{'Experimental H1','Complex mean','A analytic','A same estimator'},'Location','best');
end
xlabel('Frequency [Hz]'); sgtitle('3: FRFs (all estimable windows retained)');
figure(fig(4)); tiledlayout(2,1);
nexttile; semilogx(f,r.frf.coherence,'LineWidth',1.5); yline(.9,'r--');
ylim([0 1.03]); ylabel('Ensemble MSC'); grid on;
nexttile; semilogx(f,r.frf.validTrials,'LineWidth',1.5); ylim([0 n+1]);
ylabel('Valid repetitions'); xlabel('Frequency [Hz]'); grid on;
sgtitle('4: Local chirp-regression coherence (not Welch)');
figure(fig(5)); tiledlayout(3,1);
for j=1:2
    nexttile; semilogx(f,viewH([E r.modelLocal],j),'LineWidth',1.3);
    axislabel(j); grid on; legend('Experiment H1','A','B FRF-fit','B time-fit','Location','best');
end
nexttile; hold on;
colors=get(gca,'ColorOrder');
for m=1:3
    semilogx(f,angle(r.modelLocal(:,m)./E)*180/pi,'LineWidth',1.3,'Color',colors(m+1,:));
end
set(gca,'XScale','log'); legend('A','B FRF-fit','B time-fit','Location','best');
ylabel('Model - data phase [deg]'); xlabel('Frequency [Hz]'); grid on;
sgtitle('5: Same-estimator delayed model comparison');
figure(fig(6)); tiledlayout(2,2);
ix=[1 find(r.split.validation,1)]; if numel(ix)==1, ix=[1 n]; end
for k=1:2
    i=ix(k); d=r.trials{i}.d;
    for j=1:2
        nexttile((j-1)*2+k);
        obs={d.x,d.v}; plot(d.t,obs{j},'k'); hold on;
        for m=1:3, plot(d.t,r.predictions{i,2,m}(:,j)); end
        title(sprintf('Trial %d, training-fixed parameters',i));
        ylabel(labels{j+1}); xlabel('Time [s]'); grid on;
    end
end
legend('Observed','A','B FRF-fit','B time-fit'); sgtitle('6: Free predictions using training-only parameters');
figure(fig(7)); tiledlayout(2,3);
fields={'angleFit_percent','angleRMSE_rad','velocityRMSE_rad_s','gainRMSE_dB','phaseRMSE_deg'};
for k=1:5
    nexttile; hold on;
    for m=["A","B_FRF","B_time"]
        mask=r.metrics.scope=="training-fixed"&r.metrics.model==m;
        plot(r.metrics.trial(mask),r.metrics.(fields{k})(mask),'o-');
    end
    if any(r.split.validation), xline(find(r.split.validation,1)-.5,'k--','Holdout'); end
    ylabel(strrep(fields{k},'_',' ')); xlabel('Trial'); grid on;
end
legend('A','B FRF-fit','B time-fit'); sgtitle('7: Model performance, no holdout refitting');
figure(fig(8)); tiledlayout(2,2);
dp=r.delay.phaseByTrial; td=dp.delay_s(dp.low_Hz==30 & dp.high_Hz==100)*1000;
nexttile; plot(1:n,td,'o-'); ylabel('30-100 Hz effective delay [ms]'); xlabel('Trial'); grid on;
nexttile; scatter(r.individual.q0_rad*180/pi,td,45,1:n,'filled'); xlabel('Reference [deg]'); ylabel('Delay [ms]'); grid on;
nexttile; scatter(r.individual.q0_rad*180/pi,r.individual.J,45,1:n,'filled'); xlabel('Reference [deg]'); ylabel('J'); grid on;
nexttile; scatter(r.individual.q0_rad*180/pi,r.individual.B,45,1:n,'filled'); xlabel('Reference [deg]'); ylabel('B'); grid on;
sgtitle('8: Trial reproducibility and initial position (color = trial index)');
for k=1:8
    savefig(fig(k),fullfile(r.output,sprintf('figure_%d.fig',k)));
    exportgraphics(fig(k),fullfile(r.output,sprintf('figure_%d.png',k)),'Resolution',160);
end
end
function a=viewH(H,j)
if j==1, a=20*log10(abs(H)); else, a=unwrap(angle(H),[],1)*180/pi; end
end
function axislabel(j)
if j==1, ylabel('Gain [dB rad/Nm]'); else, ylabel('Phase [deg]'); end
end
