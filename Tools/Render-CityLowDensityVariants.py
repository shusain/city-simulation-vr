"""D103 common-camera comparisons using actual packed Blender surfaces."""
from pathlib import Path
import argparse,json,sys,math
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import digest
from CityAssetPreview import preview_scene,studio_lights


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory',type=Path,default=ROOT/'Artifacts/D103')
    parser.add_argument('--refresh',action='store_true')
    parser.add_argument('--reuse-verification',type=Path)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    out=args.output_directory.absolute()
    study=json.loads((out/'Study.json').read_text())
    source=ROOT/study['source']
    before=digest(source)
    refresh=args.refresh
    unchanged=set()
    if refresh:
        first=json.loads((args.reuse_verification or out/'FirstSourceGeometry.json').read_text())
        final=json.loads((out/'SourceVerification.json').read_text())
        unchanged={name for name,value in final['objects'].items() if first['objects'].get(name)==value}
    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene=bpy.context.scene
    scene.render.engine='CYCLES'
    scene.cycles.samples=24
    scene.cycles.use_denoising=True
    scene.render.resolution_x=scene.render.resolution_y=800
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.view_settings.view_transform='AgX'
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.20,.25,.33,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.65
    camera=bpy.data.objects.new('D103_ReviewCamera',bpy.data.cameras.new('D103_ReviewCamera'))
    scene.collection.objects.link(camera)
    scene.camera=camera
    camera.data.type='ORTHO'
    light_objects=studio_lights(scene,[Vector((10,10,10))])
    bpy.ops.mesh.primitive_plane_add(size=160,location=(0,0,-.215))
    ground=bpy.context.object
    ground.name='D103_ReviewGround'
    material=bpy.data.materials.new('D103_ReviewGround')
    material.use_nodes=True
    shader=material.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value=(.16,.19,.23,1)
    shader.inputs['Roughness'].default_value=.85
    ground.data.materials.append(material)
    view_specs=[('iso',(23,-32,21)),('front',(0,-40,8)),('back',(-23,32,21))]
    if study.get('revision',1)>=3:view_specs.append(('entrance',(2,-18,4)))
    render_count=0
    for view,offset in view_specs:
        destination=out/'Preview'/view
        destination.mkdir(parents=True,exist_ok=True)
        old_manifest=destination/'PreviewManifest.json'
        old_entries={entry['object']:entry for entry in json.loads(old_manifest.read_text())['views']} if old_manifest.is_file() else {}
        rows=[]
        for index,record in enumerate(study['assets'],1):
            if view=='entrance' and not record.get('entrance'):continue
            obj=bpy.data.objects[record['name']]
            citizen=bpy.data.objects['Ruler_'+obj.name]
            saved=obj.location.copy()
            citizen_offset=citizen.location-obj.location
            obj.location=(0,0,0)
            citizen.location=citizen_offset
            ground.location.z=record['bounds_min_m'][2]-.015
            for mesh in scene.objects:
                if mesh.type=='MESH':mesh.hide_render=mesh not in (obj,citizen,ground)
            height={'Home':2.2,'Shop':5.0,'Factory':7.4}[record['category']]
            target=Vector((0,-.9,height)) if view!='entrance' else Vector((0 if record['category']=='Shop' else .25,-7.7,1.85))
            camera.location=target+Vector(offset)
            camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
            camera.data.ortho_scale={'Home':20,'Shop':28,'Factory':32}[record['category']] if view!='entrance' else (7.1 if record['category']=='Shop' else 13.5)
            bpy.context.view_layer.update()
            if view=='entrance':
                low,high=record['entrance']['detail_bounds_m']
                points=[Vector((x,y,z)) for x in (low[0],high[0]) for y in (low[1],high[1]) for z in (low[2],high[2])]
            else:points=[obj.matrix_world@v.co for v in obj.data.vertices]
            projected=[world_to_camera_view(scene,camera,p) for p in points]
            assert all(.015<=v.x<=.985 and .015<=v.y<=.985 and v.z>0 for v in projected),(obj.name,'cropped')
            image=destination/f'{index:02}_{obj.name}_{view}.png'
            scene.render.filepath=str(image)
            old=old_entries.get(obj.name,{})
            reusable=refresh and obj.name in unchanged and image.is_file() and abs(old.get('floor_z_m',-.215)-ground.location.z)<1e-5
            if not reusable:
                bpy.ops.render.render(write_still=True)
            rows.append(dict(object=obj.name,view=view,image=image.name,sha256=digest(image),
                             dimensions_m=record['dimensions_m'],triangles=record['triangles'],
                             ortho_scale=camera.data.ortho_scale,citizen_height_m=1.43175,
                             floor_z_m=ground.location.z,reused_verified_unchanged_geometry=bool(reusable)))
            obj.location=saved
            citizen.location=saved+citizen_offset
            render_count+=1
        (destination/'PreviewManifest.json').write_text(json.dumps(dict(schema_version=1,source=str(source),source_sha256=before,
            source_unchanged=True,scene=scene.name,frame=scene.frame_current,engine='cycles',color='material',resolution=800,
            views=rows,limitations='Common camera per category, actual Blender surfaces and accepted 1.43175m citizen ruler; no native/device claim.'),indent=2)+'\n')
    assert digest(source)==before
    print('CITYSIM_D103_PREVIEWS_PASS',render_count,'renders; common category scale; source unchanged')


if __name__=='__main__':main()
