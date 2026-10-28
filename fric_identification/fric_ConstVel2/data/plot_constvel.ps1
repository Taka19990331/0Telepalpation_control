param(
  [string]$CsvPath = 'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_200111.csv',
  [string]$PngPath = 'C:\0Telepalpation_control\fric_identification\fric_ConstVel2\data\ConstVel_20261027_200111_last7s.png',
  [double]$LastSeconds = 7.0
)
Add-Type -AssemblyName System.Drawing
$lines = [System.IO.File]::ReadAllLines($CsvPath)
$headerIndex = -1
for ($i=0; $i -lt $lines.Length; $i++) { if ($lines[$i].StartsWith('timestamp_us,')) { $headerIndex=$i; break } }
if ($headerIndex -lt 0) { throw 'CSV column header not found.' }
$rows = $lines[$headerIndex..($lines.Length-1)] | Where-Object { $_ -and -not $_.StartsWith('CONSTVEL_') } | ConvertFrom-Csv
if (-not $rows -or $rows.Count -lt 2) { throw 'No sample rows found.' }
$culture = [Globalization.CultureInfo]::InvariantCulture
$runEndTime = [double]::Parse($rows[-1].timestamp_us,$culture) / 1000000.0
$rows = @($rows | Where-Object { ([double]::Parse($_.timestamp_us,$culture) / 1000000.0) -ge ($runEndTime - $LastSeconds) })
$speedKp = 1.44566
$time = [System.Collections.Generic.List[double]]::new()
$speedCmd = [System.Collections.Generic.List[double]]::new()
$speedEst = [System.Collections.Generic.List[double]]::new()
$torqueCmd = [System.Collections.Generic.List[double]]::new()
$torqueEst = [System.Collections.Generic.List[double]]::new()
foreach ($row in $rows) {
  $t = [double]::Parse($row.timestamp_us,$culture) / 1000000.0
  $cmd = [double]::Parse($row.command_rad_s,$culture)
  $vel = [double]::Parse($row.velocity_rad_s,$culture)
  $iTerm = [double]::Parse($row.i_torque_command_nm,$culture)
  $estTau = [double]::Parse($row.torque_estimate_nm,$culture)
  $time.Add($t); $speedCmd.Add($cmd); $speedEst.Add($vel)
  $torqueCmd.Add(($speedKp * ($cmd - $vel)) + $iTerm); $torqueEst.Add($estTau)
}
$W=1600; $H=1020
$bmp = [System.Drawing.Bitmap]::new($W,$H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::White)
$font = [System.Drawing.Font]::new('Segoe UI',11)
$small = [System.Drawing.Font]::new('Segoe UI',10)
$titleFont = [System.Drawing.Font]::new('Segoe UI',22,[System.Drawing.FontStyle]::Bold)
$panelFont = [System.Drawing.Font]::new('Segoe UI',15,[System.Drawing.FontStyle]::Bold)
$black = [System.Drawing.Brushes]::Black
$gridPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(220,225,232),1)
$axisPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(75,85,99),1.4)
$cmdColor = [System.Drawing.Color]::FromArgb(150,220,80,55)
$estColor = [System.Drawing.Color]::FromArgb(155,35,105,190)
$cmdPen = [System.Drawing.Pen]::new($cmdColor,2.2)
$estPen = [System.Drawing.Pen]::new($estColor,1.8)
$minTime = $time[0]
$maxTime = $time[$time.Count-1]
$timeSpan = [Math]::Max(0.001,$maxTime-$minTime)
function Draw-Panel($g,$top,$panelTitle,$ylabel,$a,$aLabel,$b,$bLabel,$format) {
  $left=130; $right=1530; $plotTop=$top+42; $plotBottom=$top+360; $plotW=$right-$left; $plotH=$plotBottom-$plotTop
  $all = @($a.ToArray()) + @($b.ToArray())
  $yMin=($all | Measure-Object -Minimum).Minimum; $yMax=($all | Measure-Object -Maximum).Maximum
  if ($yMax -le $yMin) { $yMin -= 1; $yMax += 1 }
  $pad=($yMax-$yMin)*0.08; $yMin -= $pad; $yMax += $pad
  $g.DrawString($panelTitle,$panelFont,$black,130,$top)
  for ($k=0; $k -le 5; $k++) {
    $yv=$yMin+($yMax-$yMin)*$k/5.0; $yp=$plotBottom-($plotH*$k/5.0)
    $g.DrawLine($gridPen,$left,[single]$yp,$right,[single]$yp)
    $g.DrawString(($yv.ToString($format,$culture)), $small, $black, 68, [single]($yp-9))
  }
  for ($k=0; $k -le 6; $k++) {
    $xv=$minTime+$timeSpan*$k/6.0; $xp=$left+($plotW*$k/6.0)
    $g.DrawLine($gridPen,[single]$xp,[single]$plotTop,[single]$xp,[single]$plotBottom)
    $g.DrawString(($xv.ToString('0.0',$culture)), $small, $black, [single]($xp-16), [single]($plotBottom+7))
  }
  $g.DrawLine($axisPen,$left,$plotTop,$left,$plotBottom); $g.DrawLine($axisPen,$left,$plotBottom,$right,$plotBottom)
  $step=[Math]::Max(1,[Math]::Floor($time.Count/7000))
  foreach ($series in @(@{values=$b;pen=$estPen},@{values=$a;pen=$cmdPen})) {
    $pts=[System.Collections.Generic.List[System.Drawing.PointF]]::new()
    for ($i=0; $i -lt $time.Count; $i += $step) {
      $x=$left+(($time[$i]-$minTime)/$timeSpan)*$plotW
      $y=$plotBottom-(($series.values[$i]-$yMin)/($yMax-$yMin))*$plotH
      $pts.Add([System.Drawing.PointF]::new([single]$x,[single]$y))
    }
    if (($time.Count-1) % $step -ne 0) {
      $i=$time.Count-1; $x=$left+(($time[$i]-$minTime)/$timeSpan)*$plotW; $y=$plotBottom-(($series.values[$i]-$yMin)/($yMax-$yMin))*$plotH
      $pts.Add([System.Drawing.PointF]::new([single]$x,[single]$y))
    }
    if ($pts.Count -gt 1) { $g.DrawLines($series.pen,$pts.ToArray()) }
  }
  $g.DrawString($ylabel,$small,$black,10,[single]($plotTop+$plotH/2-10))
  $legendY=$top+5
  $g.DrawLine($cmdPen,1030,[single]$legendY,1065,[single]$legendY); $g.DrawString($aLabel,$small,$black,1072,[single]($legendY-9))
  $g.DrawLine($estPen,1280,[single]$legendY,1315,[single]$legendY); $g.DrawString($bLabel,$small,$black,1322,[single]($legendY-9))
}
$g.DrawString('Constant velocity test — final 1 s — ' + [IO.Path]::GetFileName($CsvPath),$titleFont,$black,90,25)
Draw-Panel $g 95 'Speed' 'rad/s' $speedCmd 'Command' $speedEst 'Estimated' '0.00'
Draw-Panel $g 535 'Torque' 'N·m' $torqueCmd 'Command (reconstructed)' $torqueEst 'Estimated' '0.000'
$g.DrawString('Time (s)',$font,$black,770,960)
$bmp.Save($PngPath,[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "Saved plot: $PngPath"
Write-Output "Samples: $($time.Count); interval: $($minTime.ToString('0.00',$culture))–$($maxTime.ToString('0.00',$culture)) s"






