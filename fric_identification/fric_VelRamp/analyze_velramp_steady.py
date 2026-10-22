"""Python equivalent of VelRamp_steady_analysis.m plus threshold sensitivity by speed band.

Requires numpy and Pillow. Defaults to the newest data/VelRamp_*.csv.
"""
import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FIELDS = ["timestamp_us", "command_rad_s", "position_rad", "velocity_rad_s",
          "torque_estimate_nm", "i_torque_command_nm", "stage", "error_code",
          "feedback_valid"]
GROUPS = [
    ("Positive acceleration", (0, 1), "#0072B2"),
    ("Positive return to zero", (2, 3), "#D55E00"),
    ("Negative acceleration", (4, 5), "#009E73"),
    ("Negative return to zero", (6, 7), "#CC79A7"),
]
BASE = dict(short_window_samples=10, max_short_range=0.04, exclude_after_step_s=0.030,
            min_continuous_s=0.030, trend_lookback_s=0.060,
            max_speed_slope=0.25, max_command_slope=0.25, enable_trend=True,
            command_step_jump=0.050, require_fresh_feedback=False)


def read_capture(path):
    meta, rows = {}, []
    with path.open(encoding="utf-8-sig", newline="") as f:
        for line in f:
            if line.startswith("#"):
                key, _, value = line[1:].rstrip().partition(",")
                meta[key] = value
            elif line.startswith("timestamp_us,"):
                break
        for r in csv.DictReader(f, fieldnames=FIELDS):
            if r["timestamp_us"]:
                rows.append([float(r[k]) for k in FIELDS])
    if not rows:
        raise ValueError(f"No sample rows found in {path}")
    return meta, np.asarray(rows, dtype=np.float64)


def moving_mean_trailing(values, window):
    finite = np.isfinite(values)
    vals = np.where(finite, values, 0.0)
    sums = np.r_[0.0, np.cumsum(vals)]
    counts = np.r_[0, np.cumsum(finite.astype(np.int64))]
    end = np.arange(1, len(values) + 1)
    start = np.maximum(0, end - window)
    total = sums[end] - sums[start]
    count = counts[end] - counts[start]
    result = np.full(len(values), np.nan)
    good = count > 0
    result[good] = total[good] / count[good]
    return result


def detect_steady(data, dt, settings):
    t_us, command, speed, torque, stage, feedback = (data[:, i] for i in (0, 1, 3, 4, 6, 8))
    t = (t_us - t_us[0]) * 1e-6
    diffs_t, diffs_u, diffs_stage = np.diff(t), np.diff(command), np.diff(stage)
    breaks = np.r_[True, (diffs_stage != 0) |
                   (np.abs(diffs_u) >= settings["command_step_jump"]) |
                   (~np.isfinite(diffs_u)) | (~np.isfinite(diffs_stage)) |
                   (~np.isfinite(diffs_t)) | (diffs_t <= 0) | (diffs_t > 0.05)]
    segment_id = np.cumsum(breaks) - 1
    starts = np.flatnonzero(breaks)
    ends = np.r_[starts[1:] - 1, len(t) - 1]

    valid = np.isfinite(t) & np.isfinite(speed) & np.isfinite(torque) & np.isfinite(command) & np.isfinite(stage)
    if settings["require_fresh_feedback"]:
        valid &= feedback == 1
    valid &= np.isin(stage, np.arange(8))
    steady = np.zeros(len(t), dtype=bool)
    short_range = np.full(len(t), np.nan)
    speed_slope = np.full(len(t), np.nan)
    command_slope = np.full(len(t), np.nan)
    win = int(settings["short_window_samples"])
    lag = max(1, round(settings["trend_lookback_s"] / dt))

    for start, end in zip(starts, ends):
        idx = np.arange(start, end + 1)
        n = len(idx)
        if n < win:
            continue
        sv, su, st = speed[idx], command[idx], t[idx]
        good = valid[idx]
        ranges = np.full(n, np.nan)
        ranges[win-1:] = (np.lib.stride_tricks.sliding_window_view(sv, win).max(axis=1) -
                          np.lib.stride_tricks.sliding_window_view(sv, win).min(axis=1))
        short_range[idx] = ranges
        valid_count = np.zeros(n, dtype=int)
        valid_count[win-1:] = np.lib.stride_tricks.sliding_window_view(good.astype(int), win).sum(axis=1)
        candidate = (np.isfinite(ranges) & (ranges <= settings["max_short_range"]) &
                     (valid_count == win) & ((st - st[0]) >= settings["exclude_after_step_s"]))

        if settings["enable_trend"]:
            if n <= lag:
                candidate[:] = False
            else:
                va, ua = moving_mean_trailing(sv, win), moving_mean_trailing(su, win)
                vsl, usl = np.full(n, np.nan), np.full(n, np.nan)
                dt_lag = st[lag:] - st[:-lag]
                ok = np.isfinite(dt_lag) & (dt_lag > 0)
                vdiff = np.abs(va[lag:] - va[:-lag])
                udiff = np.abs(ua[lag:] - ua[:-lag])
                vsl[lag:][ok] = vdiff[ok] / dt_lag[ok]
                usl[lag:][ok] = udiff[ok] / dt_lag[ok]
                speed_slope[idx], command_slope[idx] = vsl, usl
                candidate &= (vsl <= settings["max_speed_slope"]) & (usl <= settings["max_command_slope"])

        transitions = np.diff(np.r_[False, candidate, False].astype(np.int8))
        run_starts, run_ends = np.flatnonzero(transitions == 1), np.flatnonzero(transitions == -1) - 1
        for a, b in zip(run_starts, run_ends):
            if st[b] - st[a] + dt >= settings["min_continuous_s"]:
                steady[idx[a:b+1]] = True
    return dict(t=t, segment_id=segment_id, starts=starts, ends=ends, steady=steady,
                short_range=short_range, speed_slope=speed_slope,
                command_slope=command_slope, valid=valid)


