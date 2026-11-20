% Design/evaluate the asymmetric fixed notch with MATLAB (base MATLAB only).
clear; close all; clc;
root=fileparts(fileparts(mfilename('fullpath')));
resultPath=fullfile(root,'verification','asymmetric_notch_results.txt');
fid=fopen(resultPath,'w'); fclose(fid); diary(resultPath);
cfg=fileread(fullfile(root,'control_config.h'));
readval=@(name) str2double(regexp(cfg,[name '\s*=\s*([0-9.eE+\-]+)'],'tokens','once'));
Fs=1e6/readval('CONTROL_PERIOD_US'); centerHz=readval('NOTCH_CENTER_HZ');
lowOffsetHz=readval('NOTCH_LOW_SIDE_HZ'); highOffsetHz=readval('NOTCH_HIGH_SIDE_HZ'); depth=readval('NOTCH_DEPTH');
edgeHz=[centerHz-lowOffsetHz centerHz+highOffsetHz];
assert(all(isfinite([Fs centerHz lowOffsetHz highOffsetHz depth])) && Fs>0 && centerHz>0 && lowOffsetHz>0 && lowOffsetHz<centerHz && highOffsetHz>0 && edgeHz(2)<Fs/2 && depth>1-10^(-3/20) && depth<=1,'Invalid settings or depth too shallow for -3 dB crossings');
targetDb=-3; targetGain=10^(targetDb/20);
% A biquad with zeros fixed at 12 Hz and poles at an independently fitted
% radius/frequency gives an approximate minimum near 12 Hz and asymmetric skirts.
encode=@(r,fp)[log(r/(1-r)),log((fp/(Fs/2))/(1-fp/(Fs/2)))];
decode=@(p)[1/(1+exp(-p(1))),(Fs/2)/(1+exp(-p(2)))];

opts=optimset('Display','off','TolX',1e-11,'TolFun',1e-14,'MaxIter',4000,'MaxFunEvals',10000);
starts=[exp(-pi*sum([lowOffsetHz highOffsetHz])/Fs) centerHz];
best=Inf; p=[];
for k=1:size(starts,1)
  [q,c]=fminsearch(@(x) objective(x,Fs,centerHz,edgeHz,depth,targetGain),encode(starts(k,1),starts(k,2)),opts);
  if c<best, best=c; p=q; end
end
v=decode(p); r=v(1); fp=v(2);
w0=2*pi*centerHz/Fs; wp=2*pi*fp/Fs;
b0=[1 -2*cos(w0) 1]; a=[1 -2*r*cos(wp) r^2];
kdc=sum(a)/sum(b0); b0=kdc*b0;
b=(1-depth)*a+depth*b0;
designName='asymmetric pole-shift';
checkF=unique([linspace(0,Fs/2,20001) linspace(0,min(Fs/2,2*edgeHz(2)),20001) edgeHz centerHz]);
checkZ=exp(-1i*2*pi*checkF/Fs);
checkH=polyval(fliplr(b),checkZ)./polyval(fliplr(a),checkZ);
edgeH=polyval(fliplr(b),exp(-1i*2*pi*edgeHz/Fs))./polyval(fliplr(a),exp(-1i*2*pi*edgeHz/Fs));
if any(~isfinite(checkH)) || max(abs(checkH))>1+1e-8 || any(abs(20*log10(abs(edgeH))+3)>0.001)
  % Low-order, non-amplifying approximation when the asymmetric fit is infeasible.
  alpha=sin(w0)*(lowOffsetHz+highOffsetHz)/(2*centerHz);
  b0=[1 -2*cos(w0) 1]/(1+alpha);
  a=[1 -2*cos(w0)/(1+alpha) (1-alpha)/(1+alpha)];
  b=(1-depth)*a+depth*b0;
  designName='RBJ approximation (requested asymmetry not exactly realizable)';
