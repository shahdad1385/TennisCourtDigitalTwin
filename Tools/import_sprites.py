"""Import the dropped sprite assets (racket, tennis ball, rock) into /Game/Sprites.

Extracts Sprites\*.zip into RawAssets\Sprites\<zipname>\, imports the meshes as
SM_Racket / SM_TennisBall / SM_Rock, imports their textures with a T_Sprt_ prefix,
builds simple materials for meshes that imported without one, and writes a
per-step report to RawAssets\import_sprites_result.txt.

Run inside the editor (it must NOT quit the editor itself):
  UnrealEditor.exe <uproject> -ExecutePythonScript=C:\Projects\ClayTennis\Tools\import_sprites.py
"""
import unreal
import os
import sys
import zipfile
import traceback

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

SRC = r"C:\Projects\ClayTennis\Sprites"
RAW = r"C:\Projects\ClayTennis\RawAssets\Sprites"
OUT = r"C:\Projects\ClayTennis\RawAssets\import_sprites_result.txt"

MESH_DIR = "/Game/Sprites"
TEX_DIR = "/Game/Sprites/Textures"
MAT_DIR = "/Game/Sprites"

# Laptop budget (16 GB, RTX 3070): cap sprite textures at 4K.
MAX_TEX = 4096
MASKS = getattr(unreal.TextureCompressionSettings, "TC_MASKS", None)

# (zip file, marker file that proves the zip was already extracted)
ZIPS = [
    ("racket.zip", "source/racket.fbx"),
    ("tennisball.zip", "scene.gltf"),
    ("desert_western_pile_rock_small_01_ul4lehiga_high.zip",
     "Desert_Western_Pile_Rock_Small_01_ul4lehiga_High.fbx"),
    ("girl-in-blue-rigged.zip", "source/Girl In Blue.blend"),
    ("thailand-girl-number-1-rigged.zip", "source/thailand_girl_1.blend"),
]

# (extracted folder, relative texture path, asset name, import_texture kwargs)
TEX_JOBS = [
    ("tennisball", "textures/material0_baseColor.jpeg",
     "T_Sprt_Ball_BaseColor", dict(srgb=True, max_size=MAX_TEX)),
    ("tennisball", "textures/material0_normal.png",
     "T_Sprt_Ball_Normal", dict(normal=True, max_size=MAX_TEX)),
    ("tennisball", "textures/material0_metallicRoughness.png",
     "T_Sprt_Ball_MetalRough", dict(srgb=False, compression=MASKS, max_size=MAX_TEX)),
    ("desert_western_pile_rock_small_01_ul4lehiga_high",
     "Desert_Western_Pile_Rock_Small_01_ul4lehiga_High_4K_BaseColor.jpg",
     "T_Sprt_Rock_BaseColor", dict(srgb=True)),
    ("desert_western_pile_rock_small_01_ul4lehiga_high",
     "Desert_Western_Pile_Rock_Small_01_ul4lehiga_High_4K_Normal.jpg",
     "T_Sprt_Rock_Normal", dict(normal=True)),
    ("desert_western_pile_rock_small_01_ul4lehiga_high",
     "Desert_Western_Pile_Rock_Small_01_ul4lehiga_High_4K_Roughness.jpg",
     "T_Sprt_Rock_Roughness", dict(srgb=False, compression=MASKS)),
]

RES = []


def note(s):
    RES.append(str(s))
    try:  # rewrite on every step so a hard crash still leaves the report
        with open(OUT, "w", encoding="utf-8") as f:
            f.write("\n".join(RES))
    except Exception as exc:  # noqa: BLE001
        unreal.log_warning("[SPRITES] could not write report: %s" % exc)


def load(path):
    try:
        return U.EAL.load_asset(path)
    except Exception:  # noqa: BLE001
        return None


# ------------------------------------------------------------------- extraction

def extract_all():
    for zip_name, marker in ZIPS:
        zp = os.path.join(SRC, zip_name)
        dest = os.path.join(RAW, os.path.splitext(zip_name)[0])
        if not os.path.isfile(zp):
            note("failed: missing zip %s" % zp)
            continue
        if os.path.isfile(os.path.join(dest, marker)):
            note("extracted: skipped, already extracted (%s present) %s" % (marker, dest))
            continue
        try:
            U.ensure_dir(dest)
            with zipfile.ZipFile(zp) as z:
                z.extractall(dest)
            note("extracted: %s -> %s" % (zip_name, dest))
        except Exception as exc:  # noqa: BLE001
            note("failed: extract %s: %s" % (zip_name, exc))


