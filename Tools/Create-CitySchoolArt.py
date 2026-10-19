"""Original compact public school; two shared material slots and logical3x2 lot."""
from pathlib import Path
import bpy,json
root=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    box, combine, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=1.0
name='SM_Building_School_A'
parts=[box(name+'_Lot',(24,16,.14),(0,0,.07),'Roof'),box(name+'_Classrooms',(18,10,5.8),(0,1,3.04),'Trim'),box(name+'_Roof',(18.5,10.5,.4),(0,1,6.14),'Roof'),box(name+'_Entry',(3.2,2,3),(0,-4.8,1.64),'Trim'),box(name+'_Door',(1.6,.12,2.6),(0,-5.85,1.44),'Roof'),box(name+'_Steps',(4.2,2.4,.3),(0,-6.1,.29),'Trim'),box(name+'_Sign',(4.8,.3,.8),(0,-4.22,4.9),'Roof')]
# A simple book silhouette above the entry distinguishes the school from a station.
for x,angle in ((-.6,-.18),(.6,.18)):
 part=box(name+'_Book',(1.2,.22,.85),(x,-4.43,4.9),'Trim');part.rotation_euler.y=angle;parts.append(part)
for x in (-7,-4,4,7):
 for y in (-4.08,6.08):parts.append(box(name+'_Window',(1.8,.1,1.6),(x,y,3.5),'Roof'))
for x in (-10,10):parts.append(box(name+'_Bench',(1.4,3,.5),(x,-3,.4),'Trim'))
obj=combine(name,parts); obj.data.calc_loop_triangles(); count=len(obj.data.loop_triangles)
if count>800 or len(obj.data.materials)>2:raise RuntimeError('School geometry budget exceeded')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CitySchoolKit.blend'));export_one(obj)
dim=[round(v,4) for v in obj.dimensions]
entry={'asset_id':'building.school.a','civic_service':'school','source':'ArtSource/Blender/CitySchoolKit.blend','export':'ArtSource/Exports/'+name+'.fbx','dimensions_m':dim,'footprint_cells':[3,2],'mesh_destination':'/Game/CitySim/Art/Meshes/'+name,'material_slots':{'Trim':'/Game/CitySim/Art/Materials/MI_CityTrim','Roof':'/Game/CitySim/Art/Materials/MI_CityRoof'},'lod_policy':'one shared school mesh; below800 triangles','collision_policy':'none; logical footprint is authoritative','pivot_policy':'ground center','provenance':'Original project Blender primitives; Create-CitySchoolArt.py','entrance_sides_local':['-Y'],'presentation_yaw_offset_quarter_turns':2}
p=root/'ArtSource/Manifests/city_starter_kit.json';m=json.loads(p.read_text(encoding='utf-8'));m['assets']=[e for e in m['assets'] if e['asset_id']!=entry['asset_id']]+[entry];p.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8')
report=root/'Artifacts/AssetImport/D46-school-geometry.json';report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps({'triangles':count,'dimensions_m':dim,'slots':[m.name for m in obj.data.materials]},indent=2),encoding='utf-8')
print('CITYSIM_SCHOOL_ART_SAVED',count,dim)
