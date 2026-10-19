"""Original sports parks; retain existing park sources and use two shared slots."""
from pathlib import Path
import bpy
import bmesh
import json
import math

root = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, beam, box, combine, cylinder, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 1
entries, report = [], []


def ring(name, radius, width, z, x=0, y=0, segments=12):
    vertices, faces = [], []
    for i in range(segments):
        angle = 2*math.pi*i/segments
        for height in (z-.025, z+.025):
            for r in (radius-width/2, radius+width/2):
                vertices.append((x+r*math.cos(angle), y+r*math.sin(angle), height))
    for i in range(segments):
        a, b = 4*i, 4*((i+1) % segments)
        faces.extend([(a,a+1,b+1,b), (a+2,b+2,b+3,a+3), (a,b,b+2,a+2), (a+1,a+3,b+3,b+1)])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(MATERIALS['Traffic'])
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def quarter_pipe(name, sign, y):
    # Closed extrusion, with a smooth six-panel riding curve matching activity sampling.
    vertices, faces = [], []
    profile = [(8+i, .36+.12*i*i) for i in range(6)] + [(13,.16),(8,.16)]
    for side in (-1,1):
        for x,z in profile:
            vertices.append((sign*x, y+side*3, z))
    n = len(profile)
    faces.extend([tuple(range(n-1,-1,-1)), tuple(range(n,2*n))])
    for i in range(n):
        j = (i+1) % n
        faces.append((i,j,n+j,n+i))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(MATERIALS['Traffic'])
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


for kind,cells in [('Basketball',5),('Skate',5),('Baseball',7),('Soccer',7)]:
    name = 'SM_Building_'+kind+'Park_A'
    side, half = cells*8, cells*4
    parts = [box(name+'_Lawn',(side,side,.12),(0,0,.06),'Park')]
    # Border promenade leaves all four entrances open and keeps the sport area clear.
    for axis in (0,1):
        for sign in (-1,1):
            dims, pos = [side-2,1.4,.12], [0,sign*(half-1),.18]
            if axis: dims[:2]=dims[:2][::-1]; pos[:2]=pos[:2][::-1]
            parts.append(box(name+'_Promenade',dims,pos,'Traffic'))
    for x,y in [(-half+4,-half+4),(half-4,half-4)]:
        parts.append(box(name+'_Trunk',(.6,.6,2.2),(x,y,1.22),'Traffic'))
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=2.2,location=(x,y,3.6))
        tree=bpy.context.object;tree.data.materials.append(MATERIALS['Park']);parts.append(tree)
    for x,y in [(-6,half-3),(6,half-3)]:
        parts.append(box(name+'_Bench',(3,1,.5),(x,y,.45),'Traffic'))
        parts.append(box(name+'_BenchBack',(3,.2,.8),(x,y+.4,.8),'Traffic'))
    if kind=='Basketball':
        parts.append(box(name+'_Court',(28,18,.16),(0,0,.2),'Park'))
        for x in (-14,14):parts.append(box(name+'_EndLine',(.18,18,.06),(x,0,.31),'Traffic'))
        for y in (-9,9):parts.append(box(name+'_SideLine',(28,.18,.06),(0,y,.31),'Traffic'))
        parts.append(box(name+'_CenterLine',(.18,18,.06),(0,0,.31),'Traffic'))
        parts.append(ring(name+'_CenterCircle',2,.18,.31))
        for sign in (-1,1):
            for y in (-3,3):parts.append(box(name+'_LaneLine',(5,.18,.06),(sign*11.5,y,.31),'Traffic'))
            parts.append(box(name+'_FreeThrowLine',(.18,6,.06),(sign*9,0,.31),'Traffic'))
            parts.append(box(name+'_HoopPost',(.35,.35,3.6),(sign*15,0,1.98),'Traffic'))
            parts.append(beam(name+'_HoopArm',(sign*15,0,3.65),(sign*13,0,3.65),.25,'Traffic'))
            parts.append(box(name+'_Backboard',(.18,2,1.25),(sign*13.3,0,3.5),'Traffic'))
            parts.append(ring(name+'_Hoop',.46,.09,3.05,sign*12.75,0,8))
    elif kind=='Skate':
        parts.append(box(name+'_Concrete',(28,19,.2),(0,0,.22),'Traffic'))
        parts += [quarter_pipe(name+'_WestRamp',-1,0), quarter_pipe(name+'_EastRamp',1,0)]
        # Separate flat lanes beside the half-pipe and a recognizable grind rail.
        parts.append(beam(name+'_GrindRail',(-6,6,1.05),(6,6,1.05),.22,'Traffic'))
        for x in (-5,5):parts.append(box(name+'_RailPost',(.22,.22,.8),(x,6,.65),'Traffic'))
        parts.append(box(name+'_ManualPad',(6,3,.65),(0,-6,.6),'Traffic'))
    elif kind=='Soccer':
        for x in (-22,22):parts.append(box(name+'_EndLine',(.18,30,.06),(x,0,.24),'Traffic'))
        for y in (-15,15):parts.append(box(name+'_SideLine',(44,.18,.06),(0,y,.24),'Traffic'))
        parts.append(box(name+'_HalfwayLine',(.18,30,.06),(0,0,.24),'Traffic'))
        parts.append(ring(name+'_CenterCircle',4,.18,.24))
        for sign in (-1,1):
            for y in (-6,6):parts.append(box(name+'_PenaltyLine',(6,.18,.06),(sign*19,y,.24),'Traffic'))
            parts.append(box(name+'_PenaltyLine',(.18,12,.06),(sign*16,0,.24),'Traffic'))
            for y in (-3,3):
                parts.append(beam(name+'_GoalPost',(sign*22,y,.2),(sign*22,y,2.6),.22,'Traffic'))
                parts.append(beam(name+'_GoalBack',(sign*22,y,2.6),(sign*24,y,.2),.12,'Traffic'))
            parts.append(beam(name+'_GoalCrossbar',(sign*22,-3,2.6),(sign*22,3,2.6),.22,'Traffic'))
            parts.append(beam(name+'_GoalRear',(sign*24,-3,.2),(sign*24,3,.2),.12,'Traffic'))
            for y in (-1,1):parts.append(beam(name+'_NetStrand',(sign*22,y,2.6),(sign*24,y,.2),.06,'Traffic'))
    else:
        diamond=[(-18,-18),(0,-18),(0,0),(-18,0)]
        for i,(x,y) in enumerate(diamond):
            parts.append(box(name+'_Base',(.85,.85,.12),(x,y,.22),'Traffic'))
            end=diamond[(i+1)%4]
            parts.append(beam(name+'_BaseLine',(x,y,.22),(*end,.22),.2,'Traffic'))
        parts.append(cylinder(name+'_PitchersMound',1.3,.3,(-9,-9,.3),'Traffic'))
        for end in ((24,-18),(-18,24)):
            parts.append(beam(name+'_FoulLine',(-18,-18,.22),(*end,.22),.2,'Traffic'))
        for i in range(12):
            a,b=i*math.pi/24,(i+1)*math.pi/24
            start=(-18+42*math.cos(a),-18+42*math.sin(a),1.5)
            end=(-18+42*math.cos(b),-18+42*math.sin(b),1.5)
            parts.append(beam(name+'_OutfieldRail',start,end,.16,'Traffic'))
            if i%3==0:parts.append(beam(name+'_FencePost',(start[0],start[1],.2),start,.2,'Traffic'))
        for i in range(3):parts.append(box(name+'_Bleachers',(7,1,.35),(7+i*.3,-23+i, .4+i*.35),'Traffic'))
    obj=combine(name,parts)
    bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
    obj.data.calc_loop_triangles()
    triangles=len(obj.data.loop_triangles)
    if triangles>800 or len(obj.data.materials)!=2:raise RuntimeError('Sports park budget exceeded: '+name+' '+str(triangles))
    export_one(obj)
    dimensions=[round(v,4) for v in obj.dimensions]
    entries.append(dict(asset_id='building.park.'+kind.lower(),civic_service='park',park_variant=kind.lower(),source='ArtSource/Blender/CitySportsParkKit.blend',export='ArtSource/Exports/'+name+'.fbx',dimensions_m=dimensions,footprint_cells=[cells,cells],mesh_destination='/Game/CitySim/Art/Meshes/'+name,material_slots={'Park':'/Game/CitySim/Art/Materials/MI_CityPark','Traffic':'/Game/CitySim/Art/Materials/MI_CityTraffic'},lod_policy='one shared sports park mesh; at most800 triangles',collision_policy='none; logical footprint is authoritative',pivot_policy='ground center',provenance='Original project Blender primitives; Create-CitySportsParkArt.py',entrance_sides_local=['-Y','+X','+Y','-X'],presentation_yaw_offset_quarter_turns=2))
    report.append(dict(name=name,triangles=triangles,dimensions_m=dimensions,slots=[m.name for m in obj.data.materials]))

