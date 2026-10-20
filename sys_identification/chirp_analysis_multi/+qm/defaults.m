function o = defaults(o)
root=fileparts(fileparts(mfilename('fullpath')));
d=struct('captureDir',fullfile(fileparts(root),'captures'), ...
    'outputRoot',fullfile(root,'results'),'files',strings(0,1), ...
    'makeFigures',true,'frfPoints',100,'frfCycles',4,'maxIterations',350, ...
    'delayBounds',[0 0.005],'delaySeed',0.00145);
for k=string(fieldnames(d))'
    if ~isfield(o,k), o.(k)=d.(k); end
end
end
