function verify_baseline_diagnosis
root=fileparts(mfilename('fullpath')); addpath(root);
load(fullfile(root,'baseline_diagnosis','diagnosis.mat'),'diagnosis');
d=diagnosis;
assert(~d.closedLoopValidated);
assert(all(d.ideal.measurementNoise==0,'all'));
assert(d.baselines.bandRMS(1)<.01 && ~d.baselines.screeningPassed(1));
assert(all(isfinite(d.sensitivity.bandRMS)) && height(d.sensitivity)==117);
assert(d.measured.peakHz>11 && d.measured.peakHz<13);
assert(abs(d.measured.bandRMS-.337)<.02);
% Same seed repeats exactly; more substeps must preserve noisy trajectory.
c=d.cfg; c.saveResults=false; c.figures(:)=false;
evalc('r=rfob_oscillation_analysis(c);');
assert(isequal(r.closed{1}.measurementNoise,d.noisy.measurementNoise));
assert(max(abs(r.closed{1}.w-d.noisy.w))<1e-12);
c.sim.substeps=20; evalc('fine=rfob_oscillation_analysis(c);');
fprintf('Noisy substep convergence max speed difference: %.6g rad/s\n',max(abs(fine.closed{1}.w-d.noisy.w)));
assert(abs(fine.closed{1}.measurementMetrics.bandRMS/d.noisy.measurementMetrics.bandRMS-1)<.01);
assert(abs(fine.closed{1}.metrics.bandRMS/d.noisy.metrics.bandRMS-1)<.01);
assert(isequal(d.cfg.validation.rmsRatioRange,[.5 2]) && d.cfg.validation.frequencyToleranceHz==2);
assert(isequal(d.cfg.validation.sustainedRatioRange,[.5 2]) && d.cfg.validation.minimumRMS_rad_s==.01);
% Idle fluctuation resampling must not alter the caller's random stream.
before=rng; evalc('rfob_oscillation_analysis(c);'); assert(isequal(before,rng));
files={'rfob_oscillation_analysis.m','run_baseline_diagnosis.m','verify_baseline_diagnosis.m'};
for k=1:numel(files), disp(checkcode(fullfile(root,files{k}),'-id')); end
fprintf('BASELINE DIAGNOSIS CHECKS PASSED.\n');
end
