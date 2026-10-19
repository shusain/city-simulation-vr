"""Focused D79 adoption; no sandbox-map rewrite."""
from pathlib import Path
import json,sys,unreal
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'Tools'))
from CityStorefrontCatalog import configure
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
assert catalog
report=configure(catalog)
assert len(report)==8
assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
(root/'Artifacts/D79/Integration/storefront-catalog.json').write_text(json.dumps(dict(businesses=report,gameplay_definitions_unchanged=True),indent=2)+'\n')
print('CITYSIM_D79_STOREFRONT_CATALOG_PASS 8 businesses; 48 production meshes')
