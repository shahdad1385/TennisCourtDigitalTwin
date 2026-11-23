$p = 'C:\Projects\ClayTennis\Tools\build_level.py'
$s = [System.IO.File]::ReadAllText($p)
function Rep($a, $b) { $script:s = $script:s.Replace($a, $b) }

Rep 'U.spawn_plane("Ground_Apron", (0, 0, -1.0), (36.6, 18.3), clay)' 'U.spawn_plane("Ground_Apron", (0, 0, -1.0), (44.0, 26.0), clay)'
Rep 'U.spawn_plane("Ground_Outer", (0, 0, -6.0), (400, 400), ground)' 'U.spawn_plane("Ground_Outer", (0, 0, -6.0), (500, 500), ground)'
Rep '    fx = 1750.0' '    fx = 2300.0'
Rep '    fy = 950.0' '    fy = 1350.0'
Rep 'U.spawn_mesh("Fence_End_%d" % i, "Cube", (x, 0, 175), (0.04, 19.0, 3.5), screen)' 'U.spawn_mesh("Fence_End_%d" % i, "Cube", (x, 0, 200), (0.04, 27.0, 4.0), screen)'
Rep 'U.spawn_mesh("Fence_Side_%d" % i, "Cube", (0, y, 175), (35.0, 0.04, 3.5), screen)' 'U.spawn_mesh("Fence_Side_%d" % i, "Cube", (0, y, 200), (46.0, 0.04, 4.0), screen)'
Rep 'U.spawn_mesh("Post_%.0f_%.0f" % (x, y), "Cylinder", (x, y, 180), (0.08, 0.08, 3.6), metal)' 'U.spawn_mesh("Post_%.0f_%.0f" % (x, y), "Cylinder", (x, y, 200), (0.08, 0.08, 4.0), metal)'
Rep '            y = side * (1180 + row * 90)' '            y = side * (1520 + row * 90)'
Rep '        U.spawn_mesh("StandWall_%d" % side, "Cube", (0, side * 1640, 380), (34.0, 0.3, 7.6), concrete)' '        U.spawn_mesh("StandWall_%d" % side, "Cube", (0, side * 2000, 380), (34.0, 0.3, 7.6), concrete)'
Rep '    for x in (-1950, 1950):' '    for x in (-2600, 2600):'
Rep '        for y in (-1150, 1150):' '        for y in (-1500, 1500):'

# lighting / exposure
Rep 'unreal.Rotator(-9.0, 205.0, 0))' 'unreal.Rotator(-10.0, 200.0, 0))'
Rep '"intensity": 8.0,' '"intensity": 9.0,'
Rep '"temperature": 3400.0,' '"temperature": 3600.0,'
Rep '"fog_density": 0.015,' '"fog_density": 0.006,'
Rep '"volumetric_fog_extinction_scale": 0.6,' '"volumetric_fog_extinction_scale": 0.35,'
Rep 'setp("auto_exposure_bias", 0.7, "override_auto_exposure_bias")' 'setp("auto_exposure_bias", 0.0, "override_auto_exposure_bias")'
Rep 'setp("auto_exposure_max_brightness", 8.0, "override_auto_exposure_max_brightness")' 'setp("auto_exposure_max_brightness", 3.0, "override_auto_exposure_max_brightness")'

# player start / camera helper position
Rep 'unreal.Vector(-1850, 120, 780),
                                       unreal.Rotator(-14.0, 8.0, 0))' 'unreal.Vector(-1240, -130, 110),
                                       unreal.Rotator(-6.0, 3.0, 0))'
Rep 'unreal.Vector(-1850, 120, 780),
                                        unreal.Rotator(-14.0, 8.0, 0))' 'unreal.Vector(-1500, -1000, 165),
                                        unreal.Rotator(-4.0, -33.0, 0))'

[System.IO.File]::WriteAllText($p, $s)
Write-Host 'level layout patched'
Select-String -Path $p -Pattern 'fx = |fy = |intensity|fog_density|exposure_bias|max_brightness|PlayerStart|CineCameraActor' | Select-Object LineNumber,Line

