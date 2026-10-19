"""D79 shared brick surfaces on nine accepted R/C/I building forms.

Uses separate source/exports/manifest; never overwrites accepted D79 geometry,
the main art manifest, Unreal production meshes, or benchmark packages.
Blender --background --factory-startup --python Tools/...py
"""
from pathlib import Path
import bpy, bmesh, hashlib, json, math, sys
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'Tools'))
from CityBuildingSurfaceGeometry import refine, REFINED_FORMS
from CityBuildingRoofSigns import RoofSigns
focused_renders='--refinements-only' in sys.argv
skip_renders='--skip-renders' in sys.argv
geometry_output=root/'ArtSource/Exports/BuildingSurfaceGeometry'
geometry_output.mkdir(parents=True,exist_ok=True)
output = root / 'ArtSource/Exports/BuildingSurfacePrototypes'
preview = root / 'Artifacts/ArtPreview/D79/Surfaces'
output.mkdir(parents=True, exist_ok=True); preview.mkdir(parents=True, exist_ok=True)
source = root / 'ArtSource/Blender/CityBuildingSurfacePrototype.blend'
manifest_path = root / 'ArtSource/Manifests/city_building_surface_prototype.json'
originals = json.loads((root/'ArtSource/Manifests/city_starter_kit.json').read_text(encoding='utf-8'))['assets']
originals=[e.get('surface_source_entry',e) for e in originals]
textures = root/'ArtSource/Textures/BuildingSurfaces'
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1
scene.render.engine = 'CYCLES'; scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.18,.22,.28,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .65

