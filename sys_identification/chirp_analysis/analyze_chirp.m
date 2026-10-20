% Offline QDD identification. Change only dataName to analyze another capture.
dataName = 'chirp_20261020_101838_880284';
analysisDir = fileparts(mfilename('fullpath'));
addpath(analysisDir);
options = struct;
options.makeFigures = true;
options.saveResults = true;
% Algorithm settings only; experiment settings always come from the CSV.
options.frfCycles = 4;
options.frfMaxRelativeBandwidth = 0.35;
options.frfPoints = 100;
options.maxIterations = 500;
results = qdd_chirp_analysis(dataName, options);
