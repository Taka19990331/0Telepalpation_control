function [p,starts]=fit(trials,o)
% Equal trial weight, each angle MSE divided by that trial's angle variance.
c=trials{1}.c; js=c.Kp./(2*pi*[c.f0 sqrt(c.f0*c.f1) c.f1]).^2;
opts=optimset('Display','off','MaxIter',o.maxIterations, ...
    'MaxFunEvals',4*o.maxIterations,'TolX',1e-7,'TolFun',1e-9);
rows=[];
for j=js
    for b=[0 .1 1]*max(c.Kd,sqrt(j*c.Kp))
        if b==0
            [z,cost,flag,info]=fminsearch(@(z)qm.cost([exp(z) 0],0,trials),log(j),opts);
            jb=[exp(z) 0];
        else
            [z,cost,flag,info]=fminsearch(@(z)qm.cost(exp(z),0,trials),log([j b]),opts);
            jb=exp(z);
        end
        rows(end+1,:)=[j b jb cost flag info.iterations]; %#ok<AGROW>
    end
end
starts=array2table(rows,'VariableNames',{'seedJ','seedB','J','B','cost','exitflag','iterations'});
ok=find(starts.exitflag>0 & starts.cost<1e90);
assert(~isempty(ok),'No multistart solution converged');
[~,i]=min(starts.cost(ok)); i=ok(i); p=[starts.J(i) starts.B(i)];
end
