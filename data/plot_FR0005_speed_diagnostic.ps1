param(
    [string]$CsvPath = (Join-Path $PSScriptRoot 'FR0005.csv'),
    [string]$OutputPath = (Join-Path $PSScriptRoot 'FR0005_speed_diagnostic.png')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$rows = @(Get-Content -LiteralPath $CsvPath | Select-Object -Skip 5 | ConvertFrom-Csv)
if ($rows.Count -lt 10) { throw 'CSV has too few data rows.' }
$eventIndex = 60693
$brakeIndex = 64693
$exitIndex = 80693
$event = $rows[$eventIndex]
$torqueBeforeEvent = [double]$rows[$eventIndex-1].target_torque_Nm
$brakeStart = $rows[$brakeIndex]
$exitRow = $rows[$exitIndex]
$brakeRows = @($rows | Where-Object { [int]$_.settling_status -eq 5 })
$lastWindow = @($brakeRows | Where-Object { [double]$_.actual_us -ge ([double]$exitRow.actual_us - 500000) })

$width = 1800; $height = 1280
$bitmap = [System.Drawing.Bitmap]::new($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::White)
$font = [System.Drawing.Font]::new('Segoe UI', 10)
$smallFont = [System.Drawing.Font]::new('Segoe UI', 8)
$titleFont = [System.Drawing.Font]::new('Segoe UI', 17, [System.Drawing.FontStyle]::Bold)
$axisPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(70,70,70), 1)
$gridPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225,230,235), 1)
$speedPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(23,105,170), 1.2)
$eventPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(224,122,0), 2)
$limitPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(178,58,72), 1.4)
$limitPen.DashPattern = [single[]]@(5,4)
$coastBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(28,242,193,78))
$brakeBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(25,224,122,0))
$bandBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(30,76,159,112))
$textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(40,40,40))
$orangeBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(224,122,0))
$blueBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(23,105,170))

function Map-X([double]$value, [double]$min, [double]$max, [System.Drawing.RectangleF]$plot) {
    return [single]($plot.Left + (($value-$min)/($max-$min))*$plot.Width)
}
function Map-Y([double]$value, [double]$min, [double]$max, [System.Drawing.RectangleF]$plot) {
    return [single]($plot.Bottom - (($value-$min)/($max-$min))*$plot.Height)
}
function Draw-Frame($g, [System.Drawing.RectangleF]$plot, [double]$xmin, [double]$xmax, [double]$ymin, [double]$ymax, [double[]]$xticks, [double[]]$yticks, [string]$title, [string]$xlabel, [string]$ylabel) {
    $g.DrawString($title, $font, $textBrush, $plot.Left, $plot.Top-28)
    foreach ($x in $xticks) {
        $px = Map-X $x $xmin $xmax $plot
        $g.DrawLine($gridPen, $px, $plot.Top, $px, $plot.Bottom)
        $label = if ($x -eq [math]::Floor($x)) { $x.ToString('0') } else { $x.ToString('0.0') }
        $g.DrawString($label, $smallFont, $textBrush, $px-12, $plot.Bottom+5)
    }
    foreach ($y in $yticks) {
        $py = Map-Y $y $ymin $ymax $plot
        $g.DrawLine($gridPen, $plot.Left, $py, $plot.Right, $py)
        $label = $y.ToString('0.##')
        $g.DrawString($label, $smallFont, $textBrush, $plot.Left-53, $py-8)
    }
    $g.DrawRectangle($axisPen, $plot.X, $plot.Y, $plot.Width, $plot.Height)
    $size = $g.MeasureString($xlabel, $smallFont)
    $g.DrawString($xlabel, $smallFont, $textBrush, $plot.Left+($plot.Width-$size.Width)/2, $plot.Bottom+24)
    $g.DrawString($ylabel, $smallFont, $textBrush, $plot.Left, $plot.Top-15)
}
function Draw-SpeedLine($g, [object[]]$data, [System.Drawing.RectangleF]$plot, [double]$xmin, [double]$xmax, [double]$ymin, [double]$ymax, [System.Drawing.Pen]$pen, [int]$first=0, [int]$last=-1) {
    if ($last -lt 0) { $last = $data.Count-1 }
    $pts = [System.Drawing.PointF[]]::new($last-$first+1)
    $j = 0
    for ($i=$first; $i -le $last; $i++) {
        $x = [double]$data[$i].actual_us/1000000.0
        $y = [double]$data[$i].speed_rad_s
        $pts[$j] = [System.Drawing.PointF]::new((Map-X $x $xmin $xmax $plot),(Map-Y $y $ymin $ymax $plot))
        $j++
    }
    if ($pts.Length -gt 1) { $g.DrawLines($pen, $pts) }
}

