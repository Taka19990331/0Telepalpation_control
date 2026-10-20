function test_qdd_chirp_analysis
% Integration tests. Control System Toolbox used ONLY as independent oracle.
root=fileparts(mfilename('fullpath'));
source=fullfile(fileparts(root),'captures','chirp_20261020_101838_880284.csv');
text=fileread(source);
lines=splitlines(string(text));
header=lines(startsWith(lines,'#'));
io=detectImportOptions(source,'NumHeaderLines',numel(header),'VariableNamingRule','preserve');
T=readtable(source,io);
temp=fullfile(root,'test_output');
if ~isfolder(temp), mkdir(temp); end
file=fullfile(temp,'synthetic.csv');
opt=struct('makeFigures',false,'saveResults',false);
% Known physical model, nonzero initial angle AND speed. Independent LSIM.
J=0.006; B=0.025; Kp=0.00834*180/pi; Kd=0.100000001;
q0=-33.822933197*pi/180;
active=T.phase==1 | T.phase==2;
tt=T.actual_us(active)*1e-6; tt=tt-tt(1);
sys=ss([0 1;-Kp/J -(B+Kd)/J],[0;1/J],eye(2),zeros(2,1));
yy=lsim(sys,T.chirp_Nm(active),tt,[0.03;-0.02],'zoh');
T.position_deg(active)=(q0+yy(:,1))*180/pi;
T.speed_rad_s(active)=yy(:,2);
T.pd_Nm(active)=-Kp*yy(:,1)-Kd*yy(:,2);
T.command_Nm(active)=T.chirp_Nm(active)+T.pd_Nm(active);
% Stop sample must not introduce an artificial discontinuity.
last=find(active,1,'last');
T.position_deg(last+1:end)=T.position_deg(last);
T.speed_rad_s(last+1:end)=T.speed_rad_s(last);
writeFixture(file,header,T);
evalc('r=qdd_chirp_analysis(file,opt);');
assert(abs(r.fit.J/J-1)<1e-4,'Synthetic J recovery failed.');
assert(abs(r.fit.B/B-1)<1e-4,'Synthetic B recovery failed.');
assert(r.fit.RMSE<1e-7,'Initial state or ZOH timing is incorrect.');
assert(abs(r.conditions.Kp-Kp)<1e-12,'Degree gain conversion failed.');
assert(any(r.frf.usableWindow) && any(~r.frf.usableWindow),'FRF quality flags missing.');
% Missing input column must reconstruct the same excitation from CSV metadata.
T2=removevars(T,'chirp_Nm'); writeFixture(file,header,T2);
evalc('reconstructed=qdd_chirp_analysis(file,opt);');
assert(abs(reconstructed.fit.J/J-1)<1e-4,'Reconstructed input recovery failed.');
% Exact admissible boundary B=0 must not be excluded by a log transform.
zeroB=T;
sys0=ss([0 1;-Kp/J -Kd/J],[0;1/J],eye(2),zeros(2,1));
y0=lsim(sys0,T.chirp_Nm(active),tt,[0.03;-0.02],'zoh');
zeroB.position_deg(active)=(q0+y0(:,1))*180/pi;
zeroB.speed_rad_s(active)=y0(:,2);
zeroB.pd_Nm(active)=-Kp*y0(:,1)-Kd*y0(:,2);
zeroB.command_Nm(active)=zeroB.chirp_Nm(active)+zeroB.pd_Nm(active);
zeroB.position_deg(last+1:end)=zeroB.position_deg(last);
writeFixture(file,header,zeroB);
evalc('boundary=qdd_chirp_analysis(file,opt);');
assert(abs(boundary.fit.J/J-1)<1e-4 && boundary.fit.B<1e-6,'B=0 boundary recovery failed.');
% Negative tests must fail before any unsupported parameter estimate.
badHeader=header(~startsWith(header,'# SAMPLING_FREQUENCY_HZ'));
writeFixture(file,badHeader,T); expectFailure(file,opt,'Missing/ambiguous CSV condition');
bad=T; bad.actual_us(100)=bad.actual_us(99);
writeFixture(file,header,bad); expectFailure(file,opt,'Nonmonotonic actual timestamps');
bad=T; bad.torque_limited(5000)=1;
writeFixture(file,header,bad); expectFailure(file,opt,'Torque limiting');
bad=T; bad.pd_Nm(5000)=bad.pd_Nm(5000)+0.1;
writeFixture(file,header,bad); expectFailure(file,opt,'Recorded PD');
bad=T; bad.position_deg(5000)=NaN;
writeFixture(file,header,bad); expectFailure(file,opt,'Missing/nonfinite');
% Leave a valid synthetic fixture for reproducibility.
writeFixture(file,header,T);
fprintf('PASS: synthetic J/B + nonzero IC, reconstructed input, B=0, missing metadata, timing, clipping, PD, NaN.\n');
end

function writeFixture(file,header,T)
fid=fopen(file,'w'); assert(fid>=0);
for k=1:numel(header), fprintf(fid,'%s\n',header(k)); end
fclose(fid);
writetable(T,file,'WriteMode','append','WriteVariableNames',true);
end

function expectFailure(file,opt,fragment)
failed=false;
try
    evalc('qdd_chirp_analysis(file,opt);');
catch err
    failed=contains(err.message,fragment);
    if ~failed, rethrow(err); end
end
assert(failed,'Expected rejection: %s',fragment);
end
