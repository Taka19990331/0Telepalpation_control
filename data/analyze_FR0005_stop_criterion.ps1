param(
    [string]$CsvPath = (Join-Path $PSScriptRoot 'FR0005.csv'),
    [string]$OutputPath = (Join-Path $PSScriptRoot 'FR0005_stop_criterion_rolling_1s.png')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$rows = @(Get-Content -LiteralPath $CsvPath | Select-Object -Skip 5 | ConvertFrom-Csv)
$triggerIndex = 60693
$windowSamples = 2001 # 2001 samples at 2 kHz span exactly 1.000 s, endpoint to endpoint.
$periodUs = 500.0
$triggerUs = [double]$rows[$triggerIndex].actual_us
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$count = $rows.Count - $triggerIndex
$rawPos = [double[]]::new($count)
$position = [double[]]::new($count)
$speed = [double[]]::new($count)
$time = [double[]]::new($count)
$lastRaw = 0.0
$unwrapped = 0.0
for ($i=0; $i -lt $count; $i++) {
    $row = $rows[$triggerIndex+$i]
    $raw = [double]::Parse($row.position_deg,$invariant)*[math]::PI/180.0
    if ($i -eq 0) { $unwrapped=$raw }
    else {
        $delta=$raw-$lastRaw
        if ($delta -gt 12.5) { $delta-=25.0 }
        elseif ($delta -lt -12.5) { $delta+=25.0 }
        $unwrapped+=$delta
    }
    $rawPos[$i]=$raw
    $position[$i]=$unwrapped
    $speed[$i]=[double]::Parse($row.speed_rad_s,$invariant)
    $time[$i]=([double]$row.actual_us-$triggerUs)/1e6
    $lastRaw=$raw
}

# Monotonic index queues yield each rolling position max/min in O(n).
$minQueue=[int[]]::new($count); $maxQueue=[int[]]::new($count)
$minHead=0; $minTail=0; $maxHead=0; $maxTail=0
$rolling=[System.Collections.Generic.List[object]]::new()
$speedSum=0.0
$firstPassing=$null
for ($i=0; $i -lt $count; $i++) {
    while ($minTail -gt $minHead -and $position[$i] -le $position[$minQueue[$minTail-1]]) { $minTail-- }
    $minQueue[$minTail]=$i; $minTail++
    while ($maxTail -gt $maxHead -and $position[$i] -ge $position[$maxQueue[$maxTail-1]]) { $maxTail-- }
    $maxQueue[$maxTail]=$i; $maxTail++
    $speedSum+=$speed[$i]
    $lower=$i-$windowSamples+1
    if ($lower -gt 0) { $speedSum-=$speed[$lower-1] }
    while ($minHead -lt $minTail -and $minQueue[$minHead] -lt $lower) { $minHead++ }
    while ($maxHead -lt $maxTail -and $maxQueue[$maxHead] -lt $lower) { $maxHead++ }
    if ($i -lt $windowSamples-1) { continue }
    $width=$position[$maxQueue[$maxHead]]-$position[$minQueue[$minHead]]
    $mean=$speedSum/$windowSamples
    $elapsed=$time[$i]
    $pass=($width -lt 0.003 -and [math]::Abs($mean) -lt 0.03)
    $point=[pscustomobject]@{Elapsed_s=$elapsed;WindowStart_s=$elapsed-1.0;PositionRange_rad=$width;MeanSpeed_rad_s=$mean;Pass=$pass}
    $rolling.Add($point)
    if ($null -eq $firstPassing -and $pass) { $firstPassing=$point }
}

$bitmap=[System.Drawing.Bitmap]::new(1500,1000)
$graphics=[System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::White)
$font=[System.Drawing.Font]::new('Segoe UI',10)
$small=[System.Drawing.Font]::new('Segoe UI',9)
$title=[System.Drawing.Font]::new('Segoe UI',17,[System.Drawing.FontStyle]::Bold)
$gridPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225,230,235),1)
$axisPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(70,70,70),1)
$linePen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(23,105,170),1.3)
$thresholdPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(178,58,72),1.5)
$thresholdPen.DashPattern=[single[]]@(5,4)
$eventPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(224,122,0),1.5)
$blue=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(23,105,170))
$orange=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(224,122,0))
$text=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(40,40,40))
function Map-LogY([double]$v,[double]$lo,[double]$hi,[System.Drawing.RectangleF]$r) {
    $floor=1e-7
    $v=[math]::Max($v,$floor)
    $a=[math]::Log10($lo); $b=[math]::Log10($hi); $z=[math]::Log10($v)
    [single]($r.Bottom-($z-$a)/($b-$a)*$r.Height)
}
function Map-TimeX([double]$v,[double]$xmax,[System.Drawing.RectangleF]$r) { [single]($r.Left+$v/$xmax*$r.Width) }
function Draw-LogFrame($g,[System.Drawing.RectangleF]$r,[double]$xmax,[double]$ymin,[double]$ymax,[double[]]$yticks,[string]$panelTitle,[string]$thresholdText) {
    $g.DrawString($panelTitle,$font,$text,$r.Left,$r.Top-28)
    foreach ($x in @(0,1,2,3,4,5,6,7,8,9,10)) {
        $px=Map-TimeX $x $xmax $r
        $g.DrawLine($gridPen,$px,$r.Top,$px,$r.Bottom)
        $g.DrawString($x.ToString('0'),$small,$text,$px-4,$r.Bottom+5)
    }
    foreach ($y in $yticks) {
        $py=Map-LogY $y $ymin $ymax $r
        $g.DrawLine($gridPen,$r.Left,$py,$r.Right,$py)
        $g.DrawString($y.ToString('G2'),$small,$text,$r.Left-48,$py-8)
    }
    $g.DrawRectangle($axisPen,$r.X,$r.Y,$r.Width,$r.Height)
    $g.DrawString('Elapsed since overspeed / torque cut [s]',$small,$text,$r.Left+$r.Width/2-105,$r.Bottom+25)
    $g.DrawString($thresholdText,$small,$text,$r.Right-250,$r.Top+4)
}
try {
    $graphics.DrawString('FR0005 rolling 1.0 s stop criterion',$title,$text,65,20)
    $graphics.DrawString('Window uses 2001 samples (1.000 s endpoint span); position is unwrapped across the 25 rad encoder boundary.',$font,$text,67,53)
    $xmax=[double]$time[$count-1]
    $rangeRect=[System.Drawing.RectangleF]::new(130,145,1285,300)
    Draw-LogFrame $graphics $rangeRect $xmax 1e-5 30 @(1e-5,1e-4,1e-3,0.003,0.01,0.1,1,10) 'Rolling position range: max(position) - min(position) [rad, log scale]' 'Threshold: 0.003 rad'
    $rangeThreshold=Map-LogY 0.003 1e-5 30 $rangeRect
    $graphics.DrawLine($thresholdPen,$rangeRect.Left,$rangeThreshold,$rangeRect.Right,$rangeThreshold)
    $rangePoints=[System.Drawing.PointF[]]::new($rolling.Count)
    for ($i=0;$i -lt $rolling.Count;$i++) {
        $p=$rolling[$i]
        $rangePoints[$i]=[System.Drawing.PointF]::new((Map-TimeX $p.Elapsed_s $xmax $rangeRect),(Map-LogY $p.PositionRange_rad 1e-5 30 $rangeRect))
    }
    $graphics.DrawLines($linePen,$rangePoints)

    $meanRect=[System.Drawing.RectangleF]::new(130,600,1285,300)
    Draw-LogFrame $graphics $meanRect $xmax 1e-4 30 @(1e-4,1e-3,0.01,0.03,0.1,1,10) 'Absolute 1.0 s mean speed: abs(mean(speed)) [rad/s, log scale]' 'Threshold: 0.03 rad/s'
    $meanThreshold=Map-LogY 0.03 1e-4 30 $meanRect
    $graphics.DrawLine($thresholdPen,$meanRect.Left,$meanThreshold,$meanRect.Right,$meanThreshold)
    $meanPoints=[System.Drawing.PointF[]]::new($rolling.Count)
    for ($i=0;$i -lt $rolling.Count;$i++) {
        $p=$rolling[$i]
        $meanPoints[$i]=[System.Drawing.PointF]::new((Map-TimeX $p.Elapsed_s $xmax $meanRect),(Map-LogY ([math]::Abs($p.MeanSpeed_rad_s)) 1e-4 30 $meanRect))
    }
    $graphics.DrawLines($linePen,$meanPoints)
    if ($null -ne $firstPassing) {
        $passX=Map-TimeX $firstPassing.Elapsed_s $xmax $rangeRect
        $graphics.DrawLine($eventPen,$passX,$rangeRect.Top,$passX,$rangeRect.Bottom)
        $passX2=Map-TimeX $firstPassing.Elapsed_s $xmax $meanRect
        $graphics.DrawLine($eventPen,$passX2,$meanRect.Top,$passX2,$meanRect.Bottom)
        $graphics.FillEllipse($orange,$passX-4,(Map-LogY $firstPassing.PositionRange_rad 1e-5 30 $rangeRect)-4,8,8)
        $graphics.FillEllipse($orange,$passX2-4,(Map-LogY ([math]::Abs($firstPassing.MeanSpeed_rad_s)) 1e-4 30 $meanRect)-4,8,8)
        $graphics.DrawString(('First pass at +{0:F3} s' -f $firstPassing.Elapsed_s),$font,$orange,$passX+7,$rangeRect.Top+12)
    }
    $bitmap.Save($OutputPath,[System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Saved: $OutputPath"
    Write-Host "Rolling windows: $($rolling.Count); samples/window: $windowSamples"
    if ($null -eq $firstPassing) {
        Write-Host 'No rolling window met both thresholds.'
        Write-Host ("Minimum position range={0:G6} rad; minimum absolute mean speed={1:G6} rad/s" -f (($rolling | Measure-Object PositionRange_rad -Minimum).Minimum),(($rolling | ForEach-Object {[math]::Abs($_.MeanSpeed_rad_s)} | Measure-Object -Minimum).Minimum))
    } else {
        $triggerTime=$triggerUs/1e6
        $absoluteTime=$triggerTime+$firstPassing.Elapsed_s
        Write-Host ("First passing window ends at run t={0:F4} s (index time from overspeed={1:F4} s)" -f $absoluteTime,$firstPassing.Elapsed_s)
        Write-Host ("Window start={0:F4} s, end={1:F4} s after overspeed; position range={2:G6} rad; mean speed={3:G6} rad/s" -f $firstPassing.WindowStart_s,$firstPassing.Elapsed_s,$firstPassing.PositionRange_rad,$firstPassing.MeanSpeed_rad_s)
        Write-Host ("Elapsed from overspeed detection to condition: {0:F4} s" -f $firstPassing.Elapsed_s)
        $fromFirst=@($rolling | Where-Object { $_.Elapsed_s -ge $firstPassing.Elapsed_s })
        $failedAfter=@($fromFirst | Where-Object { -not $_.Pass })
        Write-Host ("Windows meeting both conditions from first pass to log end: {0}/{1}" -f ($fromFirst.Count-$failedAfter.Count),$fromFirst.Count)
    }
} finally {
    $graphics.Dispose();$bitmap.Dispose();$font.Dispose();$small.Dispose();$title.Dispose()
    $gridPen.Dispose();$axisPen.Dispose();$linePen.Dispose();$thresholdPen.Dispose();$eventPen.Dispose();$blue.Dispose();$orange.Dispose();$text.Dispose()
}
