function [x,scan]=bounded(fun,o)
% Global coarse search, then refinement around every sampled local minimum.
grid=unique([linspace(o.delayBounds(1),o.delayBounds(2),21) o.delaySeed]);
val=arrayfun(fun,grid); cand=grid; costs=val;
for k=2:numel(grid)-1
    if val(k)<=val(k-1) && val(k)<=val(k+1)
        [a,b]=fminbnd(fun,grid(k-1),grid(k+1),optimset('TolX',1e-8,'Display','off'));
        cand(end+1)=a; costs(end+1)=b; %#ok<AGROW>
    end
end
[~,i]=min(costs); x=cand(i);
scan=table(cand(:),costs(:),'VariableNames',{'delay_s','cost'});
end
