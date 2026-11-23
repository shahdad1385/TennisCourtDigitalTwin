$path = 'C:\Projects\ClayTennis\Tools\build_assets.py'
$s = [System.IO.File]::ReadAllText($path)

function Repl([string]$pattern, [string]$replacement) {
    $script:s = [regex]::Replace($script:s, $pattern, $replacement)
    Write-Host ("replaced: " + $pattern.Substring(0, [Math]::Min(50, $pattern.Length)))
}

Repl 'tint_col = U\.const3\(mat, \(1\.19, 0\.80, 0\.63\), -760, -300\)\s*\r?\n\s*tint_col\.set_editor_property\("parameter_name", "Tint"\)' 'tint_col = U.vector_param(mat, "Tint", (1.19, 0.80, 0.63), -760, -300)'
Repl 'r_min = U\.const\(mat, 0\.76, -1250, 220\)\s*\r?\n\s*r_min\.set_editor_property\("parameter_name", "RoughMin"\)\s*\r?\n\s*r_max = U\.const\(mat, 0\.97, -1250, 300\)\s*\r?\n\s*r_max\.set_editor_property\("RoughMax"\)' 'r_min = U.scalar_param(mat, "RoughMin", 0.76, -1250, 220)`r`n    r_max = U.scalar_param(mat, "RoughMax", 0.97, -1250, 300)'
Repl 'base = U\.const3\(mat, \(0\.93, 0\.92, 0\.89\), -700, -200\)\s*\r?\n\s*base\.set_editor_property\("parameter_name", "BaseColor"\)' 'base = U.vector_param(mat, "BaseColor", (0.93, 0.92, 0.89), -700, -200)'
Repl 'rough = U\.const\(mat, 0\.62, -700, -80\)\s*\r?\n\s*rough\.set_editor_property\("parameter_name", "Roughness"\)' 'rough = U.scalar_param(mat, "Roughness", 0.62, -700, -80)'
Repl 'r = U\.const\(mat, 0\.72, -700, 0\)\s*\r?\n\s*r\.set_editor_property\("parameter_name", "Roughness"\)' 'r = U.scalar_param(mat, "Roughness", 0.72, -700, 0)'
Repl 'ball_col = U\.const3\(mat, \(0\.77, 0\.86, 0\.10\), -900, -200\)\s*\r?\n\s*ball_col\.set_editor_property\("parameter_name", "BallColor"\)' 'ball_col = U.vector_param(mat, "BallColor", (0.77, 0.86, 0.10), -900, -200)'
Repl 'col = U\.const3\(mat, \(0\.02, 0\.09, 0\.05\), -600, -200\)\s*\r?\n\s*col\.set_editor_property\("parameter_name", "ScreenColor"\)' 'col = U.vector_param(mat, "ScreenColor", (0.02, 0.09, 0.05), -600, -200)'
Repl 'em = U\.const3\(mat, \(1\.0, 0\.96, 0\.88\), -600, -200\)\s*\r?\n\s*em\.set_editor_property\("parameter_name", "EmitColor"\)' 'em = U.vector_param(mat, "EmitColor", (1.0, 0.96, 0.88), -600, -200)'

[System.IO.File]::WriteAllText($path, $s)
Write-Host ("remaining const3 param lines: " + ([regex]::Matches($s, 'set_editor_property\("parameter_name"')).Count)
