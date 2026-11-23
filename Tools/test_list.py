import os
d=r"C:\Projects\ClayTennis\RawAssets\Textures"
open(r"C:\Projects\ClayTennis\RawAssets\test_list.txt","w",encoding="utf-8").write("\n".join([repr(x) for x in os.listdir(d)]))
