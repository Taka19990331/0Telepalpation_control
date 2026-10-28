exec(open(r'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\cogging_metrics.py').read().split("print(f'ROWS=")[0])
# variables defined above in the common section; inspect harmonics over individual revolutions.
turn=np.floor((th-th[0])/(2*np.pi)).astype(int)
for k in [1,2,8,16,32,54,107,108]:
 print('ORDER',k)
 for name,a in [('speed',vv),('tau',tt),('cmd',tc)]:
  rr=[]
  for j in range(turn.min(),turn.max()+1):
   mm=turn==j
   if mm.sum()<100: continue
   rr.append(fit_harm(a[mm],th[mm],k))
  A=np.array([z[0] for z in rr]); ph=np.array([z[1] for z in rr]); R=np.abs(np.mean(np.exp(1j*ph)))
  print(name,'amps',np.round(A,4),'phaseLock',round(float(R),3),'mean',round(float(A.mean()),5))
# Time spectrum amplitudes around 8x mechanical rotation and exact frequency vs all-orders
rot_hz=th[-1]-th[0]
rot_hz/=(ts[-1]-ts[0]); rot_hz/=2*np.pi
n=len(ts); win=np.hanning(n); freq=np.fft.rfftfreq(n,1/2000)
def sp(a):return 2*np.abs(np.fft.rfft((a-a.mean())*win))/win.sum()
for name,a in [('speed',vv),('tau',tt),('cmd',tc)]:
 A=sp(a); f0=8*rot_hz; ii=np.argmin(abs(freq-f0)); print('8x',name,'targetHz',f0,'nearest',freq[ii],'amp',A[ii])
