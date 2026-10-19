"""D79 surface-only adaptation of the accepted D66 low-home display mesh."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability, set_capability
import hashlib,json,sys,bpy
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'Tools'))
from CitySeasonalBuildingSurfaces import apply_home
path=root/'ArtSource/Manifests/city_seasonal_kit.json';doc=json.loads(path.read_text())
entry=next(e for e in doc['assets'] if e['name']=='SM_Building_Residential_SeasonalSpace_A')
if has_capability(entry, 'reviewed_building'):
    print('CITYSIM_D91_REVIEWED_HOME_RETAINED; use Create-CityBuildingWindowStudy.py for home authoring')
    raise SystemExit(0)
prior=entry.get('surface_source_entry',dict(entry))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
bpy.ops.import_scene.fbx(filepath=str(root/prior['export']))
obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
geometry=[v.co.copy() for v in obj.data.vertices];alpha=[v.color[3] for v in obj.data.color_attributes.get('WindowMask').data]
slots=apply_home(obj)
assert geometry==[v.co for v in obj.data.vertices] and alpha==[v.color[3] for v in obj.data.color_attributes.get('WindowMask').data]
for index,material in enumerate(bpy.data.materials):material.name='_SeasonalSurface_'+str(index)
for material,name in zip(obj.data.materials,slots):material.name=name
export=root/'ArtSource/Exports/BuildingSurfacePrototypes/SM_ResidentialSeasonal_Surface.fbx'
source=root/'ArtSource/Blender/CityResidentialSurface.blend'
bpy.ops.export_scene.fbx(filepath=str(export),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
    global_scale=1,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(source))
entry.update(surface_source_entry=prior,building_surface_task='D79',material_slots=slots,
             source=source.relative_to(root).as_posix(),export=export.relative_to(root).as_posix())
set_capability(entry, 'building_surfaces')
path.write_text(json.dumps(doc,indent=2)+'\n')
out=root/'Artifacts/D79/Integration';out.mkdir(parents=True,exist_ok=True)
(out/'seasonal-surface.json').write_text(json.dumps(dict(geometry_unchanged=True,alpha_unchanged=True,
    original_sha256=hashlib.sha256((root/prior['export']).read_bytes()).hexdigest(),export_sha256=hashlib.sha256(export.read_bytes()).hexdigest(),entry=entry),indent=2)+'\n')
print('CITYSIM_D79_SEASONAL_SURFACE_PASS')
