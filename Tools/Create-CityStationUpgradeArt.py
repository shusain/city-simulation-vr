"""Author new station stages in a separate source; preserve accepted starter assets."""
from pathlib import Path
import json
import bpy

root=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(root / 'Tools'))
from CityArtKit import box, combine, cylinder, export_one
from CityCivicMeshes import emergency_vehicle, initialize_civic_materials
initialize_civic_materials()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=1.0

def expansion(name,slot,stage):
    fire=slot=='Fire'
    parts=[box(name+'_Lot',(24,16,.14),(0,0,.07),'Roof'),
           box(name+'_Body',(14,10,6),(-4,1,3),slot),
           box(name+'_Roof',(14.6,10.6,.5),(-4,1,6.25),'Roof'),
           box(name+'_Driveway',(20,6.4,.16),(0,-7.2,.08),'Roof'),
           box(name+'_Entry',(2.2,.3,3.4),(-8,-4.18,1.7),'Roof'),
           box(name+'_Sign',(4,.5,.8),(-7,-4.35,4.9),slot),
           box(name+'_GarageWing',(7.4,10,5.1),(7.2,1,2.69),slot),
           box(name+'_GarageRoof',(7.8,10.4,.4),(7.2,1,5.44),'Roof')]
    for x in (0,5.5,9):
        parts.append(box(name+'_Bay',(3,.14,4),(x,-4.09,2.18),'Roof'))
        for z in (.9,1.8,2.7,3.6):
            parts.append(box(name+'_BayRib',(2.8,.08,.08),(x,-4.19,z),slot))
    for x in (-9,-5,-1):
        parts.append(box(name+'_BackWindow',(2,.12,1.5),(x,6.08,3.6),'Roof'))
    parts.append(box(name+'_FrontWindow',(2,.12,1.6),(-4,-4.08,3.5),'Roof'))
    if fire:
        parts += [box(name+'_Tower',(3,3,9),(-8,4,4.5),slot),box(name+'_TowerCap',(3.4,3.4,.4),(-8,4,9.2),'Roof')]
    else:
        parts += [cylinder(name+'_Antenna',.12,2.5,(-7,4,7.75),'Roof')]
    if stage==2:
        parts += [box(name+'_DepartmentWing',(13,6,3),(-4,2.5,8.02),slot),box(name+'_WingRoof',(13.5,6.5,.4),(-4,2.5,9.73),'Roof')]
        for x in (-8,-4,0):
            parts.append(box(name+'_WingWindow',(2,.12,1.3),(x,-.57,8.2),'Roof'))
    for i,x in enumerate((-7,7) if stage==1 else (-7,0,7)):
        parked=emergency_vehicle(name+'_Parked'+str(i),slot,fire,ladder_rungs=False)
        parked.rotation_euler.z=-1.57079632679
        parked.location=(x,-7.25,.17)
        bpy.context.view_layer.objects.active=parked
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=False)
        parts.append(parked)
    for x in (-10,-3.5,3.5,10):
        parts.append(box(name+'_ParkingStripe',(.08,5.9,.025),(x,-7.25,.175),'Roof'))
    return combine(name,parts)

manifest_path=root/'ArtSource/Manifests/city_starter_kit.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
entries=[]
measurements=[]
for service in ('police','fire'):
    base=next(e for e in manifest['assets'] if e['asset_id']==f'building.{service}.a')
    for stage,suffix in ((1,'Medium'),(2,'High')):
        name=f'SM_Building_{service.title()}_{suffix}_A'
        obj=expansion(name,service.title(),stage)
        obj.data.calc_loop_triangles()
        count=len(obj.data.loop_triangles)
        if count>1500 or len(obj.data.materials)>2:raise RuntimeError(f'Station stage budget exceeded: {name} {count} triangles')
        export_one(obj)
        entry=dict(base)
        entry.update(asset_id=f'building.{service}.{suffix.lower()}.a',source='ArtSource/Blender/CityStationUpgradeKit.blend',
                     export=f'ArtSource/Exports/{name}.fbx',mesh_destination=f'/Game/CitySim/Art/Meshes/{name}',
                     dimensions_m=[round(v,4) for v in obj.dimensions],civic_stage=suffix.lower(),station_upgrade=True,
                     parking_policy=f'{stage+1} decorative parked vehicles; no dispatch authority',
                     provenance='Original project Blender primitives; Create-CityStationUpgradeArt.py',
                     lod_policy='shared civic expansion mesh; below1500 triangles')
        entries.append(entry)
        measurements.append({'name':name,'triangles':count,'slots':[m.name for m in obj.data.materials],'dimensions_m':entry['dimensions_m']})
        obj.location.x=len(entries)*35
bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityStationUpgradeKit.blend'))
manifest['assets']=[e for e in manifest['assets'] if not e.get('station_upgrade')]+entries
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(root/'Artifacts/AssetImport/D48-station-geometry.json').write_text(json.dumps(measurements,indent=2),encoding='utf-8')
print('CITYSIM_STATION_UPGRADE_ART_READY',len(entries))
