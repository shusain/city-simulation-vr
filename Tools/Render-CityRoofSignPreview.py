"""Render the saved D79 roof/sign collection at close and whole-building scales."""
from pathlib import Path
import bpy,json,sys
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root/'ArtSource/Blender/CityBuildingSurfacePrototype.blend'))
manifest=json.loads((root/'ArtSource/Manifests/city_building_surface_prototype.json').read_text())
out=root/'Artifacts/ArtPreview/D79/Surfaces'
scene=bpy.context.scene;scene.cycles.samples=16
scene.render.resolution_x=scene.render.resolution_y=850
camera=scene.camera
details_only='--details-only' in sys.argv
def render(entry,suffix,aim,scale,offset=(30,-44,23)):
    obj=bpy.data.objects[Path(entry['export']).stem];obj.location=(0,0,0)
    for o in scene.objects:
        if o.type=='MESH':o.hide_render=o!=obj
    target=Vector(aim);camera.location=target+Vector(offset)
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=scale
    scene.render.filepath=str(out/(obj.name+'-'+suffix+'.png'))
    bpy.ops.render.render(write_still=True)
for e in manifest['assets']:
    if not details_only:render(e,'Whole',(0,0,e['dimensions_m'][2]*.48),max(27,e['dimensions_m'][2]*1.24))
    if not e['damaged']:
        if not details_only and (e['category'],e['density']) in (('Residential','Low'),('Commercial','Medium'),('Industrial','High')):
            z={('Residential','Low'):7,('Commercial','Medium'):21,('Industrial','High'):19}[e['category'],e['density']]
            render(e,'RoofSurface',(0,0,z),22 if e['category']!='Industrial' else 26,(24,-32,30))
        if e['geometry_refinement']:
            render(e,'Detail',(0,-4.5,11.5) if e['category']=='Commercial' else (0,-2,13 if e['density']=='Medium' else 19),
                   15 if e['category']=='Commercial' else 24)
        if (e['category'],e['density'])==('Residential','High'):
            render(e,'Wall',(0,-6.5,27),14);render(e,'Roof',(2.4,1.8,38.4),12)
for e in manifest['sign_variants']:
    if not details_only and e['density']=='Medium' and not e['damaged']:
        render(e,'Sign',(0,-4.5,11.5),13,(16,-44,16))
print('CITYSIM_ROOF_SIGN_PREVIEW_PASS',5 if details_only else 34,'renders')
