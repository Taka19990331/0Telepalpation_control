% Diagnostic only: does not change the identified J/B or refit the model.
analysisRoot = fileparts(mfilename('fullpath'));
dataName = 'chirp_20261020_101838_880284';
resultDir = fullfile(analysisRoot,'results',dataName);
F = readtable(fullfile(resultDir,'frf.csv'));
S = load(fullfile(resultDir,'analysis.mat'),'r');
c = S.r.conditions;
f = F.frequency_Hz(:);
Hexp = F.real_rad_per_Nm(:) + 1i*F.imag_rad_per_Nm(:);
Hlocal = F.modelLocalReal(:) + 1i*F.modelLocalImag(:);
phiExp = unwrap(angle(Hexp))*180/pi;
phiLocal = unwrap(angle(Hlocal))*180/pi;
delta = phiExp-phiLocal;                 % negative means experiment lags model
gainError = 20*log10(abs(Hexp)./abs(Hlocal));
status = string(F.windowStatus);
fullWindow = status=="eligible" & isfinite(delta) & isfinite(gainError);
edgeWindow = status=="edge-truncated";
bands = [1 10;10 30;30 60;60 100];
bandNames = ["1-10 Hz";"10-30 Hz";"30-60 Hz";"60-100 Hz"];
rows = cell(size(bands,1),8);
for k=1:size(bands,1)
    ix = fullWindow & f>=bands(k,1) & f<bands(k,2);
    rows{k,1}=bandNames(k); rows{k,2}=sum(ix);
    if sum(ix)>=3
        [rows{k,3},rows{k,4},rows{k,5},rows{k,6},rows{k,7},rows{k,8}] = fitDelay(f(ix),delta(ix));
    else
        [rows{k,3:8}] = deal(NaN);
    end
end
bandTable=cell2table(rows,'VariableNames',{'band','fullWindowPoints', ...
    'zeroInterceptDelay_ms','linearFitDelay_ms','linearIntercept_deg', ...
    'linearFitR2','zeroInterceptR2','linearFitRMSE_deg'});
ix=fullWindow;
[tau0ms,taulinms,b0,r2lin,r20,rmse] = fitDelay(f(ix),delta(ix));
linearCoefficients=polyfit(f(ix),delta(ix),1);
linearPrediction=polyval(linearCoefficients,f(ix));
fitResidual=delta(ix)-linearPrediction;
residualSlope=polyfit(f(ix),fitResidual,1);
% AK80 standard MIT response decode: 16-bit unsigned position, [-12.5,+12.5] rad.
positionStepRad=25/(2^16-1);
positionStepDeg=positionStepRad*180/pi;
target=95;
[~,i95]=min(abs(f-target));
amp95=c.A0+(c.A1-c.A0)*((f(i95)-c.f0)/(c.f1-c.f0))^c.a;
anglePeak95=abs(Hexp(i95))*amp95;
anglePeak95Deg=anglePeak95*180/pi;
peakCodes95=anglePeak95/positionStepRad;
gainError95=gainError(i95);
phaseError95=delta(i95);
pointDelay95=-phaseError95/(360*f(i95));
fprintf('\nPhase delay diagnostic (no J/B refit)\n');
fprintf('Experiment-minus-same-estimator-model phase is negative for extra lag.\n');
fprintf('Full windows: %d/%d; edge-truncated displayed separately: %d; under-2-cycle: %d.\n', ...
    sum(fullWindow),numel(f),sum(edgeWindow),sum(status=="under-2-cycles"));
fprintf('Full-band through-origin delay %.5f ms; linear fit delay %.5f ms; intercept %.4f deg; R2 %.5f; RMSE %.4f deg.\n', ...
    tau0ms,taulinms,b0,r2lin,rmse);
fprintf('95-Hz nearest bin %.5f Hz: exp phase %.3f deg, local-model phase %.3f deg, delta %.3f deg, gain error %.4f dB, pointwise delay %.4f ms.\n', ...
    f(i95),phiExp(i95),phiLocal(i95),phaseError95,gainError95,1000*pointDelay95);
fprintf('AK80 position code step %.9g rad = %.9g deg; estimated 95-Hz angle peak %.6g deg = %.3f codes.\n', ...
    positionStepRad,positionStepDeg,anglePeak95Deg,peakCodes95);
disp(bandTable);

diagnostic=table(f,phiExp,phiLocal,delta,-delta./(360*f)*1000, ...
    F.gain_dB,20*log10(abs(Hlocal)),gainError,status, ...
    'VariableNames',{'frequency_Hz','experimentPhase_deg','sameEstimatorModelPhase_deg', ...
    'experimentMinusModelPhase_deg','pointwiseEquivalentDelay_ms','experimentGain_dB', ...
    'sameEstimatorModelGain_dB','experimentMinusModelGain_dB','windowStatus'});
writetable(diagnostic,fullfile(resultDir,'phase_delay_by_frequency.csv'));
writetable(bandTable,fullfile(resultDir,'phase_delay_by_band.csv'));

