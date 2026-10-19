"""D71 original simplified human proportions, kept outside runtime selection.

Blender --background --factory-startup --python this-file creates three
ordinary-clothing static prototypes and paired simpler meshes. One vertex-color
slot, no textures, skeletons, ticking Actors or changes to SM_Citizen_A.
"""
from pathlib import Path
import bpy, bmesh, json, math
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/ArtPreview/D71'
EXPORT=ROOT/'ArtSource/Exports/CitizenPrototypes'
SOURCE=ROOT/'ArtSource/Blender/CityCitizenPrototype.blend'
OUT.mkdir(parents=True,exist_ok=True)
EXPORT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
palette=bpy.data.materials.new('CitizenVertexPalette')
palette.diffuse_color=(.65,.65,.65,1)

def person(name,style,detail):
    verts=[];faces=[];colors=[]
    segments=8 if detail else 6
    skin,shirt,pants,hair=style
    shoes=(.065,.075,.085,1)
    def loft(rings,color,n=segments):
        start=len(verts)
        for x,y,z,rx,ry in rings:
            verts.extend((x+rx*math.cos(2*math.pi*j/n),y+ry*math.sin(2*math.pi*j/n),z) for j in range(n))
        newfaces=[tuple(start+j for j in range(n-1,-1,-1))]
        for i in range(len(rings)-1):
            newfaces += [(start+i*n+j,start+i*n+(j+1)%n,start+(i+1)*n+(j+1)%n,start+(i+1)*n+j) for j in range(n)]
        newfaces.append(tuple(start+(len(rings)-1)*n+j for j in range(n)))
        faces.extend(newfaces);colors.extend([color]*len(newfaces))
    # About seven and a half head lengths: shoulder/hip taper and limb lengths
    # are human proportions rather than an enlarged capsule head.
    loft([(0,0,.88,.15,.095),(0,0,.99,.165,.10)],pants)
    loft([(0,0,.96,.16,.095),(0,0,1.08,.145,.095),
          (0,0,1.30,.185,.11),(0,0,1.39,.215,.095),
          (0,0,1.44,.15,.075)],shirt)
    loft([(0,0,1.43,.055,.05),(0,0,1.52,.055,.05)],skin)
    head=[(0,-.004,1.49,.046,.058),(0,-.014,1.53,.071,.075),
          (0,0,1.60,.088,.092),(0,0,1.68,.086,.092),
          (0,.008,1.735,.062,.070),(0,.01,1.755,.025,.03)]
    loft(head if detail else [head[i] for i in (0,2,3,5)],skin,n=10 if detail else 6)
    # A restrained hair cap; no large cartoon eyes or fingers at city scale.
    loft([(0,.008,1.675,.092,.102),(0,.012,1.740,.069,.078),
          (0,.014,1.765,.026,.034)],hair)
    for side in (-1,1):
        # A relaxed stride is a static pose, not an animation or gait claim.
        y=-.07*side
        loft([(side*.088,y,.09,.05,.052),(side*.085,y*.6,.44,.057,.062),
              (side*.087,0,.88,.081,.078)],pants)
        loft([(side*.088,y-.055,0,.060,.118),
              (side*.088,y-.055,.10,.056,.10)],shoes)
        loft([(side*.185,0,1.43,.048,.052),
              (side*.22,0,1.37,.058,.064),
              (side*.25,.025*side,1.17,.048,.05)],shirt)
        forearm_color=skin if name.endswith('Casual') else shirt
        loft([(side*.25,.025*side,1.18,.048,.05),
              (side*.27,.07*side,.98,.031,.035)],forearm_color)
        loft([(side*.27,.07*side,.99,.033,.035),
              (side*.27,.07*side,.90,.027,.03)],skin)
        if detail:
            loft([(side*.09,0,1.565,.016,.027),
                  (side*.09,0,1.62,.016,.027)],skin,n=6)
    if detail:
        for side in (-1,1):
            loft([(side*.035,-.089,1.623,.012,.006),
                  (side*.035,-.091,1.635,.013,.007)],(.065,.058,.05,1),n=6)
        loft([(0,-.083,1.549,.023,.004),(0,-.083,1.555,.021,.004)],
             tuple(c*.65 if i<3 else c for i,c in enumerate(skin)),n=6)
        # Small nose wedge and shirt placket: readable up close, not busy afar.
        start=len(verts)
        verts += [(-.016,-.085,1.63),(.016,-.085,1.63),
                  (.018,-.087,1.58),(-.018,-.087,1.58),(0,-.117,1.593)]
        faces += [(start,start+1,start+4),(start+1,start+2,start+4),
                  (start+2,start+3,start+4),(start+3,start,start+4),
                  (start+3,start+2,start+1,start)]
        colors += [skin]*5
        loft([(0,-.101,1.12,.006,.002),(0,-.111,1.31,.006,.002)],
             tuple(c*.85 if i<3 else c for i,c in enumerate(shirt)),n=4)
    # Facing +X matches traffic Direction.Rotation().Yaw in the current pools.
    verts=[(-y,x,z) for x,y,z in verts]
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(verts,[],faces);mesh.materials.append(palette)
    attr=mesh.color_attributes.new(name='CitizenPalette',type='BYTE_COLOR',domain='CORNER')
    for polygon,color in zip(mesh.polygons,colors):
        polygon.use_smooth=True
        for loop in polygon.loop_indices:attr.data[loop].color=color
    mesh.color_attributes.active_color=attr
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    assert not any(e.is_boundary for e in bm.edges)
    assert not any(f.calc_area()<1e-8 for f in bm.faces)
    bm.to_mesh(mesh);bm.free()
    mesh.calc_loop_triangles()
    assert len(mesh.loop_triangles)<=1000
    return obj

