% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
function value = textValue(M,key)
i=find(M.Key==string(key));
assert(isscalar(i),'Missing/ambiguous CSV condition: %s',key);
value=M.Value(i);
end


