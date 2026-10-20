function r=qdd_chirp_multi(o)
%QDD_CHIRP_MULTI Reproducible independent-trial QDD identification, base MATLAB.
if nargin<1, o=struct; end
o=qm.defaults(o);
root=fileparts(mfilename('fullpath'));
% No outputs may escape this project's folder.
dest=char(java.io.File(o.outputRoot).getCanonicalPath());
base=char(java.io.File(root).getCanonicalPath());
assert(startsWith(lower(dest),[lower(base) filesep]),'outputRoot must be inside chirp_analysis_multi');
out=fullfile(dest,char(datetime('now','Format','yyyyMMdd_HHmmss_SSS')));
while isfolder(out), out=[out '_new']; end
mkdir(out); fprintf('Output: %s\n',out);
[trials,selection,metadata,referenceCheck]=qm.select(o,out);
n=numel(trials); [train,test]=qm.split(n);
split=table((1:n)',string(cellfun(@(t)t.file,trials,'UniformOutput',false))', ...
    ismember((1:n)',train),ismember((1:n)',test), ...
    'VariableNames',{'trial','file','training','validation'});
writetable(split,fullfile(out,'split.csv'));
individual=table; starts=cell(n,1); residuals=cell(n,1);
for i=1:n
    fprintf('Individual fit %d/%d: %s\n',i,n,trials{i}.file);
    [p,starts{i}]=qm.fit(trials(i),o);
    y=qm.predict(p(1),p(2),0,trials{i}); d=trials{i}.d;
    residuals{i}=[d.x d.v]-y;
    row=table(i,trials{i}.file,p(1),p(2),trials{i}.c.q0,d.initial(1),d.initial(2), ...
        100*(1-norm(residuals{i}(:,1))/norm(d.x-mean(d.x))), ...
        sqrt(mean(residuals{i}(:,1).^2)),sqrt(mean(residuals{i}(:,2).^2)), ...
        'VariableNames',{'trial','file','J','B','q0_rad','x0_rad','v0_rad_s', ...
        'angleFit_percent','angleRMSE_rad','velocityRMSE_rad_s'});
    individual=[individual;row]; %#ok<AGROW>
    writetable(starts{i},fullfile(out,sprintf('multistart_trial_%02d.csv',i)));
end
writetable(individual,fullfile(out,'individual_parameters.csv'));
fprintf('All-trial common J/B fit\n');
[pooled,pooledStarts]=qm.fit(trials,o);
writetable(pooledStarts,fullfile(out,'multistart_pooled.csv'));
fprintf('All-trial effective-delay analysis\n');
delay=qm.delay(trials,pooled,o);
fprintf('Training-only J/B and delay fit (%d trials); holdout %d trials\n',numel(train),numel(test));
if isempty(test)
    training=pooled; trainingStarts=pooledStarts; trainingDelay=delay;
else
    [training,trainingStarts]=qm.fit(trials(train),o);
    trainingDelay=qm.delay(trials(train),training,o);
end
writetable(trainingStarts,fullfile(out,'multistart_training.csv'));
writetable(delay.phaseByTrial,fullfile(out,'delay_by_trial_band.csv'));
writetable(delay.phasePooled,fullfile(out,'delay_pooled_band.csv'));
writetable(delay.frfScan,fullfile(out,'delay_frf_scan.csv'));
writetable(delay.timeScan,fullfile(out,'delay_time_scan.csv'));
writetable(trainingDelay.frfScan,fullfile(out,'training_delay_frf_scan.csv'));
writetable(trainingDelay.timeScan,fullfile(out,'training_delay_time_scan.csv'));
params=table(["all";"training"],[pooled(1);training(1)],[pooled(2);training(2)], ...
    [delay.frf_s;trainingDelay.frf_s],[delay.time_s;trainingDelay.time_s], ...
    'VariableNames',{'scope','J','B','Td_FRF_s','Td_time_s'});
writetable(params,fullfile(out,'common_parameters.csv'));
stats=table(["J";"B"],mean(individual{:,{'J','B'}},1)',std(individual{:,{'J','B'}},0,1)', ...
    'VariableNames',{'parameter','mean','std'});
stats.CV=stats.std./abs(stats.mean);
writetable(stats,fullfile(out,'parameter_statistics.csv'));
r=struct('options',o,'output',out,'matlabVersion',version,'trials',{trials}, ...
    'selection',selection,'metadata',{metadata},'referenceCheck',referenceCheck, ...
    'split',split,'individual',individual,'starts',{starts},'residuals',{residuals}, ...
    'pooled',pooled,'pooledStarts',pooledStarts,'training',training, ...
    'trainingStarts',trainingStarts,'delay',delay,'trainingDelay',trainingDelay, ...
    'parameters',params,'statistics',stats);
r=qm.evaluate(r);
qm.report(r);
save(fullfile(out,'analysis.mat'),'r','-v7.3');
if o.makeFigures, qm.figures(r); end
fprintf('Finished. Results: %s\n',out);
end
