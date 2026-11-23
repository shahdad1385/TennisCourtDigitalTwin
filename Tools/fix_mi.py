import unreal, sys
sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U
parent = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_LinePaint")
if parent:
    U.new_instance("MI_CourtLine", parent)
    unreal.log("[FIX] MI_CourtLine created")
# verify every MI referenced by the level exists
for n in ["MI_ClayCourt","MI_ClayApron","MI_CourtLine","MI_Net","MI_Metal","MI_Windscreen","MI_Hedge","MI_Tree","MI_Carbon","MI_Concrete","MI_Floodlight","MI_TennisBall","MI_Crowd","MI_CourtLogo"]:
    print(n, unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/" + n))
