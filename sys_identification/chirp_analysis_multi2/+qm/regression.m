function F=regression(tr,Y,o)
% Columns of Y share the recorded chirp phase and identical Hann windows.
d=tr.d; c=tr.c; ts=d.nominalChirpTime;
f=logspace(log10(c.f0),log10(c.f1),o.frfPoints)';
phi=2*pi*c.f0*expm1(c.k*ts)/c.k;
C=complex(nan(numel(f),size(Y,2))); r2=nan(size(C)); diag=nan(numel(f),5);
status=repmat("not-estimable",numel(f),1);
for i=1:numel(f)
    center=log(f(i)/c.f0)/c.k; width=o.frfCycles/f(i);
    ix=find(abs(ts-center)<=width/2 & d.phase==1);
    if numel(ix)<4, continue; end
    z=(ts(ix)-center)/(width/2); w=.5*(1+cos(pi*z));
    X=[cos(phi(ix)) sin(phi(ix)) ones(numel(ix),1) z]; Xw=X.*sqrt(w);
    rc=rcond(Xw'*Xw);
    if rc<1e-12, continue; end
    coef=Xw\(Y(ix,:).*sqrt(w)); C(i,:)=coef(1,:)-1i*coef(2,:);
    err=Y(ix,:)-X*coef;
    r2(i,:)=1-sum(w.*err.^2)./max(sum(w.*(Y(ix,:)-sum(w.*Y(ix,:))/sum(w)).^2),eps);
    cycles=(phi(ix(end))-phi(ix(1)))/(2*pi);
    bandwidth=(c.f0*exp(c.k*ts(ix(end)))-c.f0*exp(c.k*ts(ix(1))))/f(i);
    diag(i,:)=[numel(ix) rc cycles bandwidth double(center-width/2<0 || center+width/2>c.T)];
    status(i)="full";
    if diag(i,5), status(i)="edge"; end
    if cycles<2, status(i)=status(i)+";under-2-cycles"; end
end
F=struct('f',f,'C',C,'R2',r2,'diagnostics',diag,'status',status);
end
