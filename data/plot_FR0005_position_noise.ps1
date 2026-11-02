$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms.DataVisualization
Add-Type -AssemblyName System.Drawing
$dir = Join-Path $PSScriptRoot 'FR0005_position_analysis'
$data = Get-Content (Join-Path $dir 'plot_data.json') -Raw | ConvertFrom-Json
$chart = New-Object System.Windows.Forms.DataVisualization.Charting.Chart
$chart.Width = 1600
$chart.Height = 1000
$chart.BackColor = [System.Drawing.Color]::White
$chart.AntiAliasing = 'All'
$title = $chart.Titles.Add('FR0005: stationary position at zero commanded torque')
$title.Font = [System.Drawing.Font]::new('Segoe UI',20,[System.Drawing.FontStyle]::Bold)
$keys = @('initial','post_coast')
$colors = @([System.Drawing.Color]::FromArgb(39,100,165),[System.Drawing.Color]::FromArgb(197,106,37))
$names = @('Initial: 0.0005 - 0.7825 s','After coast: 31.8465 - 32.3460 s')
for ($col=0; $col -lt 2; $col++) {
    $v = $data.($keys[$col])
    $values = @($v.position_deg | Sort-Object)
    $median = [double]$values[[int][math]::Floor($values.Count/2)]
    for ($row=0; $row -lt 2; $row++) {
        $id = "panel_${row}_${col}"
        $area = $chart.ChartAreas.Add($id)
        $area.Position = [System.Windows.Forms.DataVisualization.Charting.ElementPosition]::new([single](2+49*$col),[single](10+44*$row),47,40)
        $area.AxisX.LabelStyle.Font = [System.Drawing.Font]::new('Segoe UI',11)
        $area.AxisY.LabelStyle.Font = [System.Drawing.Font]::new('Segoe UI',11)
        $area.AxisX.TitleFont = [System.Drawing.Font]::new('Segoe UI',12)
        $area.AxisY.TitleFont = [System.Drawing.Font]::new('Segoe UI',12)
        $area.AxisX.MajorGrid.Enabled = $false
        $area.AxisY.MajorGrid.LineColor = [System.Drawing.Color]::FromArgb(225,230,235)
        $series = $chart.Series.Add($id)
        $series.ChartArea = $id
        $series.Color = $colors[$col]
        if ($row -eq 0) {
            $series.ChartType = 'StepLine'
            $series.BorderWidth = 1
            $area.AxisX.Title = 'Run time [s]'
            $area.AxisY.Title = 'Position minus median [deg]'
            $area.AxisY.Minimum = -0.03
            $area.AxisY.Maximum = 0.03
            $area.AxisY.Interval = 0.01
            $area.AxisY.LabelStyle.Format = '0.00'
            $area.AxisX.Minimum = [double]$v.actual_us[0]/1e6
            $area.AxisX.Maximum = [double]$v.actual_us[-1]/1e6
            $area.AxisX.LabelStyle.Format = '0.00'
            for ($i=0;$i -lt $v.actual_us.Count;$i++) {
                [void]$series.Points.AddXY(([double]$v.actual_us[$i]/1e6),([double]$v.position_deg[$i]-$median))
            }
        } else {
            $series.ChartType = 'Column'
            $series.IsValueShownAsLabel = $true
            $series.LabelFormat = '0.00'
            $series.Font = [System.Drawing.Font]::new('Segoe UI',12)
            $area.AxisX.Title = 'Position code relative to median [LSB]'
            $area.AxisY.Title = 'Samples [%]'
            $area.AxisX.Minimum = -2
            $area.AxisX.Maximum = 2
            $area.AxisX.Interval = 1
            $area.AxisY.Minimum = 0
            $area.AxisY.Maximum = 110
            $area.AxisY.Interval = 20
            foreach ($group in ($v.position_deg | Group-Object | Sort-Object { [double]$_.Name })) {
                $offset = [math]::Round(([double]$group.Name-$median)/0.0218569388545)
                [void]$series.Points.AddXY($offset,100*$group.Count/$v.actual_us.Count)
            }
        }
        $pt = $chart.Titles.Add($(if($row -eq 0){$names[$col]}else{'Position level occupancy'}))
        $pt.DockedToChartArea = $id
        $pt.IsDockedInsideChartArea = $false
        $pt.Font = [System.Drawing.Font]::new('Segoe UI',14,[System.Drawing.FontStyle]::Bold)
    }
}
$foot = $chart.Titles.Add('1 LSB = 0.02185694 deg = 0.000381476 rad. Initial invalid record excluded. All plotted records: target_torque_Nm = 0.')
$foot.Docking = 'Bottom'
$foot.Font = [System.Drawing.Font]::new('Segoe UI',11)
try {
    $chart.SaveImage((Join-Path $dir 'position_noise.png'),'Png')
} finally {
    $chart.Dispose()
}