styles={
    'Casual':((.64,.39,.25,1),(.16,.40,.53,1),(.12,.16,.22,1),(.075,.055,.045,1)),
    'Office':((.79,.58,.43,1),(.55,.60,.65,1),(.11,.14,.18,1),(.12,.09,.065,1)),
    'Worker':((.40,.24,.15,1),(.65,.29,.075,1),(.15,.20,.23,1),(.055,.042,.035,1)),
}
assets=[];entries=[]
for label,style in styles.items():
    for detail in (True,False):
        name='SM_Citizen_PC_'+label+('' if detail else '_LOD1')
        # Keep the style suffix for forearm color selection on the simpler mesh.
        obj=person('SM_Citizen_PC_'+label,style,detail)
        obj.name=name;obj.data.name=name
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
        bpy.context.view_layer.objects.active=obj
        path=EXPORT/(name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',mesh_smooth_type='FACE',
            axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
        assets.append(obj)
        entries.append(dict(name=name,source=str(SOURCE.relative_to(ROOT)).replace('\\','/'),
            export=str(path.relative_to(ROOT)).replace('\\','/'),triangles=len(obj.data.loop_triangles),
            material_slots=['CitizenVertexPalette'],color_attribute='CitizenPalette',
            bounds_min_m=[min(v.co[i] for v in obj.data.vertices) for i in range(3)],
            bounds_max_m=[max(v.co[i] for v in obj.data.vertices) for i in range(3)],
            forward_axis='+X',pivot_policy='feet at z=0; centered local origin',
            collision_policy='none; presentation prototype',prototype_only=True,
            lod_role='LOD0' if detail else 'LOD1 candidate (not automatically selected)',
            provenance='Original project lofted geometry; Tools/Create-CityCitizenPrototype.py'))

for i,obj in enumerate(assets):obj.location=(i//2*1.15,i%2*1.8,0)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
manifest=dict(schema_version=1,task='D71',runtime_selected=False,assets=entries,
    material_integration='Requires a shared vertex-color base-color material; no new texture or custom shader',
    scale_integration='Normalize to current walker world-height for comparison; do not apply the 1.76m source at the old 0.72 scale without review',
    animation='Static relaxed stride only; existing pool movement can translate it. No gait animation.',
    source_license='Original project assets; no third-party dependencies')
(ROOT/'ArtSource/Manifests/city_citizen_prototype.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')

# Equal apparent heights for the comparison; the actual SM_Citizen_A is not changed.
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(ROOT/'ArtSource/Exports/SM_Citizen_A.fbx'))
baseline=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
baseline.data.calc_loop_triangles()
baseline.location=(-1.15,0,0)
baseline.scale*=1.765/baseline.dimensions.z
bpy.context.view_layer.update()
baseline.location.z=-min((baseline.matrix_world@v.co).z for v in baseline.data.vertices)
baseline.location.x=0;baseline.location.y=-1.15
for i,obj in enumerate(assets):obj.location=(0,i//2*1.15,0)
for obj in assets: obj.hide_render=obj.name.endswith('LOD1')
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=1600;scene.render.resolution_y=850;scene.render.resolution_percentage=100
scene.display.shading.light='STUDIO';scene.display.shading.color_type='VERTEX'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD';scene.world.color=(.11,.13,.16)
scene.view_settings.view_transform='Standard'
cam=bpy.data.cameras.new('D71ReviewCamera');camera=bpy.data.objects.new('D71ReviewCamera',cam)
scene.collection.objects.link(camera);scene.camera=camera;cam.type='ORTHO';cam.ortho_scale=5.2
aim=Vector((0,.6,.88));camera.location=aim+Vector((8,0,.9))
camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(OUT/'Citizen-Comparison.png');bpy.ops.render.render(write_still=True)
baseline.hide_render=True
for obj in assets: obj.hide_render=True
hero=assets[0];hero.location=(0,0,0);hero.hide_render=False
aim=Vector((0,0,.88));camera.location=aim+Vector((6,-3,1.2))
camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();cam.ortho_scale=2.1
scene.render.resolution_x=scene.render.resolution_y=900
scene.render.filepath=str(OUT/'Citizen-Casual-Close.png');bpy.ops.render.render(write_still=True)
print('CITYSIM_D71_PROTOTYPE_VALIDATED',json.dumps([(e['name'],e['triangles']) for e in entries]),
      'baseline_triangles',len(baseline.data.loop_triangles))