try {
    $graphics.DrawString('FR0005 speed diagnostic', $titleFont, $textBrush, 70, 22)
    $graphics.DrawString('Recorded feedback speed; thresholds and time regions follow the run settings.', $font, $textBrush, 72, 55)

    $overview = [System.Drawing.RectangleF]::new(125,115,1620,330)
    Draw-Frame $graphics $overview 0 40.5 -22 22 @(0,5,10,15,20,25,30,35,40) @(-20,-10,0,10,20) 'Complete run' 'Time from start [s]' 'Speed [rad/s]'
    foreach ($lim in @(-20,20)) { $y=Map-Y $lim -22 22 $overview; $graphics.DrawLine($limitPen,$overview.Left,$y,$overview.Right,$y) }
    Draw-SpeedLine $graphics $rows $overview 0 40.5 -22 22 $speedPen
    $eventTime = [double]$event.actual_us/1e6
    $ex = Map-X $eventTime 0 40.5 $overview
    $ey = Map-Y ([double]$event.speed_rad_s) -22 22 $overview
    $graphics.DrawLine($eventPen,$ex,$overview.Top,$ex,$overview.Bottom)
    $graphics.FillEllipse($orangeBrush,$ex-4,$ey-4,8,8)
    $graphics.DrawString(('Overspeed {0:N2} rad/s; torque {1:N2} -> 0 Nm' -f [double]$event.speed_rad_s,$torqueBeforeEvent),$smallFont,$orangeBrush,$ex+8,$ey-27)

    $response = [System.Drawing.RectangleF]::new(125,570,760,540)
    $t0 = $eventTime; $t1 = [double]$exitRow.actual_us/1e6
    Draw-Frame $graphics $response $t0 $t1 -22 22 @($t0,32,34,36,38,40) @(-20,-10,0,10,20) 'After overspeed / torque cut' 'Time from start [s]' 'Speed [rad/s]'
    $bx = Map-X ([double]$brakeStart.actual_us/1e6) $t0 $t1 $response
    $xx = Map-X ([double]$exitRow.actual_us/1e6) $t0 $t1 $response
    $graphics.FillRectangle($coastBrush,(Map-X $t0 $t0 $t1 $response),$response.Top,$bx-(Map-X $t0 $t0 $t1 $response),$response.Height)
    $graphics.FillRectangle($brakeBrush,$bx,$response.Top,$xx-$bx,$response.Height)
    foreach ($lim in @(-20,20)) { $y=Map-Y $lim -22 22 $response; $graphics.DrawLine($limitPen,$response.Left,$y,$response.Right,$y) }
    $y0=Map-Y 0 -22 22 $response
    $graphics.DrawLine($axisPen,$response.Left,$y0,$response.Right,$y0)
    Draw-SpeedLine $graphics $rows $response $t0 $t1 -22 22 $speedPen $eventIndex $exitIndex
    $graphics.DrawLine($eventPen,(Map-X $t0 $t0 $t1 $response),$response.Top,(Map-X $t0 $t0 $t1 $response),$response.Bottom)
    $graphics.DrawLine($axisPen,$bx,$response.Top,$bx,$response.Bottom)
    $graphics.DrawLine($limitPen,$xx,$response.Top,$xx,$response.Bottom)
    $graphics.DrawString('2 s coast',$smallFont,$textBrush,$bx+5,$response.Top+5)
    $graphics.DrawString('8 s brake timeout',$smallFont,$textBrush,$bx+12,$response.Top+23)

    $zoom = [System.Drawing.RectangleF]::new(970,570,775,245)
    $z0 = $t1-0.5; $z1 = $t1
    Draw-Frame $graphics $zoom $z0 $z1 -0.25 0.25 @($z0,($z0+0.25),$z1) @(-0.2,-0.1,-0.04,0,0.04,0.1,0.2) 'Final 0.5 s: near-zero samples' 'Time [s]' 'Speed [rad/s]'
    foreach ($lim in @(-0.04,0.04)) { $y=Map-Y $lim -0.25 0.25 $zoom; $graphics.DrawLine($limitPen,$zoom.Left,$y,$zoom.Right,$y) }
    $zoomPts = [System.Drawing.PointF[]]::new($lastWindow.Count)
    for ($i=0; $i -lt $lastWindow.Count; $i++) {
        $x=Map-X ([double]$lastWindow[$i].actual_us/1e6) $z0 $z1 $zoom
        $y=Map-Y ([double]$lastWindow[$i].speed_rad_s) -0.25 0.25 $zoom
        $zoomPts[$i]=[System.Drawing.PointF]::new($x,$y)
        $graphics.FillEllipse($blueBrush,$x-1.2,$y-1.2,2.4,2.4)
    }

    $hist = [System.Drawing.RectangleF]::new(970,890,775,220)
    Draw-Frame $graphics $hist -0.30 0.30 0 3200 @(-0.3,-0.2,-0.1,0,0.1,0.2,0.3) @(0,800,1600,2400,3200) 'Brake-state speed distribution (bin = 0.0025 rad/s)' 'Reported speed [rad/s]' 'Sample count'
    $binWidth=0.0025; $binCount=240; $counts=[int[]]::new($binCount)
    foreach ($r in $brakeRows) {
        $v=[double]$r.speed_rad_s
        $index=[int][math]::Floor(($v+0.30)/$binWidth)
        if ($index -ge 0 -and $index -lt $binCount) { $counts[$index]++ }
    }
    $maxCount=($counts | Measure-Object -Maximum).Maximum
    $barWidth=$hist.Width/$binCount
    for ($i=0; $i -lt $binCount; $i++) {
        if ($counts[$i] -eq 0) { continue }
        $x=$hist.Left+$i*$barWidth
        $barHeight=($counts[$i]/[double]$maxCount)*$hist.Height
        $graphics.FillRectangle($blueBrush,$x,$hist.Bottom-$barHeight,[math]::Max(1,$barWidth-0.3),$barHeight)
    }
    foreach ($lim in @(-0.04,0.04)) { $x=Map-X $lim -0.30 0.30 $hist; $graphics.DrawLine($limitPen,$x,$hist.Top,$x,$hist.Bottom) }
    $graphics.DrawString('Dashed lines: ±0.04 rad/s stop-confirmation threshold',$smallFont,$textBrush,$hist.Left,$hist.Bottom+29)

    $histCsvPath = Join-Path (Split-Path -Parent $OutputPath) 'FR0005_speed_histogram.csv'
    $invariant = [System.Globalization.CultureInfo]::InvariantCulture
    $histWriter = [System.IO.StreamWriter]::new($histCsvPath, $false, [System.Text.UTF8Encoding]::new($false))
    try {
        $histWriter.WriteLine('speed_bin_center_rad_s,bin_left_rad_s,bin_right_rad_s,count,within_abs_speed_0_04_rad_s')
        for ($i=0; $i -lt $binCount; $i++) {
            $left=-0.30+$i*$binWidth
            $right=$left+$binWidth
            $center=($left+$right)/2
            $withinBand=($left -ge -0.04 -and $right -le 0.04).ToString().ToLowerInvariant()
            $histWriter.WriteLine(('{0},{1},{2},{3},{4}' -f $center.ToString('F5',$invariant),$left.ToString('F5',$invariant),$right.ToString('F5',$invariant),$counts[$i],$withinBand))
        }
    } finally {
        $histWriter.Dispose()
    }

    $bitmap.Save($OutputPath,[System.Drawing.Imaging.ImageFormat]::Png)
    $stillRows=(@($brakeRows | Where-Object { [math]::Abs([double]$_.speed_rad_s) -le 0.04 })).Count
    $minV=($lastWindow | Measure-Object -Property speed_rad_s -Minimum).Minimum
    $maxV=($lastWindow | Measure-Object -Property speed_rad_s -Maximum).Maximum
    Write-Host "Saved: $OutputPath"
    Write-Host "Saved histogram data: $histCsvPath"
    Write-Host ("Overspeed at {0:N4} s: {1:N4} rad/s; torque {2:N2} Nm -> 0 Nm" -f $eventTime,[double]$event.speed_rad_s,$torqueBeforeEvent)
    Write-Host ("Brake samples within +/-0.04 rad/s: {0} / {1} (not necessarily consecutive)" -f $stillRows,$brakeRows.Count)
    Write-Host ("Final 0.5 s speed range: {0:N4} to {1:N4} rad/s" -f [double]$minV,[double]$maxV)
} finally {
    $graphics.Dispose(); $bitmap.Dispose()
    $font.Dispose(); $smallFont.Dispose(); $titleFont.Dispose()
    $axisPen.Dispose(); $gridPen.Dispose(); $speedPen.Dispose(); $eventPen.Dispose(); $limitPen.Dispose()
    $coastBrush.Dispose(); $brakeBrush.Dispose(); $bandBrush.Dispose(); $textBrush.Dispose(); $orangeBrush.Dispose(); $blueBrush.Dispose()
}
