%% Verification of the RFOB analysis environment (no production logic here)
% Run from MATLAB after adding this directory to the path.
root=fileparts(mfilename('fullpath')); addpath(root);
cfg=struct('visible','off','savePNG',true,'showFFT',true);
r=rfob_oscillation_analysis(cfg);
assert(r.friction{3}.reconstruction.recursiveRMS_Nm<1e-4,'Observer reconstruction mismatch.');
assert(r.replay.baselineCommandErrorRMS_Nm<1e-5,'Replay baseline mismatch.');
assert(all(r.replay.detector.gamma>=0 & r.replay.detector.gamma<=1));
assert(all(abs(r.replay.command)<=r.cfg.sim.limit_Nm,'all'));
assert(r.replay.summary.extraPower_W(3)<0,'Damping injects average energy in the measured replay.');
assert(abs(r.validation.measured.peakHz-12)<1,'Measured peak not near 12 Hz.');
assert(r.validation.baseline.bandRMS<r.cfg.validation.minimumRMS_rad_s && ~r.validation.reproduced, ...
    'Default stick/slip model must not be reported as validated.');
assert(all(r.transparencySummary.lowFrequencyMobility_rad_s_Nm>0),'Voluntary input must move plant.');
for k=1:numel(r.closed)
    assert(all(isfinite(r.closed{k}.w)));
    assert(all(abs(r.closed{k}.command)<=r.cfg.sim.limit_Nm));
end
% Physical limits: without motor feedback and hand force, dry friction dissipates energy.
c=struct; c.figures=false(1,10); c.saveResults=false;
c.RUN_DATA_ANALYSIS=false; c.RUN_SPECTRAL_ANALYSIS=false;
c.RUN_AMPLITUDE_DETECTION=false; c.RUN_FRICTION_COMPARISON=false; c.RUN_REPLAY_ANALYSIS=false;
c.controller.mode='none'; c.sim.Kr=0; c.sim.Kf=0;
c.sim.Kh=0; c.sim.Dh=0; c.sim.initialVelocity_rad_s=1;
c.sim.duration_s=2; c.sim.discard_s=.5;
p=rfob_oscillation_analysis(c);
assert(all(diff(abs(p.closed{1}.w))<=1e-12),'Unforced friction creates energy.');
% Small sweep and robustness run exercises the optional branches.
c.RUN_CLOSED_LOOP_SIM=false; c.RUN_PARAMETER_SWEEP=true; c.RUN_ROBUSTNESS_ANALYSIS=true;
c.controller.mode='damping'; c.sim.Kr=.75; c.sim.Kh=66.4; c.sim.Dh=.05;
c.sweep.values={ [.05 .1], [.08 .1] }; c.figures(10)=true; c.visible='off';
c.robustness.Kh=[60 70]; c.robustness.Dh=.05; c.robustness.delay_s=.001477;
s=rfob_oscillation_analysis(c);
assert(isequal(size(s.sweep.rms),[2 2]) && height(s.robustness)==6);
% Numerical convergence of the stick/slip trajectory at finer plant substeps.
c.RUN_PARAMETER_SWEEP=false; c.RUN_ROBUSTNESS_ANALYSIS=false; c.RUN_CLOSED_LOOP_SIM=true;
c.figures(:)=false; c.sim.Kr=0; c.sim.Kh=0; c.sim.Dh=0; c.controller.mode='none'; c.sim.substeps=20;
p2=rfob_oscillation_analysis(c);
assert(max(abs(p.closed{1}.w-p2.closed{1}.w))<.01,'Plant substep convergence failed.');
% Invalid threshold combinations must fail clearly.
try
    rfob_oscillation_analysis(struct('controller',struct('on_rad_s',.3,'full_rad_s',.2)));
    error('test:NoValidation','Invalid thresholds were accepted.');
catch e
    assert(~strcmp(e.identifier,'test:NoValidation'));
end
close all;
fprintf('\nALL RFOB VERIFICATION CHECKS PASSED.\n');