# Shared skateboard prop used by three riders; origin is at its riding surface.
name='SM_Park_Skateboard_A'
parts=[box(name+'_Deck',(1.1,.35,.08),(0,0,.04),'Traffic')]
for x in (-.35,.35):
    for y in (-.18,.18):
        parts.append(box(name+'_Wheel',(.13,.09,.13),(x,y,-.065),'Roof'))
obj=combine(name,parts);export_one(obj)
entries.append(dict(asset_id='park.activity.skateboard',civic_service='park',animation_role='park_skateboard',footprint_cells=[0,0],source='ArtSource/Blender/CitySportsParkKit.blend',export='ArtSource/Exports/'+name+'.fbx',dimensions_m=[round(v,4) for v in obj.dimensions],mesh_destination='/Game/CitySim/Art/Meshes/'+name,material_slots={'Traffic':'/Game/CitySim/Art/Materials/MI_CityTraffic','Roof':'/Game/CitySim/Art/Materials/MI_CityRoof'},lod_policy='shared bounded park activity pool',collision_policy='none; cosmetic presentation',pivot_policy='riding surface center',provenance='Original project Blender primitives; Create-CitySportsParkArt.py'))
name='SM_Park_Ball_A'
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.5,location=(0,0,0))
obj=bpy.context.object;obj.name=name;obj.data.materials.append(MATERIALS['Traffic']);export_one(obj)
entries.append(dict(asset_id='park.activity.ball',civic_service='park',animation_role='park_ball',footprint_cells=[0,0],source='ArtSource/Blender/CitySportsParkKit.blend',export='ArtSource/Exports/'+name+'.fbx',dimensions_m=[round(v,4) for v in obj.dimensions],mesh_destination='/Game/CitySim/Art/Meshes/'+name,material_slots={'Traffic':'/Game/CitySim/Art/Materials/MI_CityTraffic'},lod_policy='shared20-triangle park activity pool',collision_policy='none; cosmetic presentation',pivot_policy='ball center',provenance='Original project Blender primitives; Create-CitySportsParkArt.py'))
bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CitySportsParkKit.blend'))
p=root/'ArtSource/Manifests/city_starter_kit.json'
manifest=json.loads(p.read_text(encoding='utf-8'))
ids={e['asset_id'] for e in entries}
manifest['assets']=[e for e in manifest['assets'] if e['asset_id'] not in ids]+entries
p.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(root/'Artifacts/AssetImport/D47-sports-geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('CITYSIM_SPORTS_PARK_ART_SAVED',report)
