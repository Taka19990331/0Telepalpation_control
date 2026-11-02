function diagnosis=run_baseline_diagnosis
% Baseline only. Existing replay/measured analysis and thresholds are unchanged.
root=fileparts(mfilename('fullpath')); addpath(root);
c=struct('baselineOnly',true,'visible','off','saveResults',false,'figures',false(1,10));
c.RUN_DATA_ANALYSIS=false; c.RUN_SPECTRAL_ANALYSIS=false;
c.RUN_AMPLITUDE_DETECTION=false; c.RUN_FRICTION_COMPARISON=false; c.RUN_REPLAY_ANALYSIS=false;
% Capture warnings in each run, retaining the full transcript for audit.
transcript=evalc('ideal=rfob_oscillation_analysis(c);');
diagnosis.ideal=ideal.closed{1}; diagnosis.measured=ideal.validation.measured;
c.noise.enabled=true;
transcript=[transcript evalc('noisy=rfob_oscillation_analysis(c);')];
diagnosis.noisy=noisy.closed{1}; diagnosis.cfg=noisy.cfg;
diagnosis.idleRMS=sqrt(mean(noisy.cfg.noise.samples.^2,1));
rows={}; n=0;
% Cartesian impedance and friction scale; nominal friction shape retained.
for enabled=[false true]
    seeds=17; if enabled, seeds=[17 31 53]; end
    for seed=seeds
        for K=[30 66.4 110]
            for D=[.01 .05 .2]
                for scale=[.5 1 1.5]
                    cc=c; cc.noise.enabled=enabled; cc.noise.seed=seed;
                    cc.sim.Kh=K; cc.sim.Dh=D; cc.sim.plantFrictionScale=scale;
                    transcript=[transcript evalc('r=rfob_oscillation_analysis(cc);')]; %#ok<AGROW>
                    n=n+1; rows{n}=assess(r,enabled,seed,K,D,scale,'nominal'); %#ok<AGROW>
                end
            end
        end
    end
end
% Plant-only shape sensitivity: equal static/kinetic friction and velocity width.
for shape={'coulomb','narrow','wide'}
    for seed=[17 31 53]
        cc=c; cc.noise.seed=seed;
        p=noisy.cfg.sim.plantPositive; q=noisy.cfg.sim.plantNegative;
        switch shape{1}
            case 'coulomb', p(2)=p(1); q(2)=q(1);
            case 'narrow', p(3)=p(3)*.5; q(3)=q(3)*.5;
            case 'wide', p(3)=p(3)*2; q(3)=q(3)*2;
        end
        cc.sim.plantPositive=p; cc.sim.plantNegative=q;
        transcript=[transcript evalc('r=rfob_oscillation_analysis(cc);')]; %#ok<AGROW>
        n=n+1; rows{n}=assess(r,true,seed,66.4,.05,1,shape{1}); %#ok<AGROW>
    end
end
diagnosis.sensitivity=struct2table(vertcat(rows{:}));
diagnosis.baselines=struct2table([assess(ideal,false,17,66.4,.05,1,'nominal'); assess(noisy,true,17,66.4,.05,1,'nominal')]);
diagnosis.closedLoopValidated=false;
diagnosis.note='Idle fluctuations may include physical motion; paired moving-block bootstrap is not an identified sensor-only model. Screening is not independent validation. No periodic external force.';
out=fullfile(root,'baseline_diagnosis'); if ~exist(out,'dir'), mkdir(out); end
save(fullfile(out,'diagnosis.mat'),'diagnosis','-v7.3');
writetable(diagnosis.sensitivity,fullfile(out,'sensitivity.csv'));
writetable(diagnosis.baselines,fullfile(out,'baselines.csv'));
fid=fopen(fullfile(out,'run_log.txt'),'w'); fprintf(fid,'%s',transcript); fclose(fid);
fig=figure('Visible','off','Color','w','Position',[50 50 1200 1000]);
tiledlayout(5,2); fields={'w','theta','external','observerFriction','applied'};
labels={'Velocity (rad/s)','Position (rad)','External estimate (Nm)','Friction (Nm)','Motor torque (Nm)'};
for j=1:5
    for k=1:2
        if k==1, a=diagnosis.ideal; else, a=diagnosis.noisy; end
        nexttile; ix=a.t<=1; plot(a.t(ix),a.(fields{j})(ix)); hold on;
        if j==1, plot(a.t(ix),a.measuredVelocity(ix)); legend('True','Measured'); end
        if j==4, plot(a.t(ix),a.plantFriction(ix)); legend('Observer','Plant resisting'); end
        if j==5, plot(a.t(ix),a.command(ix)); legend('Applied','Command'); end
        grid on; xlim([0 1]); ylabel(labels{j});
        if j==1, title(sprintf('Traditional RFOB: idle noise %d',k-1)); end
        if j==5, xlabel('Time (s)'); end
    end
