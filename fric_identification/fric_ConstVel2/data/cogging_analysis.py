import csv, math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont
CSV_PATH=r'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_202404.csv'
OUT_PNG=r'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_202404_cogging_analysis.png'
OUT_TXT=r'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_202404_cogging_analysis.txt'
meta={}; rows=[]
with open(CSV_PATH,encoding='utf-8-sig',newline='') as f:
    for line in f:
        if line.startswith('#'):
            k,_,v=line[1:].strip().partition(','); meta[k]=v
        else:
            rows=list(csv.DictReader([line]+list(f))); break
x={k:np.array([float(r[k]) for r in rows]) for k in rows[0]}
t=x['timestamp_us']*1e-6; rawp=x['position_rad']; vel=x['velocity_rad_s']; tau=x['torque_estimate_nm']; iterm=x['i_torque_command_nm']; target=x['command_rad_s']
# MIT feedback position is encoded over a 25 rad range; unwrap that rollover.
dp=np.diff(rawp); dp += np.where(dp>12.5,-25.,np.where(dp < -12.5,25.,0.)); pos=np.r_[rawp[0],rawp[0]+np.cumsum(dp)]
# The speed-loop proportional gain is from constvel_config.h; torque command is reconstructed from P + logged I.
tcmd=1.44566*(target-vel)+iterm
stable=t>=0.5; ts=t[stable]; ps=pos[stable]; vs=vel[stable]; ys=tau[stable]; cs=tcmd[stable]
omega=(ps[-1]-ps[0])/(ts[-1]-ts[0]); rev_hz=omega/(2*np.pi); turns=(ps[-1]-ps[0])/(2*np.pi)
# Time spectra with a Hann window.
fs=2000.; n=len(ts); win=np.hanning(n); f=np.fft.rfftfreq(n,1/fs)
def spectrum(a): return 2*np.abs(np.fft.rfft((a-np.mean(a))*win))/np.sum(win)
St=spectrum(ys); Sv=spectrum(vs); Sc=spectrum(cs)
def dominant(a,lo=0.5,hi=500):
    s=spectrum(a); ix=np.where((f>=lo)&(f<=hi))[0]; j=ix[np.argmax(s[ix])]; return float(f[j]),float(s[j])
f_tau,a_tau=dominant(ys,5,100); f_speed,a_speed=dominant(vs,80,250)
# Harmonic regression of signals against unwrapped rotor/output position.
def fit_order(a,theta,k):
    q=k*theta; M=np.column_stack((np.ones_like(q),np.sin(q),np.cos(q))); c=np.linalg.lstsq(M,a,rcond=None)[0]
    return float(np.hypot(c[1],c[2])),float(np.arctan2(c[2],c[1]))
orders=np.arange(1,201); ord_amp={name:np.array([fit_order(a,ps,k)[0] for k in orders]) for name,a in [('tau',ys),('speed',vs),('cmd',cs)]}
# Position-synchronous average, with enough bins to resolve order 108.
nb=1024; phase=np.mod(ps,2*np.pi); bi=np.minimum((phase/(2*np.pi)*nb).astype(int),nb-1); counts=np.bincount(bi,minlength=nb)
def folded(a): return np.bincount(bi,weights=a,minlength=nb)/np.maximum(counts,1)
prof_tau=folded(ys); prof_vel=folded(vs); prof_cmd=folded(cs)
# Per revolution repeatability (phase lock) for selected spatial orders.
turn=np.floor((ps-ps[0])/(2*np.pi)).astype(int)
perrev={}
for k in [8,16,107,108]:
    vals=[]
    for r in range(turn.min(),turn.max()+1):
        m=turn==r
        if np.count_nonzero(m)>100: vals.append(fit_order(ys[m],ps[m],k))
    aa=np.array([z[0] for z in vals]); pp=np.array([z[1] for z in vals]); perrev[k]=(aa,pp,float(abs(np.mean(np.exp(1j*pp)))))