def font(size, bold=False):
    for name in (("arialbd.ttf", "Arial Bold.ttf") if bold else ("arial.ttf", "Arial.ttf")):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def plot_speed_torque(path, data, result, meta, title, steady_only=False):
    W,H=1600,1120; img=Image.new("RGB",(W,H),"white"); draw=ImageDraw.Draw(img)
    ft,fs,fa,ff=font(31,True),font(18),font(21,True),font(16)
    n=len(data); speed, torque, stage=data[:,3],data[:,4],data[:,6].astype(int)
    draw.text((145,28),title,fill="#182230",font=ft)
    draw.text((148,76),f"AK80 | ID {meta.get('motor_id','?')} | {meta.get('sampling_hz','?')} Hz | {n:,} samples | result {meta.get('result_code','?')}",fill="#506070",font=fs)
    lx,ly=155,125
    for name,stages,color in GROUPS:
        draw.ellipse((lx,ly+5,lx+13,ly+18),fill=color); draw.text((lx+21,ly),name,fill="#263442",font=ff); lx+=330
    left,right,top,bottom=145,1530,180,930; xmin,xmax=-11.5,11.5; ymin,ymax=-.9,.9
    def pixel(x,y): return (left+(x-xmin)/(xmax-xmin)*(right-left),bottom-(y-ymin)/(ymax-ymin)*(bottom-top))
    for x in np.arange(-10,10.01,2.5):
        px,_=pixel(x,0); draw.line((px,top,px,bottom),fill="#94A0AC" if x==0 else "#E2E7EC",width=2 if x==0 else 1)
        lab=f"{x:g}"; b=draw.textbbox((0,0),lab,font=ff); draw.text((px-(b[2]-b[0])/2,bottom+14),lab,fill="#354454",font=ff)
    for y in np.arange(-.8,.801,.2):
        _,py=pixel(0,y); draw.line((left,py,right,py),fill="#94A0AC" if y==0 else "#E2E7EC",width=2 if y==0 else 1)
        lab=f"{y:.1f}"; b=draw.textbbox((0,0),lab,font=ff); draw.text((left-16-(b[2]-b[0]),py-(b[3]-b[1])/2),lab,fill="#354454",font=ff)
    for name,stages,color in GROUPS:
        mask=np.isin(stage,stages)&result["valid"]
        if steady_only: mask &= result["steady"]
        indices=np.flatnonzero(mask)
        layer=Image.new("RGBA",(W,H),(255,255,255,0)); ld=ImageDraw.Draw(layer)
        rgb=tuple(int(color[i:i+2],16) for i in (1,3,5))
        stride=1 if steady_only else 8
        for i in indices[::stride]:
            px,py=pixel(speed[i],torque[i]); px,py=int(round(px)),int(round(py))
            if left<=px<=right and top<=py<=bottom: ld.ellipse((px-1,py-1,px+1,py+1),fill=(*rgb,55 if steady_only else 45))
        img=Image.alpha_composite(img.convert("RGBA"),layer).convert("RGB"); draw=ImageDraw.Draw(img)
        edges=np.arange(xmin,xmax+.25001,.25); bins=np.digitize(speed[indices],edges)-1; means=[]
        for b in range(len(edges)-1):
            chosen=bins==b
            if chosen.sum()>= (10 if steady_only else 20):
                means.append((float(speed[indices][chosen].mean()),float(torque[indices][chosen].mean())))
        if len(means)>1: draw.line([pixel(x,y) for x,y in means],fill=color,width=3)
    draw.rectangle((left,top,right,bottom),outline="#647280",width=2)
    label="Measured motor speed (rad/s)"; b=draw.textbbox((0,0),label,font=fa); draw.text(((left+right-(b[2]-b[0]))/2,bottom+56),label,fill="#182230",font=fa)
    ylab=Image.new("RGBA",(500,48),(255,255,255,0)); ImageDraw.Draw(ylab).text((0,0),"Motor torque estimate (Nm)",fill="#182230",font=fa); ylab=ylab.rotate(90,expand=True); img.paste(ylab,(28,int((top+bottom-ylab.height)/2)),ylab)
    draw=ImageDraw.Draw(img); draw.text((left,1018),"Dots: every 8th raw sample (or all steady samples) | Lines: 0.25 rad/s bin means",fill="#506070",font=fs)
    img.save(path,dpi=(180,180),optimize=True)


