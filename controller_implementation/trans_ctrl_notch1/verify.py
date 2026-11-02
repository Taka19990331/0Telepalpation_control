"""Runs production C++ with notch ON/OFF and replays recorded external torque."""
from pathlib import Path
import subprocess, os, csv, json, hashlib, sys
import numpy as np
r=Path(__file__).resolve().parent
w=r.parents[1]
out=r/'verification'; out.mkdir(exist_ok=True)
env=dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out/'zig_cache'), TEMP=str(out), TMP=str(out))
zig=r/'.tools/ziglang/zig.exe'
src=w/'controller_implementation/trans_ctrl_test1/data/trans_ctrl_test1_20261102_123759.csv'
with src.open() as f:
    rows=list(csv.DictReader(line for line in f if not line.startswith('#')))
t=np.array([float(x['timestamp_us']) for x in rows])*1e-6
x=np.array([float(x['external_torque_est_Nm']) for x in rows])
# Uniform 2 kHz grid, last recorded estimate held until the next sample.
grid=t[0]+np.arange(int((t[-1]-t[0])*2000)+1)/2000
idx=np.searchsorted(t,grid,side='right')-1
u=x[idx]; np.savetxt(out/'input.txt',u,fmt='%.9g')
logs=[]
def replay(values):
    z1=z2=0.; initialized=False; result=[]
    for value in values:
        x=float(np.float32(value))
        if not initialized: z1=(1-b0)*x; z2=(b2-a2)*x; initialized=True
        y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y
        result.append(np.float32(x+.95*(y-x)))
        assert np.isfinite([z1,z2,y]).all()
    return np.array(result)
if '--python-only' in sys.argv:
    omega=2*np.pi*12/2000; alpha=np.sin(omega)*4/24
    b0=1/(1+alpha); b1=-2*np.cos(omega)/(1+alpha); b2=b0; a1=b1; a2=(1-alpha)/(1+alpha)
    assert np.max(abs(replay(np.full(10000,2.))-2))<1e-6
    np.savetxt(out/'filtered_1.txt',replay(u))
    logs=['coeff '+' '.join(map(str,[b0,b1,b2,a1,a2]))+'\nPython reference only: C++ execution blocked by Windows Application Control (4551).\n']
for enabled in (1,0):
    if '--python-only' in sys.argv: break
    exe=out/f'verify_{enabled}.exe'
    subprocess.run([str(zig),'c++','-nostdlib++','-std=c++17','-O2',f'-DTRANS_ENABLE_FIXED_NOTCH={enabled}',str(r/'tests/verify.cpp'),'-o',str(exe)],check=True,env=env)
    result=subprocess.check_output([str(exe),str(out/'input.txt'),str(out/f'filtered_{enabled}.txt')],env=env,text=True)
    logs.append(result)
print(''.join(logs))
b0,b1,b2,a1,a2=map(float,logs[0].splitlines()[0].split()[1:])
def response(f):
    z=np.exp(-2j*np.pi*np.asarray(f)/2000)
    return .05+.95*(b0+b1*z+b2*z*z)/(1+a1*z+a2*z*z)
poles=np.roots([1,a1,a2]); assert max(abs(poles))<1
assert abs(abs(response(0))-1)<1e-10
assert abs(20*np.log10(abs(response(12)))+26.0206)<.001
y=np.loadtxt(out/'filtered_1.txt')
assert np.isfinite(y).all()
# Independent segmentation must carry identical state through control OFF.
warm=replay(u); assert np.max(abs(warm-y))<1e-6
enabled=(np.arange(len(u))//1000)%2==1
command=np.where(enabled,np.clip(np.float32(.75)*warm,-1,1),0)
assert np.array_equal(command[enabled],np.clip(np.float32(.75)*warm,-1,1)[enabled])
assert np.all(command[~enabled]==0)
baseline=np.float32(.75)*u.astype(np.float32)
bypass=np.float32(.75)*u.astype(np.float32)
assert np.array_equal(baseline,bypass)
# Compare exact 12 Hz projections and the integrated 10-14 Hz power after warmup.
sel=grid>=grid[0]+2
def spectrum(v):
    v=v[sel]; v=v-v.mean(); win=np.hanning(len(v))
    return np.fft.rfftfreq(len(v),1/2000), abs(np.fft.rfft(v*win))**2
f,px=spectrum(u); _,py=spectrum(y); band=(f>=10)&(f<=14)
stats=dict(source=str(src),source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),rows=len(rows),uniform_rows=len(u),
    recorded_interval_us=dict(min=float(np.diff(t).min()*1e6),max=float(np.diff(t).max()*1e6)),
    dc_gain=float(abs(response(0))),center_db=float(20*np.log10(abs(response(12)))),pole_radius=float(max(abs(poles))),
    band_10_14_power_db=float(10*np.log10(py[band].sum()/px[band].sum())),
    input_rms=float(np.sqrt(np.mean(u*u))),output_rms=float(np.sqrt(np.mean(y*y))),
    max_possible_OFF_ON_step_Nm=float(np.max(np.abs(np.clip(.75*y,-1,1)))),
    reference_checks='finite states, DC initial condition, warm OFF/ON, exact bypass arithmetic; production execution blocked')
np.savetxt(out/'offline.csv',np.column_stack([grid,u,y,.75*y,np.clip(.75*y,-1,1)]),delimiter=',',header='time_s,external_pre_Nm,external_post_Nm,torque_RFOB_if_ON_Nm,command_if_ON_Nm',comments='')
freq=np.linspace(0,100,2001)
np.savetxt(out/'response.csv',np.column_stack([freq,20*np.log10(abs(response(freq)))]),delimiter=',',header='frequency_Hz,gain_dB',comments='')
(out/'results.json').write_text(json.dumps(stats,indent=2))
(out/'host_tests.txt').write_text(''.join(logs))
print(json.dumps(stats,indent=2))