# Time-frequency peak and corresponding order. Position-synchronous average harmonic orders.
order8_tau,phase8=fit_order(ys,ps,8); order8_speed,_=fit_order(vs,ps,8)
order108_tau,_=fit_order(ys,ps,108); order108_speed,_=fit_order(vs,ps,108)
foldfft_tau=2*np.abs(np.fft.rfft(prof_tau-prof_tau.mean()))/nb
# Summary report
lines=[]
lines.append(f'File: {os.path.basename(CSV_PATH)}')
lines.append(f'Rows: {len(t)}; sampling: {fs:.0f} Hz; logged duration: {t[-1]:.3f} s; target speed: {np.median(target):.3f} rad/s; end status: {meta.get("result_code")} (manual stop)')
lines.append(f'Steady segment: t >= 0.5 s; {len(ts)} samples; {ts[-1]-ts[0]:.3f} s; unwrapped angle {ps[-1]-ps[0]:.3f} rad = {turns:.3f} revolutions; mean angle speed {omega:.4f} rad/s.')
lines.append(f'Steady estimated torque: mean {ys.mean():.4f} N m; SD {ys.std():.4f} N m; 5-95% [{np.percentile(ys,5):.4f}, {np.percentile(ys,95):.4f}] N m.')
lines.append(f'Steady speed: mean {vs.mean():.4f} rad/s; SD {vs.std():.4f} rad/s.')
lines.append(f'Time spectrum: torque estimate peak in 5-100 Hz at {f_tau:.3f} Hz, amplitude {a_tau:.5f} N m; nominal 8th position order predicts {8*rev_hz:.3f} Hz. Speed peak in 80-250 Hz at {f_speed:.3f} Hz, amplitude {a_speed:.5f} rad/s, equivalent order {f_speed/rev_hz:.2f}/rev.')
lines.append(f'Position-order regression: order 8 amplitude {order8_tau:.5f} N m (speed amplitude {order8_speed:.5f} rad/s); order 108 amplitude {order108_tau:.5f} N m (speed amplitude {order108_speed:.5f} rad/s).')
for k,(aa,pp,R) in perrev.items(): lines.append(f'Order {k} per-revolution torque amplitudes: {np.array2string(aa,precision=4)} N m; phase-lock factor R={R:.3f} (1=repeatable phase, 0=unlocked).')
lines.append(f'Position-folded torque profile RMS: {np.std(prof_tau-prof_tau.mean()):.5f} N m; order-8 Fourier amplitude {foldfft_tau[8]:.5f} N m; order-108 amplitude {foldfft_tau[108]:.5f} N m.')
lines.append('Caveat: torque_estimate_nm is the motor controller estimate; commanded torque is reconstructed as Kp*(target_speed-measured_speed)+logged_iTorque. This run does not include an independent torque transducer or rotor electrical-angle/slot calibration.')
open(OUT_TXT,'w',encoding='utf-8').write('\n'.join(lines)+'\n')
print('\n'.join(lines))
# Draw a 2x2 diagnostic figure with Pillow, no smoothing or fitted data concealed.
W,H=1800,1260; im=Image.new('RGB',(W,H),'white'); dr=ImageDraw.Draw(im)
font_path=r'C:\Windows\Fonts\segoeui.ttf'; bold_path=r'C:\Windows\Fonts\segoeuib.ttf'
def font(size,bold=False):
    try:return ImageFont.truetype(bold_path if bold else font_path,size)
    except:return ImageFont.load_default()
F=font(20); FB=font(26,True); FT=font(34,True); FS=font(17)
black=(28,34,42); blue=(35,105,190); orange=(220,80,55); grid=(220,225,232); muted=(90,100,112)
dr.text((65,24),f'Cogging / torque-ripple diagnostics — {os.path.basename(CSV_PATH)}',font=FT,fill=black)
dr.text((65,74),f'Steady segment t ≥ 0.5 s; {turns:.2f} revolutions at {omega:.2f} rad/s',font=F,fill=muted)