end
sgtitle('Baseline diagnosis: first second, no suppression controller');
exportgraphics(fig,fullfile(out,'startup.png'),'Resolution',150); close(fig);
fig=figure('Visible','off','Color','w','Position',[50 50 1100 900]); tiledlayout(3,1);
nexttile; plot(diagnosis.ideal.t,diagnosis.ideal.measuredVelocity); hold on;
plot(diagnosis.noisy.t,diagnosis.noisy.measuredVelocity); legend('Ideal','Idle noise');
ylabel('Velocity (rad/s)'); xlabel('Time (s)'); grid on;
T=diagnosis.sensitivity; nexttile; scatter(T.peakHz,T.bandRMS,22,T.noiseEnabled,'filled'); hold on;
plot(diagnosis.measured.peakHz,diagnosis.measured.bandRMS,'rp','MarkerSize',14);
xlabel('Peak frequency (Hz)'); ylabel('8-20 Hz RMS (rad/s)'); grid on;
nexttile; scatter(T.phaseError_deg,T.sustainedRatio,22,T.noiseEnabled,'filled');
xlabel('RFOB / velocity phase error vs measured (deg)'); ylabel('Late / early RMS'); grid on;
sgtitle('Baseline sensitivity: closed-loop predictions NOT validated');
exportgraphics(fig,fullfile(out,'sensitivity.png'),'Resolution',150); close(fig);
disp(diagnosis.baselines); fprintf('Screening passed: %d / %d (not validation)\n',sum(T.screeningPassed),height(T));
end

function row=assess(r,enabled,seed,K,D,scale,shape)
a=r.closed{1}; m=a.measurementMetrics; ref=r.validation.measured; v=r.cfg.validation;
row=m; row.trueBandRMS=a.metrics.bandRMS; row.rmsRatio=m.bandRMS/ref.bandRMS;
row.phaseError_deg=mod(m.rfobPhaseRelativeVelocity_deg-ref.rfobPhaseRelativeVelocity_deg+180,360)-180;
row.screeningPassed=abs(m.peakHz-ref.peakHz)<=v.frequencyToleranceHz && ...
    row.rmsRatio>=v.rmsRatioRange(1) && row.rmsRatio<=v.rmsRatioRange(2) && ...
    m.sustainedRatio>=v.sustainedRatioRange(1) && m.sustainedRatio<=v.sustainedRatioRange(2) && ...
    m.bandRMS>=v.minimumRMS_rad_s;
row.lastMotion_s=0; j=find(abs(a.w)>1e-6,1,'last'); if ~isempty(j), row.lastMotion_s=a.t(j); end
row.finalTheta_rad=a.theta(end); row.finalMotor_Nm=a.applied(end); row.finalHand_Nm=a.humanTorque(end);
row.noiseEnabled=enabled; row.seed=seed; row.Kh=K; row.Dh=D; row.frictionScale=scale; row.shape=shape;
end