def report_blends():
    for root, _dirs, files in os.walk(RAW):
        for f in files:
            if f.lower().endswith(".blend"):
                note("skipped: %s - blend unsupported, needs FBX/glTF export" % os.path.join(root, f))


# ------------------------------------------------------------------- meshes

def fbx_static_options():
    """FbxImportUI forcing a plain static mesh import; None = importer defaults."""
    try:
        opts = unreal.FbxImportUI()
        opts.import_as_skeletal = False
        opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        # we build M_Sprt_* materials ourselves (see make_materials)
        opts.import_materials = False
        opts.import_textures = False
        try:
            opts.static_mesh_import_data.import_uniform_scale = 1.0
        except Exception as exc:  # noqa: BLE001
            note("note: import_uniform_scale not set (%s)" % exc)
        return opts
    except Exception as exc:  # noqa: BLE001
        note("note: FbxImportUI unavailable (%s) - importer defaults used" % exc)
        return None


def import_mesh_task(src_file, name, options):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(src_file)
    task.destination_path = MESH_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    U.ASSET_TOOLS.import_asset_tasks([task])
    return task


def find_imported_mesh(task, claimed):
    """The mesh the importer actually created: imported_object_paths first,
    then the destination name, then any mesh in the folder (glTF/Interchange
    may name the mesh after its node instead of the destination)."""
    for p in (task.imported_object_paths or []):
        a = load(p.split(".")[0])
        if isinstance(a, unreal.StaticMesh) and a.get_path_name().split(".")[0] not in claimed:
            return a.get_path_name().split(".")[0]
    scan = [MESH_DIR + "/" + task.destination_name]
    for n in U.EAL.list_assets(MESH_DIR):
        scan.append(MESH_DIR + "/" + n)
    for p in scan:
        a = load(p)
        if isinstance(a, unreal.StaticMesh) and a.get_path_name().split(".")[0] not in claimed:
            return a.get_path_name().split(".")[0]
    return None


def ensure_name(mesh_path, final_name, label):
    """Make the mesh end up at MESH_DIR/<final_name>, deterministically."""
    final = MESH_DIR + "/" + final_name
    if not mesh_path:
        note("failed: %s - no static mesh found after import" % label)
        return None
    if mesh_path == final:
        note("imported: %s" % final)
        return final
    if U.EAL.does_asset_exist(final):
        note("note: stale %s deleted (left over from a previous run)" % final)
        U.EAL.delete_asset(final)
    new = None
    try:
        new = U.EAL.rename_asset(mesh_path, final)
    except Exception as exc:  # noqa: BLE001
        note("note: rename failed (%s)" % exc)
    if new:
        note("renamed: %s -> %s" % (mesh_path, final))
        return final
    try:
        new = U.EAL.duplicate_asset(mesh_path, final)
    except Exception as exc:  # noqa: BLE001
        note("failed: duplicate %s: %s" % (final, exc))
    if new:
        note("duplicated: %s -> %s (original %s kept)" % (mesh_path, final, mesh_path))
        return final
    note("failed: could not rename/duplicate to %s - keeping %s" % (final, mesh_path))
    return mesh_path


def import_meshes():
    claimed = set()
    meshes = {}

    def do(label, rel_src, final_name, options):
        src = os.path.join(RAW, rel_src)
        if not os.path.isfile(src):
            note("failed: %s - source file missing: %s" % (label, src))
            return None
        task = import_mesh_task(src, final_name, options)
        path = find_imported_mesh(task, claimed)
        if path:
            claimed.add(path)
        path = ensure_name(path, final_name, label)
        meshes[label] = load(path) if path else None
        if meshes[label] is None:
            note("failed: %s mesh could not be loaded at %s" % (label, path))
        return path

    do("racket", os.path.join("racket", "source", "racket.fbx"),
       "SM_Racket", fbx_static_options())
    do("ball", os.path.join("tennisball", "scene.gltf"),
       "SM_TennisBall", None)  # glTF: Interchange pipeline, no FbxImportUI
    do("rock", os.path.join("desert_western_pile_rock_small_01_ul4lehiga_high",
                            "Desert_Western_Pile_Rock_Small_01_ul4lehiga_High.fbx"),
       "SM_Rock", fbx_static_options())
    return meshes


# ------------------------------------------------------------------- textures

def import_textures():
    texs = {}
    for folder, rel, name, kw in TEX_JOBS:
        t = U.import_texture(os.path.join(RAW, folder, rel), TEX_DIR, name, **kw)
        if t:
            texs[name] = t
            note("imported: %s" % t.get_path_name().split(".")[0])
        else:
            note("failed: texture %s (%s)" % (name, os.path.join(RAW, folder, rel)))
    return texs