def panel(box,title,xlabel,ylabel,series,xlim,ylim,xticks,yticks):
    x0,y0,x1,y1=box; L=x0+88; R=x1-24; T=y0+54; B=y1-60
    dr.text((x0,y0),title,font=FB,fill=black)
    for val in yticks:
        yy=B-(val-ylim[0])/(ylim[1]-ylim[0])*(B-T); dr.line((L,yy,R,yy),fill=grid,width=1)
        dr.text((x0+2,yy-11),f'{val:g}',font=FS,fill=muted)
    for val in xticks:
        xx=L+(val-xlim[0])/(xlim[1]-xlim[0])*(R-L); dr.line((xx,T,xx,B),fill=grid,width=1)
        dr.text((xx-18,B+10),f'{val:g}',font=FS,fill=muted)
    dr.line((L,T,L,B),fill=black,width=2); dr.line((L,B,R,B),fill=black,width=2)
    dr.text((L+(R-L)/2-40,y1-28),xlabel,font=FS,fill=black); dr.text((x0+2,T-30),ylabel,font=FS,fill=black)
    for xx,yy,c,label in series:
        # downsample only for rendering
        step=max(1,len(xx)//4000); inds=np.arange(0,len(xx),step); inds=np.r_[inds,len(xx)-1] if inds[-1]!=len(xx)-1 else inds
        pts=[]
        for j in inds:
            if xlim[0]<=xx[j]<=xlim[1] and ylim[0]<=yy[j]<=ylim[1]:
                px=L+(xx[j]-xlim[0])/(xlim[1]-xlim[0])*(R-L); py=B-(yy[j]-ylim[0])/(ylim[1]-ylim[0])*(B-T); pts.append((int(px),int(py)))
            else: pts.append(None)
        last=None
        for q in pts:
            if q is None: last=None
            elif last is not None: dr.line((*last,*q),fill=c,width=2)
            last=q
    # simple legend
    lx=L+8; ly=T+7
    for c,label in [(s[2],s[3]) for s in series]:
        dr.line((lx,ly+9,lx+28,ly+9),fill=c,width=3); dr.text((lx+36,ly-2),label,font=FS,fill=black); lx+=180

# Panel 1: 0.5 s of steady time trace
mshow=(ts>=ts[-1]-0.5)
panel((40,120,885,655),'Estimated torque (last 0.5 s)','Time (s)','Torque (N m)',[(ts[mshow],ys[mshow],orange,'Estimated torque')],(ts[-1]-0.5,ts[-1]),(0.05,0.35),[round(ts[-1]-0.5,2),round(ts[-1]-0.25,2),round(ts[-1],2)],[0.05,0.15,0.25,0.35])
# Better separate variables in spectrum and order panels; time spectrum torque and spatial orders.
fmax=250; sf=(f<=fmax)
panel((915,120,1760,655),'Torque estimate spectrum','Frequency (Hz)','Amplitude (N m)',[(f[sf],St[sf],orange,'Torque estimate')],(0,fmax),(0,max(0.035,float(np.max(St[sf])*1.05))),[0,50,100,150,200,250],[0,0.01,0.02,0.03])
# position order spectrum for torque estimate
ordmax=128; oq=np.arange(1,ordmax+1); oa=ord_amp['tau'][:ordmax]
panel((40,690,885,1220),'Torque estimate vs position order','Cycles per revolution','Fitted amplitude (N m)',[(oq,oa,blue,'Torque estimate')],(0,128),(0,max(0.02,float(oa.max()*1.1))),[0,32,64,96,128],[0,0.005,0.01,0.015,0.02])
# phase folded profile and an order-8 fitted wave
nb_plot=128
phase_plot=np.mod(ps,2*np.pi); bi_plot=np.minimum((phase_plot/(2*np.pi)*nb_plot).astype(int),nb_plot-1)
cnt_plot=np.bincount(bi_plot,minlength=nb_plot)
prof_tau_plot=np.bincount(bi_plot,weights=ys,minlength=nb_plot)/np.maximum(cnt_plot,1)
phasebin=(np.arange(nb_plot)+0.5)*360/nb_plot
q=8*ps; M=np.column_stack((np.ones_like(q),np.sin(q),np.cos(q))); coef=np.linalg.lstsq(M,ys,rcond=None)[0]
phgrid=np.arange(nb_plot)*2*np.pi/nb_plot; prof_fit=coef[0]+coef[1]*np.sin(8*phgrid)+coef[2]*np.cos(8*phgrid)
ylo=float(min(prof_tau_plot.min(),prof_fit.min())-0.005); yhi=float(max(prof_tau_plot.max(),prof_fit.max())+0.005)
panel((915,690,1760,1220),'Position-synchronous average (one revolution)','Position phase (degrees)','Torque estimate (N m)',[(phasebin,prof_tau_plot,blue,'Folded mean'),(phasebin,prof_fit,orange,'Order-8 fit')],(0,360),(ylo,yhi),[0,90,180,270,360],list(np.linspace(ylo,yhi,5)))
im.save(OUT_PNG)
print('PLOT',OUT_PNG)
print('REPORT',OUT_TXT)
