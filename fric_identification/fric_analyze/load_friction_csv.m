function [T, meta] = load_friction_csv(path)
%LOAD_FRICTION_CSV Read FRIC001 comments/config metadata and numeric samples.
fid = fopen(path, 'r');
if fid < 0, error('Cannot open input CSV: %s', path); end
cleanup = onCleanup(@() fclose(fid));
metadataLines = {};
header = '';
while true
    line = fgetl(fid);
    if ~ischar(line), error('CSV has no data header: %s', path); end
    if startsWith(strtrim(line), '#')
        metadataLines{end+1} = strtrim(line); %#ok<AGROW>
    else
        header = line;
        break
    end
end
meta = struct('lines', {metadataLines}, 'config', struct(), 'result', struct(), ...
    'states', '', 'reasons', '');
for k = 1:numel(metadataLines)
    line = metadataLines{k};
    if startsWith(line, '# config = ')
        meta.config = jsondecode(extractAfter(line, '# config = '));
    elseif startsWith(line, '# result = ')
        meta.result = jsondecode(extractAfter(line, '# result = '));
    elseif startsWith(line, '# states = ')
        meta.states = extractAfter(line, '# states = ');
    elseif startsWith(line, '# reasons = ')
        meta.reasons = extractAfter(line, '# reasons = ');
    end
end
frewind(fid);
while true
    line = fgetl(fid);
    if ~ischar(line), error('CSV header disappeared while reading: %s', path); end
    if ~startsWith(strtrim(line), '#'), header = line; break; end
end
names = strsplit(strtrim(header), ',');
opts = delimitedTextImportOptions('NumVariables', numel(names));
opts.DataLines = [numel(metadataLines)+2, Inf];
opts.Delimiter = ',';
opts.VariableNames = names;
opts.VariableTypes = repmat({'double'}, 1, numel(names));
opts.ExtraColumnsRule = 'ignore';
opts.EmptyLineRule = 'read';
T = readtable(path, opts);
T.Properties.VariableNames = matlab.lang.makeValidName(T.Properties.VariableNames, 'ReplacementStyle','delete');
end
