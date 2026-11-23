$path = 'C:\Projects\ClayTennis\Tools\build_level.py'
$s = [System.IO.File]::ReadAllText($path)

$s = $s.Replace('        "volumetric_fog": True,', '        "enable_volumetric_fog": True,')
$s = $s.Replace('setp("auto_exposure_min_ev100", 8.0, "override_auto_exposure_min_ev100")
    setp("auto_exposure_max_ev100", 15.5, "override_auto_exposure_max_ev100")',
'setp("auto_exposure_min_brightness", 0.05, "override_auto_exposure_min_brightness")
    setp("auto_exposure_max_brightness", 8.0, "override_auto_exposure_max_brightness")')

$old = @'
    # ------------------------------------------------------------ postprocess
'@
$new = @'
    try:
        cam.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
        note("camera auto activate set")
    except Exception as exc:
        note("camera auto activate: %s" % exc)

    # level player start (used by the standalone game view)
    ps = actors.spawn_actor_from_class(unreal.PlayerStart,
                                       unreal.Vector(-1850, 120, 780),
                                       unreal.Rotator(-14.0, 8.0, 0))
    ps.set_actor_label("PlayerStart")

    # ------------------------------------------------------------ postprocess
'@
$s = $s.Replace($old, $new)

[System.IO.File]::WriteAllText($path, $s)
Write-Host ("fog fixed: " + $s.Contains('enable_volumetric_fog'))
Write-Host ("exposure fixed: " + $s.Contains('auto_exposure_min_brightness'))
Write-Host ("playerstart added: " + $s.Contains('PlayerStart'))
