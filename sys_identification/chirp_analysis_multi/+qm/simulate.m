% Adapted from chirp_analysis/qdd_chirp_analysis.m; inspected 2026-10-20.
function y = simulate(J,B,c,d)
% Exact ZOH integration between measured command timestamps.
A=[0 1;-c.Kp/J -(B+c.Kd)/J]; b=[0;1/J];
n=numel(d.u); y=zeros(n,2); y(1,:)=d.initial.';
P=zeros(2,2,numel(d.uniqueDt)); Q=zeros(2,numel(d.uniqueDt));
for j=1:numel(d.uniqueDt)
    E=expm([A b;0 0 0]*d.uniqueDt(j));
    P(:,:,j)=E(1:2,1:2); Q(:,j)=E(1:2,3);
end
if isscalar(d.uniqueDt)
    % Equivalent discrete transfer recursion, including measured initial state.
    % Compiled FILTER avoids a long interpreted loop at every objective call.
    D=P(:,:,1); g=Q(:,1); z=d.initial;
    den=[1,-trace(D),det(D)];
    y(:,1)=filter([0,g(1),D(1,2)*g(2)-D(2,2)*g(1)],den,d.u, ...
        [z(1);D(1,2)*z(2)-D(2,2)*z(1)]);
    y(:,2)=filter([0,g(2),D(2,1)*g(1)-D(1,1)*g(2)],den,d.u, ...
        [z(2);D(2,1)*z(1)-D(1,1)*z(2)]);
    return
end
z=d.initial;
for i=1:n-1
    j=d.dtGroup(i);
    z=P(:,:,j)*z+Q(:,j)*d.u(i);
    y(i+1,:)=z.';
end
end


