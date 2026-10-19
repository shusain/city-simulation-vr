"""Original low-poly environment meshes; separate manifest avoids service-art edits.
Run Blender background with this script to render; add -- --export after review.
"""
from pathlib import Path
import bpy, json, math, sys
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 1
colors = {'Bark': (.22,.12,.055), 'Canopy': (.12,.32,.07), 'Rock': (.30,.29,.26), 'Rain': (.38,.58,.70), 'Snow': (.86,.92,.97)}
mats = {}
for name, color in colors.items():
    mat = bpy.data.materials.new(name); mat.diffuse_color = (*color,1); mat.use_nodes = True
    mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*color,1)
    mats[name] = mat
def cone(radius, depth, location, material, top=0):
    bpy.ops.mesh.primitive_cone_add(vertices=7, radius1=radius, radius2=top, depth=depth, location=location)
    obj=bpy.context.object; obj.data.materials.append(mats[material]); return obj
def ico(scale, location, material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=location)
    obj=bpy.context.object; obj.scale=scale; obj.data.materials.append(mats[material]); return obj
def join(name, parts):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts: obj.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    bpy.context.scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True); return obj
trees = [
    join('SM_Scenery_Broadleaf_A', [cone(.22,3.8,(0,0,1.9),'Bark',.12), ico((1.6,1.45,2),(0,0,4.1),'Canopy'), ico((1.1,1,1.3),(.7,.2,5),'Canopy')]),
    join('SM_Scenery_Conifer_A', [cone(.20,4,(0,0,2),'Bark',.10)] + [cone(r,h,(0,0,z),'Canopy') for r,h,z in [(1.6,3,3.1),(1.2,2.7,4.3),(.75,2.4,5.4)]]),
    join('SM_Scenery_Rock_A', [ico((1.7,1.3,1.2),(0,0,.6),'Rock')]),
    join('SM_Weather_Rain_A', [cone(.018,.9,(0,0,0),'Rain',.018)]),
    join('SM_Weather_Snow_A', [ico((.055,.055,.04),(0,0,0),'Snow')])]
# Palm foliage is solid faceted geometry with a shared canopy material, avoiding
# alpha cards. Eight arched fronds remain readable at normal tabletop scale.
def palm_frond(angle, length):
    vertices=[]; faces=[]
    for step in range(5):
        t=step/4; reach=length*t; width=.38*math.sin(math.pi*t)+.025
        z=6.0+.8*math.sin(math.pi*t)-1.05*t
        for side,lift in ((-1,0),(0,.09),(1,0),(0,-.04)):
            across=side*width
            vertices.append((.22+math.cos(angle)*reach-math.sin(angle)*across, math.sin(angle)*reach+math.cos(angle)*across,z+lift))
    for step in range(4):
        for side in range(4):
            faces.append((step*4+side,step*4+(side+1)%4,(step+1)*4+(side+1)%4,(step+1)*4+side))
    faces.extend(((3,2,1,0),(16,17,18,19)))
    mesh=bpy.data.meshes.new('PalmFrond');mesh.from_pydata(vertices,[],faces);mesh.materials.append(mats['Canopy'])
    obj=bpy.data.objects.new('PalmFrond',mesh);bpy.context.collection.objects.link(obj);return obj
palm_parts=[cone(.24,2.1,(0,0,1.05),'Bark',.19),cone(.19,2.0,(.09,0,3.05),'Bark',.14),cone(.14,2.0,(.22,0,5.05),'Bark',.10)]
palm_parts += [palm_frond(math.tau*i/8,2.6+(i%3)*.17) for i in range(8)]
trees.append(join('SM_Scenery_Palm_A',palm_parts))
# An eight-sided thin ground ring avoids translucent particles/overdraw.
vertices=[]; faces=[]
for i in range(8):
    a=math.tau*i/8
    for r in (.11,.15): vertices.append((math.cos(a)*r,math.sin(a)*r,0))
for i in range(8): faces.append((2*i,2*((i+1)%8),2*((i+1)%8)+1,2*i+1))
mesh=bpy.data.meshes.new('Splash'); mesh.from_pydata(vertices,[],faces); mesh.materials.append(mats['Rain'])
obj=bpy.data.objects.new('SM_Weather_Splash_A',mesh); bpy.context.collection.objects.link(obj); trees.append(obj)
entries=[]
for obj in trees:
    obj.data.calc_loop_triangles()
    entries.append(dict(name=obj.name, export='ArtSource/Exports/'+obj.name+'.fbx', triangles=len(obj.data.loop_triangles), material_slots=[m.name for m in obj.data.materials], provenance='Original project Blender primitives', collision='none'))
preview=root/'Artifacts/Preview/D64-Environment-Blender.png'; preview.parent.mkdir(exist_ok=True,parents=True)
if '--export' in sys.argv:
    assert preview.exists(), 'Render and review before export'
    for obj in trees:
        if '--scenery-v1' in sys.argv and obj.name != 'SM_Scenery_Palm_A': continue
        bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
        bpy.ops.export_scene.fbx(filepath=str(root/'ArtSource/Exports'/f'{obj.name}.fbx'), use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z', apply_unit_scale=True, bake_anim=False, add_leaf_bones=False)
    manifest_path=root/'ArtSource/Manifests/city_environment_kit.json'
    if '--scenery-v1' in sys.argv:
        previous=json.loads(manifest_path.read_text(encoding='utf-8'))
        entries=[entry for entry in previous['assets'] if entry['name']!='SM_Scenery_Palm_A']+[entry for entry in entries if entry['name']=='SM_Scenery_Palm_A']
    manifest_path.write_text(json.dumps({'schema_version':1,'assets':entries},indent=2)+'\n',encoding='utf-8')
else:
    bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityEnvironmentKit.blend'))
    for obj,x in zip(trees,(0,5,10,14,16,18,23)): obj.location.x=x
    scene=bpy.context.scene; scene.render.engine='BLENDER_EEVEE'; scene.render.resolution_x=1200; scene.render.resolution_y=800; scene.render.resolution_percentage=100; scene.world.color=(.18,.18,.18)
    bpy.ops.object.light_add(type='AREA',location=(8,-8,18)); bpy.context.object.data.energy=2000; bpy.context.object.data.size=15
    bpy.ops.object.camera_add(location=(23,-30,17)); cam=bpy.context.object; cam.rotation_euler=(Vector((11,0,2.5))-cam.location).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO';cam.data.ortho_scale=30;scene.camera=cam
    scene.render.filepath=str(preview); bpy.ops.render.render(write_still=True)
print('CITYSIM_ENVIRONMENT_ART', json.dumps(entries))