end
fprintf('Design: %s\nRequested -3 dB edges: %.9f / %.9f Hz\n',designName,edgeHz);
f=unique([linspace(0,min(Fs/2,2*edgeHz(2)),200001) linspace(0,Fs/2,20001) edgeHz centerHz]); z=exp(-1i*2*pi*f/Fs);
H=polyval(fliplr(b),z)./polyval(fliplr(a),z);
mag=abs(H); db=20*log10(max(mag,realmin)); phase=unwrap(angle(H))*180/pi;
edgeDb=interp1(f,db,edgeHz); centerDb=interp1(f,db,centerHz);
crossLo=crossing(f,db,edgeHz(1),targetDb,0,centerHz);
crossHi=crossing(f,db,edgeHz(2),targetDb,centerHz,Fs/2);
fprintf('Design parameters: center %.9g Hz, low offset %.9g Hz, high offset %.9g Hz\n',centerHz,lowOffsetHz,highOffsetHz);
fprintf('b = [%.15g %.15g %.15g]\n',b);
fprintf('a = [%.15g %.15g %.15g]\n',a);
fprintf('DC=%.12g (%.9g dB); gain at edges=%.9f / %.9f dB; center=%.9f dB\n',mag(1),db(1),edgeDb(1),edgeDb(2),centerDb);
fprintf('Crossings=%.9f / %.9f Hz; errors=%.9g / %.9g Hz\n',crossLo,crossHi,crossLo-edgeHz(1),crossHi-edgeHz(2));
fprintf('Max gain (0..Nyquist)=%.12g (%.9g dB); attenuation target=%.9f dB\n',max(mag),max(db),20*log10(1-depth));
fprintf('Pole radii=%.12g, %.12g\n',abs(roots(a)));
[minimumGain,imin]=min(mag);
fprintf('Minimum: %.6f Hz / %.9f dB; frequency error %.6f Hz\n',f(imin),20*log10(minimumGain),f(imin)-centerHz);
fprintf('Phase at low edge / center / high edge: %.9f / %.9f / %.9f deg\n',interp1(f,phase,[edgeHz(1) centerHz edgeHz(2)]));
assert(max(abs(roots(a)))<1 && max(mag)<1+1e-8);
assert(all(isfinite([crossLo crossHi])) && all(abs([crossLo crossHi]-edgeHz)./edgeHz<=0.1),'Approximation exceeds 10 percent edge-frequency error; coefficients were not updated');
save(fullfile(root,'verification','matlab_reference.mat'),'b0','a','b','edgeHz','depth','Fs');
diary off;
figure('Color','w','Position',[100 100 900 700]);
subplot(2,1,1); plot(f,db,'LineWidth',1.5); grid on; xlim([0 min(Fs/2,2*edgeHz(2))]); ylim([-30 1]);
yline(-3,'--k'); xline(edgeHz(1),':r'); xline(centerHz,':k'); xline(edgeHz(2),':r');
xlabel('Frequency (Hz)'); ylabel('Magnitude (dB)'); title(sprintf('Fixed notch magnitude (Fs = %.9g Hz)',Fs));
subplot(2,1,2); plot(f,phase,'LineWidth',1.5); grid on; xlim([0 min(Fs/2,2*edgeHz(2))]);
xline(edgeHz(1),':r'); xline(centerHz,':k'); xline(edgeHz(2),':r');
xlabel('Frequency (Hz)'); ylabel('Unwrapped phase (degrees)'); title('Phase response');
exportgraphics(gcf,fullfile(fileparts(mfilename('fullpath')),'asymmetric_notch_response.png'),'Resolution',160);

function c=objective(p,Fs,f0,edges,depth,target)
  r=1/(1+exp(-p(1))); fp=(Fs/2)/(1+exp(-p(2)));
  a=[1 -2*r*cos(2*pi*fp/Fs) r^2]; bn=[1 -2*cos(2*pi*f0/Fs) 1];
  bn=bn*sum(a)/sum(bn); b=(1-depth)*a+depth*bn;
  z=exp(-1i*2*pi*edges/Fs); H=polyval(fliplr(b),z)./polyval(fliplr(a),z);
  c=sum((20*log10(abs(H))-20*log10(target)).^2);
  % Penalize any gain increase over a dense 0..100 Hz sweep.
  f=unique([linspace(0,Fs/2,2001) linspace(0,min(Fs/2,2*edges(2)),2001)]); z=exp(-1i*2*pi*f/Fs);
  h=polyval(fliplr(b),z)./polyval(fliplr(a),z);
  c=c+100*sum(max(abs(h)-1,0).^2);
end

function fc=crossing(f,g,seed,target,left,right)
  idx=find(f>=left & f<=right); ix=idx(1:end-1);
  cross=find((g(ix)-target).*(g(ix+1)-target)<=0);
  if isempty(cross), fc=NaN; return; end
  [~,q]=min(abs(f(ix(cross))-seed)); i=ix(cross(q));
  fc=interp1(g(i:i+1),f(i:i+1),target,'linear');
end






