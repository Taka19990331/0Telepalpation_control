% Run this script in MATLAB R2026a. All outputs stay beside this script.
root = fileparts(mfilename('fullpath'));
addpath(root);
options = struct;
options.files = strings(0,1); % Optional subset of eligible CSV basenames.
options.makeFigures = true;
results = qdd_chirp_multi(options);