fig=figure('Name','Phase delay diagnostic','Color','w','Visible','off');
tiledlayout(3,1,'TileSpacing','compact');
nexttile;
semilogx(f,delta,'ko','MarkerSize',4,'DisplayName','Experiment - same-estimator model'); hold on;
semilogx(f,polyval(linearCoefficients,f),'b-','LineWidth',1.2,'DisplayName','OLS phase-vs-frequency');
semilogx(f,-360*f*tau0ms/1000,'r--','LineWidth',1.2,'DisplayName','Constant-delay fit through zero');
semilogx(f(edgeWindow),delta(edgeWindow),'s','Color',[0.85 0.5 0.1],'DisplayName','Edge-truncated window');
xline(f(i95),':','95-Hz check','HandleVisibility','off'); yline(0,':','HandleVisibility','off');
grid on; ylabel('Phase difference [deg]'); legend('Location','best');
title(sprintf('Full-band delay fit %.3f ms; linear fit R^2 %.3f, intercept %.2f deg',tau0ms,r2lin,b0));
nexttile;
semilogx(f,-delta./(360*f)*1000,'ko-','MarkerSize',3); hold on;
semilogx(f(edgeWindow),-delta(edgeWindow)./(360*f(edgeWindow))*1000,'s', ...
    'Color',[0.85 0.5 0.1],'DisplayName','Edge-truncated');
yline(tau0ms,'r--',sprintf('Full-band fit %.3f ms',tau0ms));
grid on; ylabel('Equivalent delay [ms]'); legend('Pointwise phase conversion','Edge-truncated','Location','best');
nexttile;
semilogx(f,gainError,'k-','LineWidth',1.1); hold on;
plot(f(i95),gainError95,'ro','MarkerFaceColor','r');
xline(f(i95),':','HandleVisibility','off'); yline(0,':','HandleVisibility','off');
text(55,0.50,sprintf('95.455 Hz: %+.4f dB',gainError95), ...
    'VerticalAlignment','top','HorizontalAlignment','left','BackgroundColor','w');
grid on; ylabel('Gain error [dB]'); xlabel('Frequency [Hz]');
title('Experiment gain / same-estimator model gain');
savefig(fig,fullfile(resultDir,'phase_delay_diagnostic.fig'));
exportgraphics(fig,fullfile(resultDir,'phase_delay_diagnostic.png'),'Resolution',160);
close(fig);

report=fullfile(resultDir,'phase_delay_diagnostic.txt');
fid=fopen(report,'w','n','UTF-8'); assert(fid>=0);
cleanup=onCleanup(@()fclose(fid)); %#ok<NASGU>
fprintf(fid,'Diagnostic only; original J/B and model are unchanged.\n');
fprintf(fid,'Phase difference = experiment FRF phase minus same-local-estimator model phase; negative means additional experimental lag.\n');
fprintf(fid,'Plots and CSV include the full 1-100 Hz grid; regression uses complete windows only. Edge-truncated values remain plotted and tabulated, but are excluded from delay regression.\n');
fprintf(fid,'Full complete-window range %.5g-%.5g Hz (%d points). Full-band delay through zero %.6g ms.\n', ...
    min(f(ix)),max(f(ix)),sum(ix),tau0ms);
fprintf(fid,'Unconstrained linear fit: delay from slope %.6g ms, intercept %.6g deg, R2 %.6g, RMSE %.6g deg.\n', ...
    taulinms,b0,r2lin,rmse);
fprintf(fid,'95-Hz nearest bin %.8g Hz: phase difference %+.6g deg; gain difference %+.6g dB.\n', ...
    f(i95),phaseError95,gainError95);
fprintf(fid,'95-Hz pointwise phase-to-delay conversion %.6g ms is not a directly identified physical delay.\n',1000*pointDelay95);
fprintf(fid,'AK80 position decode step 25/(2^16-1) rad = %.12g rad = %.12g deg.\n', ...
    positionStepRad,positionStepDeg);
fprintf(fid,'95-Hz output angle estimated from measured FRF and recorded chirp amplitude: %.9g deg peak = %.6g position codes.\n', ...
    anglePeak95Deg,peakCodes95);
fprintf(fid,'A pure delay has unity magnitude; a first-order lag normally adds frequency-dependent phase lag together with magnitude roll-off. Similar gain and a roughly linear phase trend are consistent with delay, but do not prove it.\n');
fprintf(fid,'Band fits use full windows and are listed in phase_delay_by_band.csv. Edge windows are retained for display but not used in those fits.\n');
fprintf(fid,'This phase-derived equivalent delay is distinct from actually identifying and validating a delay in the plant model.\n');
fprintf('Saved phase diagnostic to %s\n',resultDir);

function [tau0ms,taulinms,b0,r2lin,r20,rmse] = fitDelay(f,delta)
f=f(:); delta=delta(:);
tau0=-sum(f.*delta)/(360*sum(f.^2));
tau0ms=1000*tau0;
p=polyfit(f,delta,1);
taulinms=-1000*p(1)/360; b0=p(2);
pred=polyval(p,f); resid=delta-pred;
r2lin=1-sum(resid.^2)/max(sum((delta-mean(delta)).^2),eps);
pred0=-360*f*tau0;
r20=1-sum((delta-pred0).^2)/max(sum((delta-mean(delta)).^2),eps);
rmse=sqrt(mean(resid.^2));
end
