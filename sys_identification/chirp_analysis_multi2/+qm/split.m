function [train,test]=split(n)
% Deterministic chronological holdout: latest ceil(n/4) trials; singleton fits only.
if n<2, train=1:n; test=[]; return; end
test=(n-ceil(n/4)+1):n; train=setdiff(1:n,test);
assert(isempty(intersect(train,test)));
end
