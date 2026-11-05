function tests=test_qdd_chirp_multi
tests=functiontests(localfunctions);
end
function testDifferentInitialStates(tc)
o=qm.defaults(struct('maxIterations',350)); tr=synthetic(0);
[p,starts]=qm.fit(tr,o);
verifyLessThan(tc,abs(p(1)/.012-1),2e-4);
verifyLessThan(tc,abs(p(2)/.045-1),2e-3);
verifyTrue(tc,any(starts.exitflag>0));
end
function testRelativeAngleAndMetadata(tc)
o=qm.defaults(struct);
[T,M]=qm.readCapture(fullfile(o.captureDir,'chirp_20261020_102301_899188.csv'));
[d,c]=qm.validate(T,M,o);
M2=M;
M2.Value(M2.Key=="REFERENCE_POSITION_DEG (theta_ref)")=string(sprintf('%.17g',c.q0*180/pi+50));
M2.Value(M2.Key=="CONFIG.Hold_deg")=string(sprintf('%.17g',c.q0*180/pi+50));
T.position_deg=T.position_deg+50;
[d2,~]=qm.validate(T,M2,o);
verifyEqual(tc,d2.x,d.x,'AbsTol',1e-12);
verifyEmpty(tc,qm.compareMetadata(M2,M));
M2.Value(M2.Key=="CONFIG.FeedbackTimeout_us")="30000";
verifyNotEmpty(tc,qm.compareMetadata(M2,M));
M2=M; M2.Value(M2.Key=="CONFIG.F0_Hz")="2";
failed=false;
try, qm.validate(T,M2,o); catch, failed=true; end
verifyTrue(tc,failed);
end
function testCoherenceFormula(tc)
U=[1 2 3;1 1 1]; Y=[2 4 6;1 1i -1];
s=qm.spectra(U,Y);
expected=abs(mean(conj(U).*Y,2)).^2./(mean(abs(U).^2,2).*mean(abs(Y).^2,2));
verifyEqual(tc,s.coherence,expected,'AbsTol',1e-14);
verifyEqual(tc,s.H1,mean(conj(U).*Y,2)./mean(abs(U).^2,2),'AbsTol',1e-14);
verifyEqual(tc,s.Hmean,mean(Y./U,2),'AbsTol',1e-14);
z=qm.spectra(U(:,1),Y(:,1)); verifyTrue(tc,all(isnan(z.coherence)));
end
function testDelayRecovery(tc)
tr=synthetic(.00145); o=qm.defaults(struct('frfPoints',55));
[td,~]=qm.bounded(@(td)qm.cost([.012 .045],td,tr),o);
verifyLessThan(tc,abs(td-.00145),1e-7);
D=qm.delay(tr,[.012 .045],o);
verifyLessThan(tc,abs(D.frf_s-.00145),2e-6);
end
function testZeroDelayLegacyAndExactZOH(tc)
tr=synthetic(0); tr=tr{1}; d=tr.d; c=tr.c;
y=qm.predict(.012,.045,0,tr); legacy=qm.simulate(.012,.045,c,d);
verifyEqual(tc,y,legacy,'AbsTol',1e-13);
A=[0 1;-c.Kp/.012 -(.045+c.Kd)/.012]; b=[0;1/.012];
z=d.initial; reference=zeros(size(y)); reference(1,:)=z';
for k=1:numel(d.t)-1
    E=expm([A b;0 0 0]*(d.t(k+1)-d.t(k)));
    z=E(1:2,1:2)*z+E(1:2,3)*d.u(k); reference(k+1,:)=z';
end
verifyLessThan(tc,max(abs(y-reference),[],'all'),1e-9);
end
function testHoldoutNoLeakage(tc)
[a,b]=qm.split(13); verifyEmpty(tc,intersect(a,b));
verifyEqual(tc,a,1:9); verifyEqual(tc,b,10:13);
tr=synthetic(0); [a,b]=qm.split(numel(tr));
o=qm.defaults(struct); p1=qm.fit(tr(a),o);
tr{b}.d.x=tr{b}.d.x*100+50;
p2=qm.fit(tr(a),o); verifyEqual(tc,p1,p2);
end
function testFractionalDelayIndependentOracle(tc)
tr=synthetic(0); tr=tr{1}; td=.00137;
got=qm.predict(.012,.045,td,tr); d=tr.d; c=tr.c;
A=[0 1;-c.Kp/.012 -(.045+c.Kd)/.012]; b=[0;1/.012];
% Independent event-grid solution: shifted input events and output samples.
events=unique([d.t;d.t+td]); events=events(events<=d.t(end));
z=[0;0]; expected=zeros(size(got)); sample=1;
for i=1:numel(events)
    t=events(i);
    if i>1
        mid=(events(i)+events(i-1))/2-td;
        ix=find(d.t<=mid,1,'last'); u=0; if ~isempty(ix),u=d.u(ix);end
        E=expm([A b;0 0 0]*(events(i)-events(i-1)));
        z=E(1:2,1:2)*z+E(1:2,3)*u;
    end
    if sample<=numel(d.t) && t==d.t(sample)
        expected(sample,:)=(z+expm(A*t)*d.initial)';
        sample=sample+1;
    end
end
verifyLessThan(tc,max(abs(got-expected),[],'all'),2e-9);
end
function tr=synthetic(td)
c=struct('Kp',.48,'Kd',.1,'f0',1,'f1',100,'T',4,'k',log(100)/4,'fs',2000);
t=(0:1/c.fs:4.5)'; u=sin(2*pi*c.f0*expm1(c.k*t)/c.k);
u(t>=4)=0; d=struct('t',t,'u',u,'nominalChirpTime',t,'phase',1+(t>=4));
d.dt=diff(t); d.uniqueDt=1/c.fs; d.dtGroup=ones(numel(t)-1,1);
tr=cell(1,3);
for k=1:3
    cc=c; cc.q0=(-40+k*17)*pi/180; d.initial=[.02*k;-.04*k];
    z=struct('c',cc,'d',d); pred=qm.predict(.012,.045,td,z);
    z.d.x=pred(:,1); z.d.v=pred(:,2); tr{k}=z;
end
end
