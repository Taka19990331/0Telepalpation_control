param(
    [string]$CsvPath = (Join-Path $PSScriptRoot 'FR0005.csv'),
    [string]$OutputCsv = (Join-Path $PSScriptRoot 'FR0005_speed_noise_0p5s.csv'),
    [string]$OutputPng = (Join-Path $PSScriptRoot 'FR0005_speed_noise_0p5s.png')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$allRows = @(Get-Content -LiteralPath $CsvPath | Select-Object -Skip 5 | ConvertFrom-Csv)
$brakeRows = @($allRows | Where-Object { [int]$_.settling_status -eq 5 })
if ($brakeRows.Count -ne 16000) { throw "Expected 16000 braking samples; found $($brakeRows.Count)." }
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$stats = [System.Collections.Generic.List[object]]::new()
$globalSum = 0.0
$globalSquareSum = 0.0
foreach ($row in $brakeRows) {
    $value = [double]::Parse($row.speed_rad_s,$invariant)
    $globalSum += $value
    $globalSquareSum += $value*$value
}
$globalMean = $globalSum/$brakeRows.Count
$globalStd = [math]::Sqrt(($globalSquareSum-$brakeRows.Count*$globalMean*$globalMean)/($brakeRows.Count-1))

for ($window=0; $window -lt 16; $window++) {
    $sum=0.0; $min=[double]::PositiveInfinity; $max=[double]::NegativeInfinity
    $values=[double[]]::new(1000)
    for ($j=0; $j -lt 1000; $j++) {
        $value=[double]::Parse($brakeRows[$window*1000+$j].speed_rad_s,$invariant)
        $values[$j]=$value; $sum+=$value
        if ($value -lt $min) { $min=$value }
        if ($value -gt $max) { $max=$value }
    }
    $mean=$sum/1000
    $sumSquares=0.0
    foreach ($value in $values) { $d=$value-$mean; $sumSquares+=$d*$d }
    $std=[math]::Sqrt($sumSquares/999)
    $start=$window*0.5
    $stats.Add([pscustomobject]@{
        window_index=$window
        elapsed_start_s=$start
        elapsed_end_s=$start+0.5
        elapsed_center_s=$start+0.25
        n=1000
        mean_speed_rad_s=$mean
        sample_std_speed_rad_s=$std
        min_speed_rad_s=$min
        max_speed_rad_s=$max
    })
}

$csvWriter=[System.IO.StreamWriter]::new($OutputCsv,$false,[System.Text.UTF8Encoding]::new($false))
try {
    $csvWriter.WriteLine('window_index,elapsed_start_s,elapsed_end_s,elapsed_center_s,n,mean_speed_rad_s,sample_std_speed_rad_s,min_speed_rad_s,max_speed_rad_s')
    foreach ($s in $stats) {
        $csvWriter.WriteLine(('{0},{1:F2},{2:F2},{3:F2},{4},{5:F8},{6:F8},{7:F8},{8:F8}' -f $s.window_index,$s.elapsed_start_s,$s.elapsed_end_s,$s.elapsed_center_s,$s.n,$s.mean_speed_rad_s,$s.sample_std_speed_rad_s,$s.min_speed_rad_s,$s.max_speed_rad_s))
    }
} finally { $csvWriter.Dispose() }

$bitmap=[System.Drawing.Bitmap]::new(1400,900)
$graphics=[System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::White)
$font=[System.Drawing.Font]::new('Segoe UI',10)
$small=[System.Drawing.Font]::new('Segoe UI',9)
$title=[System.Drawing.Font]::new('Segoe UI',17,[System.Drawing.FontStyle]::Bold)
$gridPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225,230,235),1)
$axisPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(70,70,70),1)
$linePen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(23,105,170),2)
$globalPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(178,58,72),1.3)
$globalPen.DashPattern=[single[]]@(5,4)
$blue=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(23,105,170))
$text=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(40,40,40))
function PX([double]$x,[System.Drawing.RectangleF]$r) { [single]($r.Left+$x/8.0*$r.Width) }
function PY([double]$y,[double]$lo,[double]$hi,[System.Drawing.RectangleF]$r) { [single]($r.Bottom-($y-$lo)/($hi-$lo)*$r.Height) }
function Draw-StatChart($g,[System.Drawing.RectangleF]$r,[string]$titleText,[string]$prop,[double]$lo,[double]$hi,[double[]]$ticks,[double]$baseline) {
    $g.DrawString($titleText,$font,$text,$r.Left,$r.Top-28)
    foreach ($t in @(0,2,4,6,8)) { $x=PX $t $r; $g.DrawLine($gridPen,$x,$r.Top,$x,$r.Bottom); $g.DrawString($t.ToString('0.0'),$small,$text,$x-12,$r.Bottom+5) }
    foreach ($t in $ticks) { $y=PY $t $lo $hi $r; $g.DrawLine($gridPen,$r.Left,$y,$r.Right,$y); $g.DrawString($t.ToString('0.000'),$small,$text,$r.Left-58,$y-8) }
    $g.DrawRectangle($axisPen,$r.X,$r.Y,$r.Width,$r.Height)
    $baseY=PY $baseline $lo $hi $r
    $g.DrawLine($globalPen,$r.Left,$baseY,$r.Right,$baseY)
    $points=[System.Drawing.PointF[]]::new($stats.Count)
    for ($i=0;$i -lt $stats.Count;$i++) {
        $s=$stats[$i]; $value=[double]$s.$prop
        $x=PX ([double]$s.elapsed_center_s) $r; $y=PY $value $lo $hi $r
        $points[$i]=[System.Drawing.PointF]::new($x,$y)
        $g.FillEllipse($blue,$x-4,$y-4,8,8)
    }
    $g.DrawLines($linePen,$points)
    $g.DrawString('Elapsed time since braking began [s]',$small,$text,$r.Left+$r.Width/2-92,$r.Bottom+25)
    $g.DrawString(('Red dashed = full 8 s value ({0:F6})' -f $baseline),$small,$text,$r.Right-245,$r.Top+5)
}
try {
    $graphics.DrawString('FR0005: 0.5 s speed statistics during braking',$title,$text,65,20)
    $graphics.DrawString('16 consecutive windows; 1000 samples per window at 2 kHz; standard deviation uses N-1.',$font,$text,67,53)
    $meanRect=[System.Drawing.RectangleF]::new(130,145,1195,260)
    Draw-StatChart $graphics $meanRect 'Mean reported speed [rad/s]' 'mean_speed_rad_s' -0.014 -0.004 @(-0.014,-0.012,-0.010,-0.008,-0.006,-0.004) $globalMean
    $stdRect=[System.Drawing.RectangleF]::new(130,560,1195,260)
    Draw-StatChart $graphics $stdRect 'Sample standard deviation of speed [rad/s]' 'sample_std_speed_rad_s' 0.064 0.073 @(0.064,0.066,0.068,0.070,0.072) $globalStd
    $bitmap.Save($OutputPng,[System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Saved: $OutputCsv"
    Write-Host "Saved: $OutputPng"
    Write-Host ("8 s overall mean={0:F6}, standard deviation={1:F6} rad/s" -f $globalMean,$globalStd)
    Write-Host ("0-2 s mean of window SDs={0:F6}; 6-8 s mean of window SDs={1:F6} rad/s" -f (($stats | Select-Object -First 4 | Measure-Object sample_std_speed_rad_s -Average).Average),(($stats | Select-Object -Last 4 | Measure-Object sample_std_speed_rad_s -Average).Average))
} finally {
    $graphics.Dispose();$bitmap.Dispose();$font.Dispose();$small.Dispose();$title.Dispose()
    $gridPen.Dispose();$axisPen.Dispose();$linePen.Dispose();$globalPen.Dispose();$blue.Dispose();$text.Dispose()
}