def plot_diagnostic(path, data, result):
    W,H=1600,1120; img=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(img)
    title_font,axis_font,tick_font=font(29,True),font(18,True),font(14)
    d.text((75,25),'Steady-state detector diagnostics',fill='#182230',font=title_font)
    t=result['t']; u=data[:,1]; v=data[:,3]; keep=result['steady']; n=len(t)
    stride=max(1,n//18000); idx=np.arange(0,n,stride)
    left,right=145,1530; panels=[(120,400, -11,11,'Command and measured speed (rad/s)'),
                                (485,690, 0,.8,'Trailing 10-sample speed range (rad/s)'),
                                (775,980, 0,12,'60 ms speed / command slope (rad/s^2)')]
    for p,(top,bottom,ymin,ymax,label) in enumerate(panels):
        def xy(tx,ty):
            px=left+(tx/max(t[-1],1e-9))*(right-left)
            py=bottom-(ty-ymin)/(ymax-ymin)*(bottom-top)
            return px,py
        for x in np.arange(0,math.ceil(t[-1]/10)*10+0.1,10):
            px,_=xy(x,ymin); d.line((px,top,px,bottom),fill='#E3E8ED',width=1)
            d.text((px-12,bottom+3),str(int(x)),fill='#566575',font=tick_font)
        for y in np.linspace(ymin,ymax,5):
            _,py=xy(0,y); d.line((left,py,right,py),fill='#E3E8ED',width=1)
            d.text((left-48,py-8),f'{y:g}',fill='#566575',font=tick_font)
        if p==0:
            cmdpts=[xy(t[i],u[i]) for i in idx if np.isfinite(u[i])]
            velpts=[xy(t[i],v[i]) for i in idx if np.isfinite(v[i])]
            if len(cmdpts)>1: d.line(cmdpts,fill='#34495E',width=2)
            if len(velpts)>1: d.line(velpts,fill='#0072B2',width=2)
            steady_idx=np.flatnonzero(keep)[::max(1,int(keep.sum()/2500) if keep.sum() else 1)]
            for i in steady_idx:
                px,py=xy(t[i],v[i]); d.ellipse((px-2,py-2,px+2,py+2),fill='#D55E00')
            d.text((right-500,top+4),'Command',fill='#34495E',font=tick_font)
            d.text((right-400,top+4),'Measured',fill='#0072B2',font=tick_font)
            d.text((right-290,top+4),'Accepted steady',fill='#D55E00',font=tick_font)
        elif p==1:
            pts=[xy(t[i],result['short_range'][i]) for i in idx if np.isfinite(result['short_range'][i])]
            if len(pts)>1: d.line(pts,fill='#0072B2',width=2)
            threshold=BASE['max_short_range']; py=xy(0,threshold)[1]
            d.line((left,py,right,py),fill='#D55E00',width=2)
            d.text((right-205,py-22),f'baseline threshold {threshold:.2f}',fill='#D55E00',font=tick_font)
        else:
            for values,color in ((result['speed_slope'],'#009E73'),(result['command_slope'],'#CC79A7')):
                pts=[xy(t[i],values[i]) for i in idx if np.isfinite(values[i])]
                if len(pts)>1: d.line(pts,fill=color,width=2)
            threshold=BASE['max_speed_slope']; py=xy(0,threshold)[1]
            d.line((left,py,right,py),fill='#D55E00',width=2)
            d.text((right-190,py-22),f'baseline {threshold:.2f}',fill='#D55E00',font=tick_font)
        d.rectangle((left,top,right,bottom),outline='#71808D',width=1)
        d.text((left,top-25),label,fill='#182230',font=axis_font)
    d.text((left,1020),'Elapsed time (s) | accepted steady points are orange in the top panel',fill='#506070',font=font(17))
    img.save(path,dpi=(180,180),optimize=True)


def export_steady_csv(path, data, result):
    with path.open("w",encoding="utf-8",newline="") as f:
        w=csv.writer(f); w.writerow(FIELDS+['segment_id','steady','short_range_rad_s','speed_slope_rad_s2','command_slope_rad_s2'])
        extras=np.c_[result['segment_id'],result['steady'].astype(int),result['short_range'],result['speed_slope'],result['command_slope']]
        for row,extra in zip(data,extras): w.writerow([*row.tolist(),*extra.tolist()])


def export_step_summary(path, data, result, dt):
    t,u,v,tau,stage=(data[:,i] for i in (0,1,3,4,6)); t=(t-t[0])*1e-6
    headers=['segment_id','stage','command_rad_s','start_time_s','segment_samples','steady_samples','steady_duration_s','mean_velocity_rad_s','std_velocity_rad_s','mean_torque_nm','std_torque_nm','has_steady']
    with path.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f); w.writerow(headers)
        for sid,(a,b) in enumerate(zip(result['starts'],result['ends'])):
            idx=np.arange(a,b+1); sel=idx[result['steady'][idx]]
            vals=[sid,int(stage[a]),float(np.nanmean(u[idx])),float(t[a]),len(idx),len(sel),len(sel)*dt]
            if len(sel): vals += [float(np.nanmean(v[sel])),float(np.nanstd(v[sel],ddof=1)),float(np.nanmean(tau[sel])),float(np.nanstd(tau[sel],ddof=1)),True]
            else: vals += [float('nan')]*4+[False]
            w.writerow(vals)