def material(name, color, texture_prefix=None, mechanical=False, texture_dir=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    m.diffuse_color = (*color, 1)
    nodes = m.node_tree.nodes; links = m.node_tree.links
    p = nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value = (*color,1)
    p.inputs['Roughness'].default_value = .7
    if not texture_prefix: return m
    def image(suffix):
        n = nodes.new('ShaderNodeTexImage')
        n.image = bpy.data.images.load(str((texture_dir or textures)/(texture_prefix+'_'+suffix+'.png')), check_existing=True)
        if suffix != 'BaseColor': n.image.colorspace_settings.name = 'Non-Color'
        n.extension = 'REPEAT'; return n
    base = image('BaseColor'); normal = image('Normal'); orm = image('ORM')
    tint = nodes.new('ShaderNodeMixRGB'); tint.blend_type = 'MULTIPLY'; tint.inputs[0].default_value = 1
    links.new(base.outputs['Color'], tint.inputs[1]); tint.inputs[2].default_value = (*color,1)
    links.new(tint.outputs[0], p.inputs['Base Color'])
    split = nodes.new('ShaderNodeSeparateColor'); links.new(orm.outputs['Color'], split.inputs[0])
    links.new(split.outputs['Green'], p.inputs['Roughness'])
    # DirectX -> OpenGL tangent normal: green becomes 1-green.
    s = nodes.new('ShaderNodeSeparateColor'); links.new(normal.outputs[0], s.inputs[0])
    invert = nodes.new('ShaderNodeMath'); invert.operation = 'SUBTRACT'; invert.inputs[0].default_value = 1
    links.new(s.outputs['Green'], invert.inputs[1])
    c = nodes.new('ShaderNodeCombineColor')
    links.new(s.outputs['Red'], c.inputs['Red']); links.new(invert.outputs[0], c.inputs['Green']); links.new(s.outputs['Blue'], c.inputs['Blue'])
    n = nodes.new('ShaderNodeNormalMap'); links.new(c.outputs[0], n.inputs['Color']); links.new(n.outputs[0], p.inputs['Normal'])
    if mechanical:
        attr = nodes.new('ShaderNodeVertexColor'); attr.layer_name = 'WindowMask'
        mask = nodes.new('ShaderNodeSeparateColor'); links.new(attr.outputs['Color'], mask.inputs[0])
        links.new(mask.outputs['Red'], p.inputs['Metallic'])
        paint = nodes.new('ShaderNodeMixRGB'); paint.inputs[1].default_value = (*color,1)
        paint.inputs[2].default_value = ((.20,.18,.16,1) if name=='CharredMechanical' else (.68,.72,.76,1))
        links.new(mask.outputs['Red'], paint.inputs[0]); links.new(paint.outputs[0], tint.inputs[2])
        rough = nodes.new('ShaderNodeMixRGB')
        rough.inputs[1].default_value = ((.94,.94,.94,1) if name=='CharredMechanical' else (.7,.7,.7,1))
        links.new(split.outputs['Green'], rough.inputs[2]); links.new(mask.outputs['Red'], rough.inputs[0]); links.new(rough.outputs[0], p.inputs['Roughness'])
    return m

wall = material('ResidentialBrick', (.22,.58,.32), 'CityBrick')
commercial_wall = material('CommercialBrick', (.12,.42,.78), 'CityBrick')
industrial_wall = material('IndustrialBrick', (.86,.64,.10), 'CityBrick')
roof = material('Roof', (.10,.14,.19))
hvac = material('Mechanical', (.22,.58,.32), 'CityMechanical', True)
industrial_hvac = material('IndustrialMechanical', (.86,.64,.10), 'CityMechanical', True)
char_wall = material('CharredBrick', (.11,.095,.08), 'CityBrick')
char_roof = material('CharredTrim', (.065,.06,.055))
char_hvac = material('CharredMechanical', (.12,.10,.08), 'CityMechanical', True)
roof_signs = RoofSigns(root, material, char_roof)

camera_data = bpy.data.cameras.new('SurfaceReviewCamera'); camera_data.type = 'ORTHO'
camera = bpy.data.objects.new('SurfaceReviewCamera', camera_data); scene.collection.objects.link(camera); scene.camera = camera
sun_data = bpy.data.lights.new('ReviewSun', 'SUN'); sun_data.energy = 3; sun_data.angle = .15
sun = bpy.data.objects.new('ReviewSun', sun_data); scene.collection.objects.link(sun); sun.rotation_euler = (.5,-.45,-.6)

def render(obj, name, aim, scale):
    for o in scene.objects:
        if o.type=='MESH': o.hide_render = o != obj
    aim = Vector(aim); camera.location = aim + Vector((30,-44,23))
    camera.rotation_euler = (aim-camera.location).to_track_quat('-Z','Y').to_euler()
    camera_data.ortho_scale = scale
    scene.render.filepath = str(preview/(name+'.png')); bpy.ops.render.render(write_still=True)

def geometry(obj):
    return sorted(tuple(round(c,6) for c in v.co) for v in obj.data.vertices), sorted(
        tuple(sorted(tuple(round(c,6) for c in obj.data.vertices[i].co) for i in p.vertices)) for p in obj.data.polygons)

assets = []; objects = []
families = (('residential','Residential',wall), ('shop','Commercial',commercial_wall),
            ('factory','Industrial',industrial_wall))
variants = [(family,category,wall_material,density,damaged)
            for family,category,wall_material in families
            for density in ('Low','Medium','High') for damaged in (False,True)]
for family,category,wall_material,density,damaged in variants:
    base_id = 'building.'+family+('.'+density.lower() if density!='Low' else '')+'.a'
    entry = next(e for e in originals if e['asset_id']==base_id+('.damaged' if damaged else ''))
    bpy.ops.import_scene.fbx(filepath=str(root/entry['export']))
    obj = next(o for o in bpy.context.selected_objects if o.type=='MESH')
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    reference_path=root/entry['export']; refinement=None
    if (category,density) in REFINED_FORMS:
        refined_state,refinement=refine(obj,category,density,damaged,refined_state if damaged else None)
        reference_path=geometry_output/('SM_'+category+density+'_SurfaceGeometry'+('_Damaged' if damaged else '')+'.fbx')
        bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(reference_path),use_selection=True,object_types={'MESH'},
            apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',
            bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
    accepted = geometry(obj)
    original_slots = [m.name.split('.')[0] for m in obj.data.materials]
    original_indices = [p.material_index for p in obj.data.polygons]
    colors = obj.data.color_attributes.get('WindowMask') or obj.data.color_attributes[0]
    original_alpha = [c.color[3] for c in colors.data]
    obj.data.calc_loop_triangles(); triangle_count = len(obj.data.loop_triangles)
    tower = family=='residential' and density=='High'
    factory = family=='factory'
    has_equipment = tower or factory
    # Find rooftop components using the intact mesh; damage keeps vertex order.
    if not damaged:
        bm = bmesh.new(); bm.from_mesh(obj.data); bm.verts.ensure_lookup_table()
        remaining = set(bm.verts); component_info = {}; flat_roof_vertices = set()
        while remaining:
            pending = [remaining.pop()]; component = []
            while pending:
                vertex = pending.pop(); component.append(vertex)
                for edge in vertex.link_edges:
                    other = edge.other_vert(vertex)
                    if other in remaining: remaining.remove(other); pending.append(other)
            low = [min(v.co[i] for v in component) for i in range(3)]
            high = [max(v.co[i] for v in component) for i in range(3)]
            size = [high[i]-low[i] for i in range(3)]
            if all(original_slots[f.material_index]=='MetalDetail' for v in component for f in v.link_faces):
                for v in component: component_info[v.index]=1
            elif tower and low[2] >= 35.999:
                painted = (abs(size[2]-4)<.001 or (abs(size[2]-2.3)<.001 and min(size[:2])<.11) or abs(size[2]-3)<.001)
                for v in component: component_info[v.index] = 0 if painted else 1
            elif factory and low[2] >= {'Low':7,'Medium':12,'High':17}[density]-.001:
                # Only category-colored roof/equipment components are selected;
                # clerestory windows and loading details keep their window mask.
                if all(original_slots[f.material_index]=='Industrial' for v in component for f in v.link_faces):
                    if abs(size[2]-1)<.001 and size[0]>15.9:
                        flat_roof_vertices.update(v.index for v in component)
                    else:
                        # Round stacks expose metal; large box equipment is paint.
                        exposed = size[0]<2.5 and size[1]<2.5
                        for v in component: component_info[v.index] = 1 if exposed else 0
        bm.free()
        # Face UVs are planar in world metres and reused intact for sagged damage.
        intact_coords = [v.co.copy() for v in obj.data.vertices]
        intact_normals = [p.normal.copy() for p in obj.data.polygons]
        intact_topology = [tuple(sorted(p.vertices)) for p in obj.data.polygons]
        section_indices = []
        for p,index in zip(obj.data.polygons,original_indices):
            equipment = all(v in component_info for v in p.vertices)
            flat_roof = all(v in flat_roof_vertices for v in p.vertices)
            # Factory masonry belongs to the hall, not stacks, tanks or roofs.
            horizontal_cap = not tower and abs(p.normal.z)>.5
            section_indices.append(2 if equipment else 0 if original_slots[index]==category and not(flat_roof or horizontal_cap) else 1)
    else:
        assert intact_topology==[tuple(sorted(p.vertices)) for p in obj.data.polygons], 'Damage topology differs from intact UV basis'
    obj.data.materials.clear()
    materials = [char_wall,char_roof] if damaged else [wall_material,roof]
    if has_equipment: materials.append(char_hvac if damaged else industrial_hvac if factory else hvac)
    for m in materials: obj.data.materials.append(m)
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name='UVMap')
    for p,index in zip(obj.data.polygons, original_indices):
        p.material_index = section_indices[p.index]
        equipment = p.material_index==2
        axis = max(range(3), key=lambda i:abs(intact_normals[p.index][i]))
        for loop_index in p.loop_indices:
            vi = obj.data.loops[loop_index].vertex_index; coord = intact_coords[vi]
            # Keep the UV basis right-handed with respect to each face normal.
            # Sign on horizontal coordinate prevents the back/left mirror seam.
            sign = 1 if intact_normals[p.index][axis] > 0 else -1
            if axis==0: a,b = sign*coord.y,coord.z
            elif axis==1: a,b = -sign*coord.x,coord.z
            else: a,b = sign*coord.x,coord.y
            # Oversize brick 2x for tabletop-camera readability, while keeping
            # the approved rooftop UV density and mechanical texture detail.
            tile = .5 if equipment else 2.4
            uv.data[loop_index].uv = (a/tile,b/tile)
            if equipment: colors.data[loop_index].color = (component_info[vi],1,1,1)
    assert geometry(obj)==accepted, 'Surface authoring changed accepted geometry'
    assert original_alpha==[c.color[3] for c in colors.data], 'Night window alpha changed'
    obj.data.calc_loop_triangles(); assert len(obj.data.loop_triangles)==triangle_count
    assert set(section_indices)==set(range(len(materials))), 'Unused or missing surface section'
    surface_detail = roof_signs.apply(obj,category,density,damaged,intact_coords,intact_normals)
    assert original_alpha==[c.color[3] for c in colors.data], 'Roof/sign window alpha changed'
    obj.name = 'SM_'+category+density+'_SurfacePrototype'+('_Damaged' if damaged else '')
    path = output/(obj.name+'.fbx')
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',
        bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
    assets.append(dict(asset_id=entry['asset_id']+'.surfaces',prototype_only=True,
        export=path.relative_to(root).as_posix(),source=source.relative_to(root).as_posix(),
        accepted_geometry_sha256=hashlib.sha256(reference_path.read_bytes()).hexdigest(),
        accepted_geometry_export=reference_path.relative_to(root).as_posix(),
        input_geometry_export=entry['export'],input_geometry_sha256=hashlib.sha256((root/entry['export']).read_bytes()).hexdigest(),
        geometry_refinement=refinement,triangles=triangle_count,sections=len(obj.data.materials),
        roof_sign_detail=surface_detail,
        category=category,density=density,damaged=damaged,dimensions_m=list(obj.dimensions),
        mechanical_mask_values=sorted(set(component_info.values())) if has_equipment else [],
        material_slots={m.name:'/Game/CitySim/Art/Prototypes/BuildingSurfaces/Materials/MI_'+('CharredTrim' if m.name=='CharredSign' else m.name) for m in obj.data.materials},
        destination='/Game/CitySim/Art/Prototypes/BuildingSurfaces/Meshes',sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    if not skip_renders and (not focused_renders or refinement):
        render(obj, obj.name+'-Whole', (0,0,19) if tower else (0,0,obj.dimensions.z*.48),
               48 if tower else max(27,obj.dimensions.z*1.24))
    if not skip_renders and not damaged and tower and not focused_renders:
        render(obj, obj.name+'-Wall', (0,-6.5,27), 14)
        render(obj, obj.name+'-Roof', (2.4,1.8,38.4), 12)
    if not skip_renders and not damaged and refinement:
        render(obj,obj.name+'-Detail',(0,-4.5,11.5) if category=='Commercial' else (0,-2,13 if density=='Medium' else 19),
               15 if category=='Commercial' else 24)
    objects.append(obj)

sign_variants=[]
for obj,entry in zip(objects[:],assets):
    if entry['category']!='Commercial': continue
    for sign in roof_signs.metadata['signs']:
        variant=obj.copy(); variant.data=obj.data.copy(); scene.collection.objects.link(variant)
        variant.name='SM_Commercial'+entry['density']+'_Store'+sign['id'].title()+('_Damaged' if entry['damaged'] else '')
        detail=roof_signs.change_sign(variant,entry['roof_sign_detail'],sign['atlas_index'])
        path=output/(variant.name+'.fbx')
        bpy.ops.object.select_all(action='DESELECT'); variant.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
            apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',
            bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
        record=dict(entry,asset_id=entry['asset_id']+'.store.'+sign['id'],
            base_asset_id=entry['asset_id'],business=sign['id'],roof_sign_detail=detail,
            export=path.relative_to(root).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        sign_variants.append(record)
        if not skip_renders and not entry['damaged'] and entry['density']=='Medium':
            render(variant,variant.name+'-Sign',(0,-4.5,11.5),13)
        objects.append(variant)

for i,obj in enumerate(objects):
    obj.location.x=(i//2%3)*32; obj.location.y=(i//6)*42+(20 if i%2 else 0); obj.hide_render=False
# Pack textures so the saved source opens with the complete look.
bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=str(source))
manifest = dict(task='D79',prototype_only=True,geometry_unchanged=False,
    production_imported=False,brick_tile_m=[2.4,2.4],brick_readability_scale=2.0,mechanical_tile_m=[.5,.5],
    vertex_channels='A=existing night-window mask; R=paint(0)/exposed metal(1) only on Mechanical',
    normal_convention='DirectX; Blender shader flips green',assets=assets,
    roof_sign_textures='ArtSource/Textures/RoofsSigns/roofs_signs.json',sign_variants=sign_variants)
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('CITYSIM_BUILDING_SURFACE_PROTOTYPE_PASS', len(assets), 'base;',len(sign_variants),'store variants')
