function y=predict(J,B,Td,tr)
% Model B: delay zero-state forced response only; measured initial-state
% homogeneous response evolves from PD activation t=0 without a time shift.
d=tr.d; c=tr.c;
if Td==0, y=qm.simulate(J,B,c,d); return; end
z=d; z.initial=[0;0]; forced=qm.simulate(J,B,c,z);
z=d; z.u=zeros(size(d.u)); y=qm.simulate(J,B,c,z);
query=d.t-Td; take=find(query>=0);
ix=discretize(query(take),[d.t;inf]);
h=query(take)-d.t(ix);
[hu,~,group]=unique(round(h*1e12)/1e12);
A=[0 1;-c.Kp/J -(B+c.Kd)/J]; b=[0;1/J];
for k=1:numel(hu)
    loc=find(group==k); j=ix(loc);
    E=expm([A b;0 0 0]*hu(k));
    shifted=forced(j,:)*E(1:2,1:2)'+d.u(j)*E(1:2,3)';
    y(take(loc),:)=y(take(loc),:)+shifted;
end
end