def speed_band_counts(speed, steady, bins=np.arange(0,12,1.0)):
    out=[]
    for polarity in (-1,1):
        for lo,hi in zip(bins[:-1],bins[1:]):
            mask=(speed*polarity>=lo)&(speed*polarity<hi)
            total=int(mask.sum()); kept=int((mask&steady).sum())
            out.append(("negative" if polarity<0 else "positive",float(lo),float(hi),total,kept,100*kept/total if total else 0.0))
    return out


def sensitivity_scenarios():
    return [
        ("MATLAB baseline", {}),
        ("Range 0.20 only", {"max_short_range": .20}),
        ("Slope 5 only", {"max_speed_slope": 5, "max_command_slope": 5}),
        ("Duration 5 ms only", {"min_continuous_s": .005}),
        ("Moderate combined", {"max_short_range": .30, "max_speed_slope": 5,
                                "max_command_slope": 5}),
        ("Moderate combined, 20 ms", {"max_short_range": .30, "max_speed_slope": 5,
                                      "max_command_slope": 5, "min_continuous_s": .020,
                                      "exclude_after_step_s": .020}),
        ("Range 0.50, slope 2", {"max_short_range": .50, "max_speed_slope": 2,
                                 "max_command_slope": 2}),
        ("Range 0.50, slope 5", {"max_short_range": .50, "max_speed_slope": 5,
                                 "max_command_slope": 5}),
        ("Relaxed combined", {"max_short_range": .50, "max_speed_slope": 5,
                               "max_command_slope": 5, "min_continuous_s": .010,
                               "exclude_after_step_s": .010}),
        ("Aggressive combined", {"max_short_range": .50, "max_speed_slope": 10,
                                  "max_command_slope": 10, "min_continuous_s": .005,
                                  "exclude_after_step_s": 0}),
        ("Very relaxed combined", {"max_short_range": 1.0, "max_speed_slope": 10,
                                    "max_command_slope": 10, "min_continuous_s": .010,
                                    "exclude_after_step_s": 0}),
        ("Range 0.50, no trend", {"max_short_range": .50, "enable_trend": False}),
        ("Range 0.50, no trend, 10 ms", {"max_short_range": .50, "enable_trend": False,
                                          "min_continuous_s": .010,
                                          "exclude_after_step_s": .010}),
    ]


