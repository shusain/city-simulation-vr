"""Original larger park meshes with two shared slots and four street entrances."""
from pathlib import Path
import bpy,json,math
root=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, box, combine, cylinder, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1.0
entries=[];report=[]
for kind,cells in [('Neighborhood',5),('City',7)]:
    name='SM_Building_'+kind+'Park_A';side=cells*8;half=side/2
    parts=[box(name+'_Lawn',(side,side,.12),(0,0,.06),'Park'),box(name+'_Plaza',(4,4,.18),(0,0,.21),'Trim')]
    # Four nonoverlapping arms reach each logical entrance without coplanar paths.
    length=half-2
    for axis in (0,1):
        for sign in (-1,1):
            pos=[0,0,.21];pos[axis]=sign*(2+length/2)
            dims=[3,3,.18];dims[axis]=length
            parts.append(box(name+'_Path',dims,pos,'Trim'))
    trees=[(-half+6,-half+6),(half-6,-half+6),(-half+6,half-6),(half-6,half-6),(-half+6,0),(half-6,0)]
    if kind=='City':trees += [(-10,-half+6),(10,-half+6),(-10,half-6),(10,half-6),(-half+6,-10),(half-6,10)]
    for i,(x,y) in enumerate(trees):
        # Move edge trees off the entrance axes so all four paths stay readable.
        if y==0:y=8
        parts.append(box(name+'_Trunk'+str(i),(.7,.7,3),(x,y,1.62),'Trim'))
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=2.8,location=(x,y,5))
        crown=bpy.context.object;crown.name=name+'_Crown'+str(i);crown.scale.z=1.15;crown.data.materials.append(MATERIALS['Park']);parts.append(crown)
    for x,y in [(-7,-6),(7,6),(-7,6),(7,-6)]:
        parts.append(box(name+'_Bench',(3.2,1,.5),(x,y,.47),'Trim'))
        parts.append(box(name+'_BenchBack',(3.2,.2,1),(x,y+.45,.82),'Trim'))
    if kind=='Neighborhood':
        parts.append(cylinder(name+'_Fountain',2.1,.55,(0,0,.575),'Trim'))
        parts.append(cylinder(name+'_FountainCore',.45,1.8,(0,0,1.4),'Trim'))
    else:
        parts.append(box(name+'_PlayCourt',(8,10,.12),(12,-12,.2),'Trim'))
        parts.append(box(name+'_CourtInset',(7.4,9.4,.08),(12,-12,.3),'Park'))
        parts.append(box(name+'_PavilionBase',(7,7,.35),(-10,-10,.295),'Trim'))
        for x,y in [(-13,-13),(-7,-13),(-13,-7),(-7,-7)]:parts.append(box(name+'_PavilionPost',(.5,.5,3.2),(x,y,2.07),'Trim'))
        parts.append(box(name+'_PavilionRoof',(7.5,7.5,.4),(-10,-10,3.87),'Trim'))
    obj=combine(name,parts);obj.data.calc_loop_triangles();count=len(obj.data.loop_triangles)
    if count>800 or len(obj.data.materials)>2:raise RuntimeError('Park geometry budget exceeded: '+name)
    export_one(obj)
    dimensions=[round(v,4) for v in obj.dimensions]
    entries.append({'asset_id':'building.park.'+kind.lower(),'civic_service':'park','park_variant':kind.lower(),'source':'ArtSource/Blender/CityParkKit.blend','export':'ArtSource/Exports/'+name+'.fbx','dimensions_m':dimensions,'footprint_cells':[cells,cells],'mesh_destination':'/Game/CitySim/Art/Meshes/'+name,'material_slots':{'Park':'/Game/CitySim/Art/Materials/MI_CityPark','Trim':'/Game/CitySim/Art/Materials/MI_CityTrim'},'lod_policy':'one shared park mesh; at most800 triangles','collision_policy':'none; logical footprint is authoritative','pivot_policy':'ground center','provenance':'Original project Blender primitives; Create-CityParkArt.py','entrance_sides_local':['-Y','+X','+Y','-X'],'presentation_yaw_offset_quarter_turns':2})
    report.append({'name':name,'triangles':count,'dimensions_m':dimensions,'slots':[m.name for m in obj.data.materials]})
bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityParkKit.blend'))
p=root/'ArtSource/Manifests/city_starter_kit.json';m=json.loads(p.read_text(encoding='utf-8'));ids={e['asset_id'] for e in entries};m['assets']=[e for e in m['assets'] if e['asset_id'] not in ids]+entries;p.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8')
(root/'Artifacts/AssetImport/D47-park-geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('CITYSIM_PARK_ART_SAVED',report)
