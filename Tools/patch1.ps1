import re, io
p = r"C:\Projects\ClayTennis\Tools\build_assets.py"
s = io.open(p, encoding="utf-8").read()
s = s.replace('''    tint_col = U.const3(mat, (1.19, 0.80, 0.63), -760, -300)
    tint_col.set_editor_property("parameter_name", "Tint")''',
'''    tint_col = U.vector_param(mat, "Tint", (1.19, 0.80, 0.63), -760, -300)''')
s = s.replace('''    r_min = U.const(mat, 0.76, -1250, 220)
    r_min.set_editor_property("parameter_name", "RoughMin")
    r_max = U.const(mat, 0.97, -1250, 300)
    r_max.set_editor_property("RoughMax")''',
'''    r_min = U.scalar_param(mat, "RoughMin", 0.76, -1250, 220)
    r_max = U.scalar_param(mat, "RoughMax", 0.97, -1250, 300)''')
s = s.replace('''    base = U.const3(mat, (0.93, 0.92, 0.89), -700, -200)
    base.set_editor_property("parameter_name", "BaseColor")''',
'''    base = U.vector_param(mat, "BaseColor", (0.93, 0.92, 0.89), -700, -200)''')
s = s.replace('''    rough = U.const(mat, 0.62, -700, -80)
    rough.set_editor_property("parameter_name", "Roughness")''',
'''    rough = U.scalar_param(mat, "Roughness", 0.62, -700, -80)''')
s = s.replace('''    r = U.const(mat, 0.72, -700, 0)
    r.set_editor_property("parameter_name", "Roughness")''',
'''    r = U.scalar_param(mat, "Roughness", 0.72, -700, 0)''')
s = s.replace('''    ball_col = U.const3(mat, (0.77, 0.86, 0.10), -900, -200)
    ball_col.set_editor_property("parameter_name", "BallColor")''',
'''    ball_col = U.vector_param(mat, "BallColor", (0.77, 0.86, 0.10), -900, -200)''')
s = s.replace('''    col = U.const3(mat, (0.02, 0.09, 0.05), -600, -200)
    col.set_editor_property("parameter_name", "ScreenColor")''',
'''    col = U.vector_param(mat, "ScreenColor", (0.02, 0.09, 0.05), -600, -200)''')
s = s.replace('''    em = U.const3(mat, (1.0, 0.96, 0.88), -600, -200)
    em.set_editor_property("parameter_name", "EmitColor")''',
'''    em = U.vector_param(mat, "EmitColor", (1.0, 0.96, 0.88), -600, -200)''')
io.open(p, "w", encoding="utf-8").write(s)
print("patched", s.count("vector_param"), s.count("scalar_param"))
