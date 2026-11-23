"""Shared helpers for building the ClayTennis Unreal project from Python."""
import unreal
import os

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[BUILD] " + str(msg))


def ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path)


def asset_exists(path):
    return EAL.does_asset_exist(path)


def delete_asset(path):
    if asset_exists(path):
        EAL.delete_asset(path)


def import_texture(filename, dest_path, name, srgb=True, normal=False,
                   compression=None, no_mips=False, max_size=0):
    """Import an image as Texture2D with sane settings. Returns the asset or None."""
    full = os.path.abspath(filename)
    if not os.path.isfile(full):
        log("MISSING FILE " + full)
        return None

    task = unreal.AssetImportTask()
    task.filename = full
    task.destination_path = dest_path
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = None
    ASSET_TOOLS.import_asset_tasks([task])

    path = dest_path + "/" + name
    if task.imported_object_paths:
        path = task.imported_object_paths[0].split(".")[0]
    tex = EAL.load_asset(path)
    if not tex:
        log("IMPORT FAILED " + path)
        return None
    path = tex.get_path_name().split(".")[0]
    tex.set_editor_property("srgb", srgb)
    if normal:
        tex.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif compression is not None:
        tex.set_editor_property("compression_settings", compression)
    if max_size:
        tex.set_editor_property("max_texture_size", max_size)
    if no_mips:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    EAL.save_loaded_asset(tex, False)
    log("imported " + path)
    return tex


def _expr(mat, cls, x, y, **props):
    node = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        try:
            node.set_editor_property(k, v)
        except Exception as exc:  # noqa: BLE001
            log("expr prop %s failed: %s" % (k, exc))
    return node


def new_material(name, folder="/Game/Materials", two_sided=False, blend=None,
                 shading=None):
    path = folder + "/" + name
    if asset_exists(path):
        return EAL.load_asset(path)
    mat = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", two_sided)
    if blend is not None:
        mat.set_editor_property("blend_mode", blend)
    if shading is not None:
        mat.set_editor_property("shading_model", shading)
    return mat


def tex_param(mat, name, x, y, sampler="2d", default=None):
    cls = (unreal.MaterialExpressionTextureSampleParameterCube
           if sampler == "cube" else
           unreal.MaterialExpressionTextureSampleParameter2D)
    node = _expr(mat, cls, x, y, parameter_name=name)
    if default is not None:
        node.set_editor_property("texture", default)
    return node


def const(mat, value, x, y):
    node = _expr(mat, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", float(value))
    return node


def const3(mat, color, x, y):
    node = _expr(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    return node


def scalar_param(mat, name, value, x, y):
    node = _expr(mat, unreal.MaterialExpressionScalarParameter, x, y,
                 parameter_name=name, default_value=float(value))
    return node


def vector_param(mat, name, color, x, y):
    node = _expr(mat, unreal.MaterialExpressionVectorParameter, x, y,
                 parameter_name=name,
                 default_value=unreal.LinearColor(color[0], color[1], color[2], 1.0))
    return node


def mul(mat, a, b, x, y):
    node = _expr(mat, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, "", node, "A")
    MEL.connect_material_expressions(b, "", node, "B")
    return node


def lerp(mat, a, b, alpha, x, y):
    node = _expr(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, "", node, "A")
    MEL.connect_material_expressions(b, "", node, "B")
    MEL.connect_material_expressions(alpha, "", node, "Alpha")
    return node


def world_uv(mat, x, y, tiling=1.0):
    """World-aligned UV (projection on XY plane) so surfaces tile without stretching."""
    uv = _expr(mat, unreal.MaterialExpressionWorldPosition, x, y)
    # component mask -> XY
    mask = _expr(mat, unreal.MaterialExpressionComponentMask, x + 150, y)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    MEL.connect_material_expressions(uv, "", mask, "")
    tile = const(mat, tiling, x + 150, y + 120)
    out = mul(mat, mask, tile, x + 320, y)
    return out


def compile_and_save(mat):
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)


def new_instance(name, parent, folder="/Game/Materials", textures=None, scalars=None, vectors=None):
    if textures is None:
        textures = {}
    if scalars is None:
        scalars = {}
    if vectors is None:
        vectors = {}
    path = folder + "/" + name
    if not asset_exists(path):
        mi = ASSET_TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    else:
        mi = EAL.load_asset(path)
    MEL.set_material_instance_parent(mi, parent)
    for param, tex in textures.items():
        MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    for param, val in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, param, float(val))
    for param, col in vectors.items():
        MEL.set_material_instance_vector_parameter_value(
            mi, param, unreal.LinearColor(col[0], col[1], col[2], 1.0))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    return mi


# ---------------------------------------------------------------- actor helpers

ACTOR_SUB = None


def actors():
    global ACTOR_SUB
    if ACTOR_SUB is None:
        ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return ACTOR_SUB


_BASIC = {}


def basic_mesh(shape):
    if shape not in _BASIC:
        _BASIC[shape] = EAL.load_asset("/Engine/BasicShapes/" + shape)
    return _BASIC[shape]


def spawn_mesh(label, shape, location, scale, material=None, rotation=(0, 0, 0),
               cast_shadow=True, collision=False):
    """scale is in metres (x,y,z); location in cm."""
    actor = actors().spawn_actor_from_class(unreal.StaticMeshActor,
                                            unreal.Vector(*location),
                                            unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    actor.set_actor_label(label)
    smc = actor.static_mesh_component
    smc.set_mobility(unreal.ComponentMobility.STATIC)
    smc.set_static_mesh(basic_mesh(shape))
    actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
    if material:
        smc.set_material(0, material)
    smc.set_editor_property("cast_shadow", cast_shadow)
    if collision:
        smc.set_collision_profile_name("BlockAll")
    else:
        smc.set_collision_profile_name("NoCollision")
    return actor


def spawn_plane(label, location, size_m, material, rotation=(0, 0, 0)):
    """Plane asset is 100x100 cm, so scale = size in metres."""
    return spawn_mesh(label, "Plane", location, (size_m[0], size_m[1], 1), material, rotation)


def spawn_light(cls, label, location, rotation, **props):
    actor = actors().spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    actor.set_actor_label(label)
    comp = actor.get_component_by_class(unreal.LightComponent)
    for k, v in props.items():
        try:
            comp.set_editor_property(k, v)
        except Exception as exc:  # noqa: BLE001
            log("light prop %s failed: %s" % (k, exc))
    return actor