# ------------------------------------------------------------------- materials

def build_material(name, base_tex=None, normal_tex=None, base_color=None, roughness=0.5):
    """Simple BaseColor + Normal material. Default sampler (project trap #1)
    and default 0..1 UVs only - NO TextureCoordinate/UV nodes (trap #2)."""
    p = MAT_DIR + "/" + name
    if U.asset_exists(p):
        m = U.EAL.load_asset(p)
        U.MEL.delete_all_material_expressions(m)
    else:
        m = U.new_material(name, folder=MAT_DIR)
    if base_tex:
        b = U.tex_param(m, "BaseColorTex", -600, -250, default=base_tex)
        U.MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    elif base_color:
        U.MEL.connect_material_property(
            U.vector_param(m, "BaseColorTint", base_color, -600, -250), "",
            unreal.MaterialProperty.MP_BASE_COLOR)
    if normal_tex:
        n = U.tex_param(m, "NormalTex", -600, 40, default=normal_tex)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.MEL.connect_material_property(
        U.scalar_param(m, "Roughness", roughness, -600, 200), "",
        unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(m)
    note("imported: %s" % m.get_path_name().split(".")[0])
    return m


def mesh_materials(mesh):
    n = 0
    try:
        n = int(unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
                .get_number_materials(mesh))
    except Exception:  # noqa: BLE001
        try:
            n = len(mesh.get_editor_property("static_materials"))
        except Exception:  # noqa: BLE001
            n = 0
    mats = []
    for i in range(n):
        try:
            mats.append(mesh.get_material(i))
        except Exception:  # noqa: BLE001
            mats.append(None)
    return mats


def ensure_simple_material(mesh, label, builder):
    """Only when the mesh imported without materials - leave imported ones alone."""
    if not mesh:
        return
    mats = mesh_materials(mesh)
    if any(m is not None for m in mats):
        note("skipped: %s already has materials from import (%d slots) - left untouched"
             % (label, len(mats)))
        return
    m = builder()
    if not m:
        return
    mat_path = m.get_path_name().split(".")[0]
    if not mats:
        try:
            mesh.add_material(m)
            note("assigned: %s -> %s (added slot 0)" % (mat_path, label))
        except Exception as exc:  # noqa: BLE001
            note("failed: assign material to %s: %s" % (label, exc))
    else:
        for i, cur in enumerate(mats):
            if cur is not None:
                continue
            try:
                mesh.set_material(i, m)
                note("assigned: %s -> %s slot %d" % (mat_path, label, i))
            except Exception as exc:  # noqa: BLE001
                note("failed: assign material to %s slot %d: %s" % (label, i, exc))
    U.EAL.save_loaded_asset(mesh, False)


def make_materials(meshes, texs):
    if meshes.get("rock"):
        ensure_simple_material(meshes["rock"], "SM_Rock", lambda: build_material(
            "M_Sprt_Rock",
            base_tex=texs.get("T_Sprt_Rock_BaseColor"),
            normal_tex=texs.get("T_Sprt_Rock_Normal"),
            roughness=0.9))
    if meshes.get("ball"):
        ensure_simple_material(meshes["ball"], "SM_TennisBall", lambda: build_material(
            "M_Sprt_Ball",
            base_tex=texs.get("T_Sprt_Ball_BaseColor"),
            normal_tex=texs.get("T_Sprt_Ball_Normal"),
            roughness=0.85))
    if meshes.get("racket"):
        ensure_simple_material(meshes["racket"], "SM_Racket", lambda: build_material(
            "M_Sprt_Racket",
            base_color=(0.6, 0.61, 0.63),
            roughness=0.35))


# ------------------------------------------------------------------- main

def inventory():
    def listing(path):
        try:
            items = U.EAL.list_assets(path)
        except Exception:  # noqa: BLE001
            items = []
        return ", ".join(items) if items else "empty"
    note("inventory: /Game/Sprites -> " + listing(MESH_DIR))
    note("inventory: /Game/Sprites/Textures -> " + listing(TEX_DIR))


def main():
    note("=== import_sprites ===")
    U.ensure_dir(RAW)
    extract_all()
    report_blends()
    texs = import_textures()
    meshes = import_meshes()
    make_materials(meshes, texs)
    inventory()
    fails = sum(1 for line in RES if line.startswith("failed"))
    unreal.log("[SPRITES] done: %d steps, %d failed - report: %s" % (len(RES), fails, OUT))


try:
    main()
except Exception:  # noqa: BLE001
    note("EXC\n" + traceback.format_exc())
    unreal.log_warning("[SPRITES] finished with exception - see %s" % OUT)
