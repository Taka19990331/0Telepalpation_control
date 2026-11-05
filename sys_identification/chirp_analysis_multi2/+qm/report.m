function report(r)
fid=fopen(fullfile(r.output,'report.txt'),'w'); cleanup=onCleanup(@()fclose(fid));
fprintf(fid,'QDD multi-trial analysis | MATLAB %s\n',r.matlabVersion);
fprintf(fid,'Selected %d; excluded %d. Reference pair condition differences: %d\n', ...
    sum(r.selection.selected),sum(~r.selection.selected),numel(r.referenceCheck));
fprintf(fid,'Chronological split: last ceil(N/4) trials held out; singleton has no holdout.\n');
fprintf(fid,'All-fit parameters are descriptive; only training-fixed/validation rows measure generalization.\n');
fprintf(fid,'Objective = mean across trials of angle MSE / within-trial angle variance.\n');
fprintf(fid,'Initial states are measured separately at PD activation, not optimized.\n');
fprintf(fid,'ZOH uses actual timestamps; local regression phase uses firmware nominal sample index.\n');
fprintf(fid,'Model B = delayed zero-state response + unshifted homogeneous initial response.\n');
fprintf(fid,'Fractional delay is evaluated by exact partial ZOH state propagation, no integer rounding.\n');
fprintf(fid,'Delay bounds: [%g, %g] s; grid includes seed %g s. J/B fixed during delay estimation.\n',r.options.delayBounds,r.options.delaySeed);
fprintf(fid,'Model C not implemented: command-path delay is a different feedback hypothesis from B.\n');
fprintf(fid,'MSC = |mean(conj(U).*Y)|^2/(mean(|U|^2)*mean(|Y|^2)), independent trial coefficients.\n');
fprintf(fid,'This is local chirp-regression ensemble coherence, NOT stationary Welch MSC.\n');
fprintf(fid,'Only numerically unestimable coefficients are NaN; no coherence-based exclusion/refit.\n');
fprintf(fid,'N<2 gives unavailable coherence. High coherence is not evidence of linearity.\n');
fprintf(fid,'Hmean is complex arithmetic mean; H1 is cross/input autospectrum. Both exported.\n');
fprintf(fid,'Model FRFs use the same estimator, windows and phase as data; analytic FRFs shown separately.\n');
fprintf(fid,'Edge/under-two-cycle windows retained with diagnostics; high-frequency encoder quantization matters.\n');
fprintf(fid,'Command torque gain is uncalibrated: J/B are conditional equivalent parameters.\n');
fprintf(fid,'Effective delay does not locate communication, driver, sensing or mechanical causes.\n');
fprintf(fid,'Full-band phase slope is signed diagnostic; model delay constrained nonnegative.\n');
fprintf(fid,'Position associations are descriptive and confounded with acquisition order; no causal claim.\n\n');
printTable(fid,'Selected files / split',r.split);
printTable(fid,'Parameters (J kg*m^2, B Nm*s/rad)',r.parameters);
printTable(fid,'Trial statistics',r.statistics);
fprintf(fid,'\nMultistart stability (solutions within 1%% of best converged cost):\n');
sets=[r.starts;{r.pooledStarts};{r.trainingStarts}];
for k=1:numel(sets)
    s=sets{k}; ok=s.exitflag>0;
    best=min(s.cost(ok)); near=ok & s.cost<=best*1.01+1e-10;
    fprintf(fid,'set %d: converged %d/%d; near-optimum J [%.8g, %.8g], B [%.8g, %.8g]\n', ...
        k,sum(ok),height(s),min(s.J(near)),max(s.J(near)),min(s.B(near)),max(s.B(near)));
end
printTable(fid,'Pooled phase slope diagnostics',r.delay.phasePooled);
td=r.delay.phaseByTrial;
td=td.delay_s(td.low_Hz==30 & td.high_Hz==100);
fprintf(fid,'Trial 30-100 Hz effective delay: mean %.6g ms; SD %.6g ms; range %.6g to %.6g ms\n', ...
    1e3*mean(td,'omitnan'),1e3*std(td,'omitnan'),1e3*min(td),1e3*max(td));
fprintf(fid,'Coherence: minimum %.6g; median %.6g; grid points below .9: %d/%d\n', ...
    min(r.frf.coherence),median(r.frf.coherence,'omitnan'),sum(r.frf.coherence<.9),height(r.frf));
printTable(fid,'Below .9 (grid endpoints, no interpolated crossing)',r.lowCoherenceBands);
printTable(fid,'Initial position associations',r.positionAssociation);
printTable(fid,'Model metrics by trial and evaluation scope',r.metrics);
fprintf(fid,'\nHeld-out mean metrics (parameters fixed from training):\n');
for m=["A","B_FRF","B_time"]
    ix=r.metrics.scope=="training-fixed"&r.metrics.subset=="validation"&r.metrics.model==m;
    if any(ix)
        a=mean(r.metrics{ix,{'angleFit_percent','angleRMSE_rad','velocityRMSE_rad_s','gainRMSE_dB','phaseRMSE_deg'}},1);
        fprintf(fid,'%s: Fit %.5g %%; angle RMSE %.6g rad; velocity RMSE %.6g rad/s; gain RMSE %.5g dB; phase RMSE %.5g deg\n',m,a);
    else
        fprintf(fid,'%s: unavailable (no held-out trials)\n',m);
    end
end
fprintf(fid,'CSV lacks an independent wall-clock timestamp; chronology is filename-derived and cannot be independently confirmed.\n');
fprintf(fid,'Inspect multistart CSVs for convergence and parameter spread; a B=0 boundary does not establish zero friction.\n');
fprintf(fid,'Synthetic tests verify algorithms; they do not establish physical identifiability.\n');
end
function printTable(fid,title,T)
fprintf(fid,'\n%s\n%s\n',title,evalc('disp(T)'));
end
