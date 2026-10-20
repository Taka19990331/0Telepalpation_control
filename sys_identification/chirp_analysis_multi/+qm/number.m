% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
function x = number(M,key)
x=str2double(qm.textValue(M,key));
assert(isfinite(x),'Non-numeric CSV condition: %s',key);
end


