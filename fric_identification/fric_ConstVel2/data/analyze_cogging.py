import csv, numpy as np
path=r'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_202404.csv'
meta={}; raw=[]
with open(path,encoding='utf-8-sig',newline='') as f:
    for line in f:
        if line.startswith('#'):
            k,_,v=line[1:].strip().partition(','); meta[k]=v
        else:
            raw=list(csv.DictReader([line]+list(f))); break
x={k:np.array([float(r[k]) for r in raw]) for k in raw[0]}
t=x['timestamp_us']*1e-6; p=x['position_rad']; v=x['velocity_rad_s']; tau=x['torque_estimate_nm']; integ=x['i_torque_command_nm']; cmd=x['command_rad_s']
kp=1.44566; torque_cmd=kp*(cmd-v)+integ
# unwrap MIT position's 25 rad representation
D=np.diff(p); D += np.where(D>12.5,-25.0,np.where(D < -12.5,25.0,0.0)); pu=np.r_[p[0],p[0]+np.cumsum(D)]
mask=t>=0.5; ts=t[mask]; ps=pu[mask]; vs=v[mask]; taus=tau[mask]; tcs=torque_cmd[mask]
print('META',meta)
print('n,duration,unwrapped span,turns',len(t),t[-1],ps[-1]-ps[0],(ps[-1]-ps[0])/(2*np.pi))
for name,a in [('velocity',vs),('torque_est',taus),('torque_cmd_reconstructed',tcs)]: print(name,'mean/std/rmsAC/p5/p95',np.mean(a),np.std(a),np.sqrt(np.mean((a-np.mean(a))**2)),np.percentile(a,[5,50,95]))
print('corr tau-est vs cmd',np.corrcoef(taus,tcs)[0,1], 'corr tau-v',np.corrcoef(taus,vs)[0,1])
# spectrum in time
fs=2000; n=len(taus); win=np.hanning(n); f=np.fft.rfftfreq(n,1/fs)
def spectrum(a): return np.abs(np.fft.rfft((a-np.mean(a))*win))*2/win.sum()
for name,a in [('torque_est',taus),('torque_cmd',tcs),('speed',vs)]:
    A=spectrum(a); valid=(f>=0.5)&(f<=400); ids=np.where(valid)[0]; inds=ids[np.argsort(A[ids])[-15:][::-1]]
    print('PEAKS',name,[(round(float(f[i]),3),round(float(A[i]),6)) for i in inds[:12]])
rotHz=np.mean(vs)/(2*np.pi); print('rotation_hz',rotHz)
for name,a in [('tau_est',taus),('tau_cmd',tcs)]:
  A=spectrum(a); print('near rotor harmonics',name,[(k, round(float(f[np.argmin(np.abs(f-k*rotHz))]),3),round(float(A[np.argmin(np.abs(f-k*rotHz))]),6)) for k in range(1,41)])
# angle synchronous average one mechanical revolution
nb=180; phase=np.mod(ps,2*np.pi); idx=np.floor(phase/(2*np.pi)*nb).astype(int); idx=np.clip(idx,0,nb-1)
for name,a in [('tau_est',taus),('tau_cmd',tcs),('speed',vs)]:
  sums=np.bincount(idx,weights=a,minlength=nb); counts=np.bincount(idx,minlength=nb); prof=sums/np.maximum(counts,1); prof-=np.mean(prof)
  amp=np.abs(np.fft.rfft(prof))*2/nb; ords=np.fft.rfftfreq(nb,1/nb)
  ix=np.argsort(amp[1:])[-12:][::-1]+1
  print('ORDER',name,'profile RMS PP',np.std(prof),np.ptp(prof),'top',[(int(ords[j]),round(float(amp[j]),6)) for j in ix[:12]])
# per revolution repeatability against phase-averaged profile
nb=128; nrev=int((ps[-1]-ps[0])/(2*np.pi)); profs=[]
phase_rel=ps-ps[0]; turns=np.floor(phase_rel/(2*np.pi)).astype(int); ph=np.mod(phase_rel,2*np.pi)/(2*np.pi)
for r in range(nrev):
  m=turns==r
  if m.sum()<10: continue
  bins=np.linspace(0,1,nb+1); centers=(bins[:-1]+bins[1:])/2
  # interpolate torque samples within this revolution
  vals=np.interp(centers, ph[m], taus[m]); profs.append(vals)
P=np.array(profs); print('revs',P.shape[0], 'per-rev std of profiles',np.mean(np.std(P,axis=1)), 'mean-profile std',np.std(np.mean(P,axis=0)))
# each profile correlation to grand mean
G=np.mean(P,axis=0); print('rev profile correlations',np.round([np.corrcoef(r,G)[0,1] for r in P],3))
