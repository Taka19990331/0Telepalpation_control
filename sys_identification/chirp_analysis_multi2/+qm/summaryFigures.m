function summaryFigures(s)
% Create and display only the two summary figures.
f=s.frequency_Hz; E=s.experimentalH1;
fig1=figure('Visible','on','Color','w','Position',[100 60 1100 1050]);
tl=tiledlayout(fig1,3,1,'TileSpacing','compact','Padding','compact');
colors=[0 0.4470 0.7410; 0.8500 0.3250 0.0980; 0 0 0];
markerIdx=unique(round(linspace(1,numel(f),14)));
ax1=nexttile(tl); hold(ax1,'on');
hA=semilogx(ax1,f,20*log10(abs(s.modelA)),'--','Color',colors(1,:), ...
    'LineWidth',1.8);
hB=semilogx(ax1,f,20*log10(abs(s.modelB_FRF)),'-.','Color',colors(2,:), ...
    'LineWidth',2.0);
hE=semilogx(ax1,f,20*log10(abs(E)),'-o','Color',colors(3,:), ...
    'LineWidth',1.3,'MarkerIndices',markerIdx,'MarkerSize',5, ...
    'MarkerFaceColor','w');
ylabel(ax1,'Gain [dB]'); title(ax1,'Gain'); grid(ax1,'on'); ax1.XMinorGrid='on';
legend(ax1,[hE hA hB],{'Experiment','Model A (no delay)', ...
    'Model B (with delay, FRF fit)'},'Location','best');

ax2=nexttile(tl); hold(ax2,'on');
semilogx(ax2,f,unwrap(angle(s.modelA))*180/pi,'--','Color',colors(1,:), ...
    'LineWidth',1.8);
semilogx(ax2,f,unwrap(angle(s.modelB_FRF))*180/pi,'-.','Color',colors(2,:), ...
    'LineWidth',2.0);
semilogx(ax2,f,unwrap(angle(E))*180/pi,'-o','Color',colors(3,:), ...
    'LineWidth',1.3,'MarkerIndices',markerIdx,'MarkerSize',5, ...
    'MarkerFaceColor','w');
ylabel(ax2,'Phase [deg]'); title(ax2,'Phase'); grid(ax2,'on'); ax2.XMinorGrid='on';

ax3=nexttile(tl); hold(ax3,'on');
semilogx(ax3,f,s.coherence,'-','Color',[0.20 0.55 0.30],'LineWidth',1.8);
ylabel(ax3,'Coherence'); xlabel(ax3,'Frequency [Hz]');
title(ax3,'Coherence'); grid(ax3,'on'); ax3.XMinorGrid='on';
cohMin=min(s.coherence,[],'omitnan');
cohPad=max(1e-4,0.15*(1-cohMin));
ylim(ax3,[max(0,cohMin-cohPad) 1.00002]);
linkaxes([ax1 ax2 ax3],'x');
sgtitle(tl,'Experimental and Identified Frequency Responses');
savefig(fig1,fullfile(s.output,'figure_gain_phase_coherence.fig'));
exportgraphics(fig1,fullfile(s.output,'figure_gain_phase_coherence.png'),'Resolution',180);

n=numel(s.trialNames); colors=lines(n);
fig2=figure('Visible','on','Color','w','Position',[120 80 1300 900]);
tl2=tiledlayout(fig2,2,1,'TileSpacing','compact','Padding','compact');
ax21=nexttile(tl2); hold(ax21,'on');
h=gobjects(n,1);
for k=1:n
    h(k)=semilogx(ax21,f,s.phaseDelayByTrial_s(:,k)*1000, ...
        'Color',colors(k,:),'LineWidth',1.3);
end
ylabel(ax21,'Phase Delay [ms]'); title(ax21,'Trial-wise Phase Delay vs Frequency');
xlabel(ax21,'Frequency [Hz]'); grid(ax21,'on'); ax21.XMinorGrid='on';
legend(ax21,h,cellstr(s.trialNames),'Location','eastoutside', ...
    'Interpreter','none');

ax22=nexttile(tl2); hold(ax22,'on');
x=s.bandStats.trial;
errorbar(ax22,x,s.bandStats.meanPhaseDelay_ms,s.bandStats.stdPhaseDelay_ms, ...
    'o-','Color',[0.10 0.30 0.65],'MarkerFaceColor','w', ...
    'LineWidth',1.5,'CapSize',8);
xlim(ax22,[0 n+1]); xticks(ax22,x);
ylabel(ax22,'Mean Phase Delay [ms]'); xlabel(ax22,'Trial');
title(ax22,'Mean +/- SD Phase Delay in 30–100 Hz Band');
grid(ax22,'on'); ax22.XMinorGrid='off';
sgtitle(tl2,'Trial-wise Delay Analysis');
savefig(fig2,fullfile(s.output,'figure_trial_phase_delay.fig'));
exportgraphics(fig2,fullfile(s.output,'figure_trial_phase_delay.png'),'Resolution',180);
end