def draw_sensitivity(path, scenarios, results):
    W,H=1850,1180; img=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(img)
    ft,fs,ff=font(29,True),font(17),font(15)
    d.text((70,28),'Steady-sample sensitivity by measured-speed band',fill='#182230',font=ft)
    d.text((74,72),'Accepted samples (%) within each absolute measured-speed band',fill='#506070',font=fs)
    # Scenario counts, sorted as configured.
    names=[s[0] for s in scenarios]; total=results[0]['n']; top=130; rowh=32; barx=340; barw=1250
    d.text((70,105),f"Total capture samples: {total:,}",fill='#344454',font=fs)
    for i,name in enumerate(names):
        y=top+i*rowh; count=results[i]['count']; pct=100*count/total
        d.text((74,y+4),name,fill='#273746',font=ff)
        d.rectangle((barx,y+4,barx+barw,y+23),fill='#E7ECF0')
        d.rectangle((barx,y+4,barx+barw*pct/100,y+23),fill='#2878B5' if i==0 else '#66A5CA')
        d.text((barx+barw+15,y+3),f'{count:,} ({pct:.1f}%)',fill='#273746',font=ff)
    selected=[0,4,6,7,8,10,12]
    palette=['#34495E','#0072B2','#56B4E9','#D55E00','#E69F00','#CC79A7','#009E73']
    plot_top=top+len(names)*rowh+125; left,right=175,1760; plot_bottom=1120
    # Put one shared legend above the two speed-polarity panels.
    for k,(si,color) in enumerate(zip(selected,palette)):
        row,col=divmod(k,4); lx=left+col*395; ly=plot_top-58+row*22
        label=names[si]
        d.line((lx,ly+10,lx+25,ly+10),fill=color,width=3)
        d.text((lx+31,ly),label,fill='#344454',font=ff)
    # Percentage coverage by absolute-speed bin, separate polarity panels.
    panel_h=(plot_bottom-plot_top-70)/2
    for p,polarity in enumerate(('positive','negative')):
        y0=plot_top+p*(panel_h+72); y1=y0+panel_h
        d.text((left,y0+3),polarity.title(),fill='#182230',font=font(17,True))
        x0,x1=left,right; bins=np.arange(0,12,1.0)
        for pct in range(0,101,20):
            py=y1-pct/100*(y1-y0-28); d.line((x0,py,x1,py),fill='#E1E6EA',width=1); d.text((x0-55,py-9),f'{pct}%',fill='#566575',font=ff)
        for b in range(11):
            px=x0+(b+.5)/11*(x1-x0); lab=f'{b}–{b+1}'
            box=d.textbbox((0,0),lab,font=ff); d.text((px-(box[2]-box[0])/2,y1+7),lab,fill='#566575',font=ff)
        for si,color in zip(selected,palette):
            band=results[si]['bands']; vals=[next((z[5] for z in band if z[0]==polarity and z[1]==float(b)),0.0) for b in range(11)]
            points=[]
            for b,val in enumerate(vals):
                px=x0+(b+.5)/11*(x1-x0); py=y1-val/100*(y1-y0-28); points.append((px,py))
            d.line(points,fill=color,width=3)
            for px,py in points: d.ellipse((px-3,py-3,px+3,py+3),fill=color)
        d.text((x0,y1+35),'Absolute measured-speed band (rad/s)',fill='#182230',font=ff)
    img.save(path,dpi=(180,180),optimize=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',nargs='?',type=Path,help='capture CSV; defaults to newest VelRamp_*.csv in data/')
    parser.add_argument('--output-dir',type=Path,help='defaults to a results/ folder beside this script')
    args=parser.parse_args()
    data_dir=Path(__file__).resolve().parent/'data'
    csv_path=args.csv
    if csv_path is None:
        candidates=list(data_dir.glob('VelRamp_*.csv'))
        if not candidates: parser.error(f'No VelRamp CSV files found in {data_dir}')
        csv_path=max(candidates,key=lambda p:p.stat().st_mtime)
    out_dir=args.output_dir or Path(__file__).resolve().parent/'results'
    out_dir.mkdir(parents=True,exist_ok=True)
    meta,data=read_capture(csv_path)
    t_us=data[:,0]; dt_values=np.diff(t_us)*1e-6; dt_values=dt_values[(dt_values>0)&(dt_values<.1)]
    if not len(dt_values): raise ValueError('No valid positive timestamp intervals')
    dt=float(np.median(dt_values)); fs=1/dt
    print(f'Input: {csv_path}\nSamples: {len(data):,} | fs: {fs:.2f} Hz | duration: {(t_us[-1]-t_us[0])*1e-6:.3f} s')
    baseline=detect_steady(data,dt,BASE)
    n=len(data); count=int(baseline['steady'].sum())
    print(f"Baseline steady: {count:,}/{n:,} ({100*count/n:.2f}%)")
    stem=csv_path.stem
    steady_csv=out_dir/f'{stem}_steady.csv'; summary_csv=out_dir/f'{stem}_steady_summary.csv'
    plot_speed_torque(out_dir/f'{stem}_raw_torque_vs_speed.png',data,baseline,meta,'VelRamp RAW: measured speed vs torque estimate')
    plot_speed_torque(out_dir/f'{stem}_steady_torque_vs_speed.png',data,baseline,meta,f'VelRamp STEADY: {count:,} / {n:,} samples',True)
    export_steady_csv(steady_csv,data,baseline); export_step_summary(summary_csv,data,baseline,dt)

    scenarios=sensitivity_scenarios(); results=[]; bands=np.arange(0,12,1.0)
    for name,changes in scenarios:
        cfg=BASE.copy(); cfg.update(changes)
        detected=baseline if not changes else detect_steady(data,dt,cfg)
        keep=detected['steady']; selected=int(keep.sum())
        results.append(dict(n=n,count=selected,pct=100*selected/n,bands=speed_band_counts(data[:,3],keep,bands)))
    sweep_csv=out_dir/f'{stem}_steady_threshold_sweep.csv'
    with sweep_csv.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f); w.writerow(['scenario','steady_samples','total_samples','steady_percent','settings'])
        for (name,changes),result in zip(scenarios,results):
            config=BASE.copy(); config.update(changes)
            w.writerow([name,result['count'],n,f"{result['pct']:.6f}",';'.join(f'{k}={v}' for k,v in config.items())])
    band_csv=out_dir/f'{stem}_steady_by_speed_band.csv'
    with band_csv.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f); w.writerow(['scenario','polarity','abs_speed_min_rad_s','abs_speed_max_rad_s','all_samples','steady_samples','steady_percent'])
        for (name,_),result in zip(scenarios,results):
            for polarity,lo,hi,total,kept,pct in result['bands']:
                w.writerow([name,polarity,lo,hi,total,kept,f'{pct:.6f}'])
    sensitivity_png=out_dir/f'{stem}_steady_sensitivity.png'
    draw_sensitivity(sensitivity_png,scenarios,results)
    diag_png=out_dir/f'{stem}_steady_diagnostic.png'
    plot_diagnostic(diag_png,data,baseline)
    # Diagnostic series saved as a compact numeric CSV for easy plotting in other tools.
    diag_csv=out_dir/f'{stem}_steady_diagnostic.csv'
    with diag_csv.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f); w.writerow(['timestamp_s','command_rad_s','velocity_rad_s','steady','short_range_rad_s','speed_slope_rad_s2','command_slope_rad_s2'])
        for i in range(n): w.writerow([baseline['t'][i],data[i,1],data[i,3],int(baseline['steady'][i]),baseline['short_range'][i],baseline['speed_slope'][i],baseline['command_slope'][i]])
    print('\nSensitivity (overall):')
    for (name,_),r in zip(scenarios,results): print(f"  {name:28s} {r['count']:7,d} ({r['pct']:6.2f}%)")
    print('\nBy speed band: see',band_csv)
    print('Outputs:',steady_csv,summary_csv,sweep_csv,band_csv,sensitivity_png,sep='\n  ')

if __name__=='__main__': main()
