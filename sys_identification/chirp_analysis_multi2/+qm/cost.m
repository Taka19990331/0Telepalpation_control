function v=cost(p,Td,trials)
v=1e100;
if any(~isfinite(p)) || p(1)<=0 || p(2)<0, return; end
costs=zeros(numel(trials),1);
for k=1:numel(trials)
    tr=trials{k};
    try
        y=qm.predict(p(1),p(2),Td,tr);
    catch
        return
    end
    costs(k)=mean((y(:,1)-tr.d.x).^2)/max(var(tr.d.x,1),eps);
end
if all(isfinite(costs)), v=mean(costs); end
end
