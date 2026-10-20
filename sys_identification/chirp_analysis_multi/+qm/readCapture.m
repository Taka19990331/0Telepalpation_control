% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
function [T,M] = readCapture(file)
assert(isfile(file),'CSV not found: %s',file);
lines = splitlines(string(fileread(file)));
isComment = startsWith(strtrim(lines),'#');
comments = lines(isComment);
keys = strings(0,1); values=keys;
for i=1:numel(comments)
    s=char(comments(i)); eq=find(s=='=',1);
    if ~isempty(eq)
        keys(end+1,1)=strtrim(s(2:eq-1)); %#ok<AGROW>
        values(end+1,1)=strtrim(s(eq+1:end)); %#ok<AGROW>
    end
end
assert(numel(unique(keys))==numel(keys),'Duplicate metadata keys in CSV.');
M=table(keys,values,'VariableNames',{'Key','Value'});
header=find(~isComment & strlength(strtrim(lines))>0,1);
assert(~isempty(header),'CSV has no data header.');
io=detectImportOptions(file,'NumHeaderLines',header-1,'VariableNamingRule','preserve');
T=readtable(file,io);
assert(height(T)>10,'Insufficient samples.');
end


