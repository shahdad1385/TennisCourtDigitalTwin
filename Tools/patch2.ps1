$path = 'C:\Projects\ClayTennis\Tools\build_assets.py'
$s = [System.IO.File]::ReadAllText($path)

# --- M_Net: switch from world-projected UV to a plane UV with explicit tiling
$old = @'
    if net_diff:
        uv = U.world_uv(mat, -1200, -300, 1.0 / 100.0)
        t = U.tex_param(mat, "NetTex", -950, -300, default=net_diff)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
'@
$new = @'
    if net_diff:
        uv = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, -1200, -300)
        uv.set_editor_property("utiling", 13.0)
        uv.set_editor_property("vtiling", 1.0)
        t = U.tex_param(mat, "NetTex", -950, -300, default=net_diff)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
'@
$s = $s.Replace($old, $new)

# --- strings: plane UV tiling
$old2 = @'
        uv = U.world_uv(mat, -1000, -200, 1.0 / 60.0)
        t = U.tex_param(mat, "StringTex", -700, -200, default=strings)
'@
$new2 = @'
        uv = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, -1000, -200)
        uv.set_editor_property("utiling", 2.2)
        uv.set_editor_property("vtiling", 2.8)
        t = U.tex_param(mat, "StringTex", -700, -200, default=strings)
'@
$s = $s.Replace($old2, $new2)

# --- rebuild materials from scratch on every run
$old3 = @'
    U.log("=== build_assets start ===")
    tex_dir = "/Game/Textures"
'@
$new3 = @'
    U.log("=== build_assets start ===")
    tex_dir = "/Game/Textures"

    for asset in U.EAL.list_assets("/Game/Materials"):
        U.EAL.delete_asset(asset)
'@
$s = $s.Replace($old3, $new3)

[System.IO.File]::WriteAllText($path, $s)
Write-Host ("net patch: " + ($s.Contains('utiling", 13.0')))
Write-Host ("strings patch: " + ($s.Contains('utiling", 2.2')))
Write-Host ("clean patch: " + ($s.Contains('list_assets("/Game/Materials")')))
