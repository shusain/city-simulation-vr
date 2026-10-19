"""Original D66 opaque low-poly tree variants and pooled seasonal props.

Blender --background --factory-startup --python this-file renders the contact
sheet and saves source/manifest. Review that sheet before repeating with
-- --export; only this kit's focused seasonal exports are written.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability, set_capability
import bpy, bmesh, json, math, sys
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'ArtSource/Blender/CitySeasonalKit.blend'
MANIFEST = ROOT / 'ArtSource/Manifests/city_seasonal_kit.json'
PREVIEW = ROOT / 'Artifacts/ArtPreview/D66-Seasonal-Blender.png'
COLORS = {
    'Bark': (.22, .12, .055), 'Autumn': (.90, .26, .025),
    'Canopy': (.12, .32, .07), 'Snow': (.86, .92, .97),
    'Foliage': (.17, .42, .09), 'Flowers': (.82, .10, .34),
    'Pumpkin': (.94, .30, .025), 'Detail': (.055, .075, .09),
    'FestiveDetail': (.95, .64, .07), 'Gift': (.65, .025, .035),
    'Ribbon': (.95, .64, .07),
}
MATERIAL_PATHS = {key: '/Game/CitySim/Art/Materials/MI_Seasonal' + key for key in COLORS}
MATERIAL_PATHS['Bark'] = '/Game/CitySim/Art/Materials/MI_EnvironmentBark'
MATERIAL_PATHS['Canopy'] = '/Game/CitySim/Art/Materials/MI_EnvironmentCanopy'
MATERIAL_PATHS['Snow'] = '/Game/CitySim/Art/Materials/MI_EnvironmentSnow'
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
materials = {}
for key, color in COLORS.items():
    mat = bpy.data.materials.new(key)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = .85
    materials[key] = mat


def cone(radius, depth, position, material, top=0, sides=7):
    bpy.ops.mesh.primitive_cone_add(vertices=sides, radius1=radius,
                                  radius2=top, depth=depth, location=position)
    obj = bpy.context.object
    obj.data.materials.append(materials[material])
    return obj


def ico(scale, position, material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=position)
    obj = bpy.context.object
    obj.scale = scale
    obj.data.materials.append(materials[material])
    return obj


def box(size, position, material):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    obj = bpy.context.object
    obj.scale = size
    obj.data.materials.append(materials[material])
    return obj


def branch(start, end, width, tip=.025):
    a, b = Vector(start), Vector(end)
    obj = cone(width, (b-a).length, (a+b)/2, 'Bark', tip, 6)
    obj.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    return obj


def join(name, parts, slots):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    floor = min(vertex.co.z for vertex in obj.data.vertices)
    for vertex in obj.data.vertices:
        vertex.co.z -= floor
    old_slots = [mat.name for mat in obj.data.materials]
    indices = [slots.index(old_slots[poly.material_index]) for poly in obj.data.polygons]
    obj.data.materials.clear()
    for slot in slots:
        obj.data.materials.append(materials[slot])
    for poly, index in zip(obj.data.polygons, indices):
        poly.material_index = index
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    assert not any(edge.is_boundary for edge in bm.edges), name
    assert not any(face.calc_area() < 1e-8 for face in bm.faces), name
    bm.to_mesh(obj.data)
    bm.free()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


def snow_cap(radius, depth, center_z, sector):
    # A closed thin snow wedge follows one upper cone facet. Alternating exposed
    # facets retain unmistakable evergreen foliage; no transparent snow cards.
    angle_a, angle_b = math.pi/2+math.tau*sector/7, math.pi/2+math.tau*(sector+1)/7
    bottom_z = center_z - depth/2
    top_z = center_z + depth/2
    verts = []
    near, far = (.70, .99) if radius > .8 else (.04, .72)
    for lift in (.04, .085):
        for fraction, angle in ((far, angle_a), (far, angle_b), (near, angle_b), (near, angle_a)):
            verts.append((radius*fraction*math.cos(angle), radius*fraction*math.sin(angle),
                          top_z - depth*fraction + lift))
    faces = [(0,3,2,1), (4,5,6,7), (0,1,5,4), (1,2,6,5), (2,3,7,6), (3,0,4,7)]
    mesh = bpy.data.meshes.new('SnowCap')
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(materials['Snow'])
    obj = bpy.data.objects.new('SnowCap', mesh)
    scene.collection.objects.link(obj)
    return obj


autumn_parts = [cone(.22, 3.8, (0,0,1.9), 'Bark', .12)]
for angle in (.3, 2.5, 4.5):
    autumn_parts.append(branch((0,0,2.7), (math.cos(angle)*1.2, math.sin(angle)*1.2,4.2), .10))
autumn_parts += [ico((1.48,1.30,1.48),(-.25,0,4.25),'Autumn'),
                 ico((.95,1.00,1.15),(.75,.25,5.10),'Autumn')]
autumn = join('SM_Scenery_Broadleaf_Autumn_A', autumn_parts, ['Bark','Autumn'])
# Rename the semantic crown slot to Canopy, while retaining autumn material path.
autumn.data.materials[1] = materials['Autumn'].copy()
autumn.data.materials[1].name = 'Canopy'

bare_parts = [cone(.22,4.75,(0,0,2.375),'Bark',.07)]
for i, angle in enumerate((.2,1.55,2.85,4.15,5.4)):
    end = (math.cos(angle)*1.48, math.sin(angle)*1.48, 4.3 + (i%2)*.65)
    start = (0,0,2.5+i*.20)
    bare_parts.append(branch(start, end, .10, .035))
    twig = (end[0]*.87, end[1]*.87, end[2]+1.15)
    joint = tuple(start[j]+.65*(end[j]-start[j]) for j in range(3))
    bare_parts.append(branch(joint, twig, .048, .018))
bare_parts.append(branch((0,0,4.2),(.25,-.20,6.05),.075,.018))
bare = join('SM_Scenery_Broadleaf_Winter_A', bare_parts, ['Bark'])

pine_parts = [cone(.20,4,(0,0,2),'Bark',.10)]
for level, (radius, depth, z) in enumerate(((1.6,3,3.1),(1.2,2.7,4.3),(.75,2.4,5.4))):
    pine_parts.append(cone(radius,depth,(0,0,z),'Canopy'))
    for sector in (0,1,3,5):
        pine_parts.append(snow_cap(radius,depth,z,(sector+level)%7))
pine = join('SM_Scenery_Conifer_Winter_A', pine_parts, ['Bark','Canopy','Snow'])

flower_parts = [box((3.0,1.6,.16),(0,0,.08),'Foliage')]
for x, y in ((-.9,-.35),(0,-.35),(.9,-.35),(-.55,.35),(.55,.35)):
    flower_parts += [ico((.36,.31,.24),(x,y,.31),'Foliage'),
                     ico((.29,.27,.20),(x,y,.61),'Flowers')]
flowers = join('SM_Seasonal_FlowerBed_A',flower_parts,['Foliage','Flowers'])
pumpkin = join('SM_Seasonal_Pumpkin_A',[
    ico((.8,.75,.66),(0,0,.66),'Pumpkin'),
    cone(.13,.36,(.05,0,1.40),'Bark',.07,6)],['Pumpkin','Bark'])
snowman_parts = [ico((.76,.73,.81),(0,0,.81),'Snow'),
                 ico((.56,.54,.64),(0,0,1.87),'Snow'),
                 ico((.42,.41,.46),(0,0,2.66),'Snow'),
                 cone(.52,.08,(0,0,3.00),'Detail',.52),
                 cone(.31,.42,(0,0,3.23),'Detail',.31)]
for x in (-.15,.15):
    snowman_parts.append(ico((.055,.055,.055),(x,-.38,2.74),'Detail'))
for z in (1.63,1.88,2.13):
    snowman_parts.append(ico((.075,.075,.075),(0,-.52,z),'Detail'))
snowman = join('SM_Seasonal_Snowman_A',snowman_parts,['Snow','Detail'])
festive_parts = [cone(.17,.6,(0,0,.30),'Canopy',.15)]
for radius, depth, z in ((1.3,1.8,1.3),(.98,1.65,2.2),(.65,1.45,3.04)):
    festive_parts.append(cone(radius,depth,(0,0,z),'Canopy'))
    count = 4 if radius == 1.3 else 2
    for i in range(count):
        a = math.tau*i/count + z*.35
        festive_parts.append(ico((.16,.16,.16),(radius*.70*math.cos(a),radius*.70*math.sin(a),z-.10),'FestiveDetail'))
# Solid five-point star, symmetric around the ground-centered tree origin.
verts=[]
for y in (-.085,.085):
    verts.append((0,y,4.04))
    for i in range(10):
        angle=math.pi/2+math.tau*i/10
        radius=.42 if i%2==0 else .19
        verts.append((math.cos(angle)*radius,y,4.04+math.sin(angle)*radius))
faces=[]
for i in range(10):
    j=(i+1)%10
    faces.extend(((0,1+i,1+j),(11,12+j,12+i),(1+i,12+i,12+j,1+j)))
mesh=bpy.data.meshes.new('FestiveStar');mesh.from_pydata(verts,[],faces);mesh.materials.append(materials['FestiveDetail'])
star=bpy.data.objects.new('FestiveStar',mesh);scene.collection.objects.link(star);festive_parts.append(star)
festive=join('SM_Seasonal_FestiveTree_A',festive_parts,['Canopy','FestiveDetail'])
festive.data.materials[1]=materials['FestiveDetail'].copy();festive.data.materials[1].name='Detail'
gift=join('SM_Seasonal_Gift_A',[
    box((1.2,1.2,1.0),(0,0,.5),'Gift'),
    box((.18,1.24,1.04),(0,0,.52),'Ribbon'),
    box((1.24,.18,1.04),(0,0,.52),'Ribbon'),
    ico((.32,.15,.14),(-.18,0,1.15),'Ribbon'),
    ico((.32,.15,.14),(.18,0,1.15),'Ribbon')],['Gift','Ribbon'])
assets=[autumn,bare,pine,flowers,pumpkin,snowman,festive,gift]
original_asset_names = {obj.name for obj in assets}
slot_overrides = {}
for season, foliage in (('Autumn','Autumn'),('Winter','Canopy')):
    bpy.ops.mesh.primitive_torus_add(major_segments=12, minor_segments=4,
        major_radius=.52,minor_radius=.08,rotation=(math.pi/2,0,0))
    ring=bpy.context.object;ring.data.materials.append(materials[foliage])
    parts=[ring]
    for angle in (.40,1.70,3.15,4.65):
        x,z=.53*math.cos(angle),.53*math.sin(angle)
        parts.append(ico((.065,.065,.065),(x,-.085,z),'Gift'))
    parts += [box((.25,.10,.18),(-.11,-.085,-.36),'Gift'),
              box((.25,.10,.18),(.11,-.085,-.36),'Gift')]
    wreath=join('SM_Seasonal_Wreath'+season+'_A',parts,[foliage,'Gift'])
    slot_overrides[wreath.name]={'Canopy':MATERIAL_PATHS[foliage],'Accent':MATERIAL_PATHS['Gift']}
    assets.append(wreath)
leaf_parts=[]
for x,y,s in ((-.47,-.12,.58),(.40,.10,.58),(0,.37,.43),(-.15,-.40,.43)):
    leaf_parts.append(ico((s,s*.70,.12),(x,y,.12),'Autumn'))
leaf=join('SM_Seasonal_LeafPile_A',leaf_parts,['Autumn'])
slot_overrides[leaf.name]={'Canopy':MATERIAL_PATHS['Autumn']}
assets.append(leaf)

# Focused display variants reproduce only the accepted low Home and pocket park
# parts; original source/exports/catalog/damaged variants are never overwritten.
sys.path.insert(0, str(ROOT/'Tools'))
import CityArtKit as starter
starter.initialize_materials()
home_name='SM_Building_Residential_A'
home_parts=[starter.box(home_name+'_Body',(14,14,6),(0,0,3),'Residential'),
    starter.gable(home_name+'_Roof'),
    starter.box(home_name+'_Chimney',(1.8,1.8,3),(4.2,3,7.5),'Roof')]
home_parts += starter.entrance_parts(home_name,-7,'Roof')
home_parts += starter.architectural_details(home_name)
for part in home_parts:
    if 'EntrancePath' in part.name:
        # Keep the baked walk inside the full saved lot. Runtime road-following
        # entrance geometry supplies the separate stretch beyond its front edge.
        part.dimensions.y=8.0-6.16;part.location.y=-(8.0+6.16)/2
        part.location.z-=.20  # Body pivot is 20cm above the lawn foundation top.
    else:
        part.scale.x*=.88;part.scale.y*=.88
        part.location.x*=.88;part.location.y*=.88
home=starter.combine('SM_Building_Residential_SeasonalSpace_A',home_parts,add_architectural_details=False)
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
assets.append(home)
slot_overrides[home.name]={'Residential':'/Game/CitySim/Art/Materials/MI_CityResidential',
                         'Roof':'/Game/CitySim/Art/Materials/MI_CityWindowDetail'}
surface_adopted=any(has_capability(e, 'building_surfaces') for e in json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())['assets'])
if surface_adopted:
    import sys
    sys.path.insert(0,str(ROOT/'Tools'))
    from CitySeasonalBuildingSurfaces import apply_home
    slot_overrides[home.name]=apply_home(home)
park_name='SM_Building_Park_December_A'
park_parts=[starter.box(park_name+'_Lawn',(22,22,.35),(0,0,.175),'Park'),
    starter.box(park_name+'_PathNS',(3,22,.12),(0,0,.41),'Trim'),
    starter.box(park_name+'_PathEW',(22,3,.12),(0,0,.41),'Trim'),
    starter.cylinder(park_name+'_CenterPedestal',2.4,.18,(0,0,.56),'Trim')]
for i,(x,y) in enumerate(((-7,-7),(7,-7),(-7,7),(7,7))):
    park_parts += [starter.cylinder(park_name+'_TreeTrunk'+str(i),.45,4,(x,y,2.4),'Trim'),
        starter.cylinder(park_name+'_TreeCrown'+str(i),2.2,3.5,(x,y,5.6),'Park')]
park=starter.combine(park_name,park_parts)
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
assets.append(park)
slot_overrides[park.name]={'Park':'/Game/CitySim/Art/Materials/MI_CityPark',
                         'Trim':'/Game/CitySim/Art/Materials/MI_CityTrim'}
for obj in ((park,) if surface_adopted else (home,park)):
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
neighborhood_name='SM_Building_NeighborhoodPark_December_A'
neighborhood_parts=[starter.box(neighborhood_name+'_Lawn',(40,40,.12),(0,0,.06),'Park'),
    starter.box(neighborhood_name+'_CenterPedestal',(4,4,.18),(0,0,.21),'Trim')]
for axis in (0,1):
    for sign in (-1,1):
        position=[0,0,.21];position[axis]=sign*11
        size=[3,3,.18];size[axis]=18
        neighborhood_parts.append(starter.box(neighborhood_name+'_Path',size,position,'Trim'))
for i,(x,y) in enumerate(((-14,-14),(14,-14),(-14,14),(14,14),(-14,8),(14,8))):
    neighborhood_parts.append(starter.box(neighborhood_name+'_Trunk'+str(i),(.7,.7,3),(x,y,1.62),'Trim'))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=2.8,location=(x,y,5))
    crown=bpy.context.object;crown.name=neighborhood_name+'_Crown'+str(i)
    crown.scale.z=1.15;crown.data.materials.append(starter.MATERIALS['Park']);neighborhood_parts.append(crown)
for x,y in ((-7,-6),(7,6),(-7,6),(7,-6)):
    neighborhood_parts += [starter.box(neighborhood_name+'_Bench',(3.2,1,.5),(x,y,.47),'Trim'),
        starter.box(neighborhood_name+'_BenchBack',(3.2,.2,1),(x,y+.45,.82),'Trim')]
neighborhood=starter.combine(neighborhood_name,neighborhood_parts)
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.context.view_layer.objects.active=neighborhood
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
assets.append(neighborhood)
slot_overrides[neighborhood.name]={'Park':'/Game/CitySim/Art/Materials/MI_CityPark',
    'Trim':'/Game/CitySim/Art/Materials/MI_CityTrim'}

entries=[]
reviewed_doc=json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {'assets':[]}
reviewed_home=next((e for e in reviewed_doc['assets'] if e['name']==home.name and has_capability(e, 'reviewed_building')),None)
if reviewed_home:
    with bpy.data.libraries.load(str(ROOT/reviewed_home['source']),link=False) as (available,loaded):
        assert home.name in available.objects;loaded.objects=[home.name]
    reviewed_object=loaded.objects[0];home.data=reviewed_object.data.copy();bpy.data.objects.remove(reviewed_object,do_unlink=True)
    slot_overrides[home.name]=reviewed_home['material_slots']
    bpy.context.view_layer.update()
for obj in assets:
    obj.data.calc_loop_triangles()
    count=len(obj.data.loop_triangles)
    limit=reviewed_home['triangle_limit'] if obj==home and reviewed_home else 500 if 'Building' in obj.name else 360 if 'Scenery' in obj.name else 260
    assert count<=limit,(obj.name,count)
    slot_map=slot_overrides.get(obj.name)
    if slot_map is None:
        slot_map={slot.name.split('.')[0]:MATERIAL_PATHS[slot.name.split('.')[0]] for slot in obj.data.materials}
    if obj==autumn:slot_map={'Bark':MATERIAL_PATHS['Bark'],'Canopy':MATERIAL_PATHS['Autumn']}
    if obj==festive:slot_map={'Canopy':MATERIAL_PATHS['Canopy'],'Detail':MATERIAL_PATHS['FestiveDetail']}
    for slot, name in zip(obj.data.materials,slot_map):slot.name=name
    entries.append(dict(name=obj.name,asset_id='seasonal.'+obj.name.removeprefix('SM_').lower(),
        source='ArtSource/Blender/CitySeasonalKit.blend',export='ArtSource/Exports/'+obj.name+'.fbx',
        mesh_destination='/Game/CitySim/Art/Meshes/'+obj.name,
        dimensions_m=[round(float(v),5) for v in obj.dimensions],
        bounds_min_m=[round(min(v.co[i] for v in obj.data.vertices),5) for i in range(3)],
        bounds_max_m=[round(max(v.co[i] for v in obj.data.vertices),5) for i in range(3)],
        triangles=count,triangle_limit=limit,material_slots=slot_map,
        collision_policy='none; decorative pooled instances only',pivot_policy='ground center',
        lod_policy='single low-poly LOD; existing bounded scenery/decoration pools',
        uv_policy='smart-projected UV0; shared solid opaque colors; no alpha texture',
        provenance='Original project Blender primitives; Tools/Create-CitySeasonalArt.py'))
    if obj==home and surface_adopted:
        entries[-1].update(building_surface_task='D79',uv_policy='physical brick2.4m and shingles4.8m; preserved window alpha')
        set_capability(entries[-1], 'building_surfaces')
    if obj.name in ('SM_Seasonal_WreathAutumn_A','SM_Seasonal_WreathWinter_A'):
        entries[-1].update(attachment_policy='authored XZ plane, front -Y; ring center Z0.60m; base pivot',
            ring_center_m=[0,0,.60],door_attachment_authored_m=[0,-6.48,2.15])
    if obj==home:
        entries[-1].update(footprint_cells=[2,2],display_variant_of='SM_Building_Residential_A',
            pivot_policy='body ground center; entrance walk extends20cm below pivot to sit on lawn',
            body_xy_scale=.88,entrance_endpoint_authored_m=[0,-8.0,-.04],
            door_front_authored_m=[0,-6.424,2],wreath_attachment_authored_m=[0,-6.48,2.15],
            body_base_authored_z_m=0,driveway_bottom_authored_z_m=-.20,
            driveway_top_authored_z_m=-.04,
            presentation_yaw_offset_quarter_turns=2)
    if obj==home and reviewed_home:
        entries[-1]=reviewed_home
    if obj==park:
        entries[-1].update(footprint_cells=[3,3],display_variant_of='SM_Building_Park_A',
            center_pedestal_top_m=.65,center_decoration_support_m=.67,
            presentation_yaw_offset_quarter_turns=2)
    if obj==neighborhood:
        entries[-1].update(footprint_cells=[5,5],display_variant_of='SM_Building_NeighborhoodPark_A',
            center_pedestal_top_m=.30,center_decoration_support_m=.32,
            presentation_yaw_offset_quarter_turns=2)
MANIFEST.parent.mkdir(parents=True,exist_ok=True)
MANIFEST.write_text(json.dumps(dict(schema_version=1,task='D66',
    material_parent='/Game/CitySim/Art/Materials/M_CityColor',
    material_colors={**{MATERIAL_PATHS[key]:list(value) for key,value in COLORS.items()},
        '/Game/CitySim/Art/Materials/MI_CityResidential':[.24,.62,.34],
        '/Game/CitySim/Art/Materials/MI_CityWindowDetail':[.18,.23,.28],
        '/Game/CitySim/Art/Materials/MI_CityPark':[.18,.62,.22],
        '/Game/CitySim/Art/Materials/MI_CityTrim':[.82,.73,.48]},
    assets=entries),indent=2)+'\n',encoding='utf-8')
if '--export' in sys.argv:
    assert (ROOT/'Artifacts/ArtPreview/D66-Refinement-Blender.png' if '--followup-only' in sys.argv else PREVIEW).exists(),'Render/review contact sheet before exporting'
    for obj, entry in zip(assets, entries):
        if obj==home and reviewed_home:
            continue # The reviewed D91 source/export remains authoritative.
        if '--residential-ground-only' in sys.argv and obj!=home:
            continue
        if '--followup-only' in sys.argv and obj.name in original_asset_names:
            continue
        # Blender globally uniquifies names. Free semantic names before each
        # selected export so autumn/green canopy and black/gold Detail all retain
        # the exact per-asset contract, without an imported .001 slot suffix.
        for index, material in enumerate(bpy.data.materials):
            material.name = '_SeasonalExport_' + str(index)
        for material, slot_name in zip(obj.data.materials, entry['material_slots']):
            material.name = slot_name
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
        bpy.context.view_layer.objects.active=obj
        bpy.ops.export_scene.fbx(filepath=str(ROOT/'ArtSource/Exports'/f'{obj.name}.fbx'),
            use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',
            apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
else:
    SOURCE.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
    PREVIEW.parent.mkdir(parents=True,exist_ok=True)
    followup='--followup-only' in sys.argv
    residential_ground='--residential-ground-only' in sys.argv
    shown=[home] if residential_ground else assets[8:] if followup else assets[:8]
    for obj in assets:
        obj.hide_render=obj not in shown
    positions=[(-7,3,0),(0,3,0),(7,3,0),(-7,-4,0),(-3.5,-4,0),(0,-4,0),(4,-4,0),(7.5,-4,0)]
    labels=['Autumn broadleaf','Winter bare broadleaf','Snow evergreen','Spring flowers','Autumn pumpkin','Winter snowman','Festive evergreen','Gift']
    if followup:
        PREVIEW=ROOT/'Artifacts/ArtPreview/D66-Refinement-Blender.png'
        positions=[(-20,-20,0),(-14,-20,0),(-6,-20,0),(-24,8,0),(-3,8,0),(29,8,0)]
        labels=['Autumn wreath (4x)','Winter wreath (4x)','Leaf pile (3x)','Home front space','December pocket center','December neighborhood center']
    if residential_ground:
        PREVIEW=ROOT/'Artifacts/ArtPreview/D66-ResidentialGround-Home-Blender.png'
        positions=[(0,0,0)];labels=['Home walk ends at saved lot edge']
        # Preview-only lawn foundation uses the same untouched shared colors.
        pad=box((16,16,.40),(0,0,-.40),'Foliage')
        pad.name='PreviewLotFoundation'
        for slot,color in (('PreviewLawn',(.16,.58,.20)),('PreviewConcrete',(.88,.88,.86))):
            value=bpy.data.materials.new(slot);value.use_nodes=True
            value.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*color,1)
            pad.data.materials.append(value)
        for polygon in pad.data.polygons:polygon.material_index=1 if polygon.normal.z>.9 else 2
    for obj,position,label in zip(shown,positions,labels):
        obj.location=position
        if followup and not residential_ground and 'Wreath' in obj.name:obj.scale*=4
        if followup and not residential_ground and 'LeafPile' in obj.name:obj.scale*=3
        bpy.ops.object.text_add(location=(position[0],position[1]-2.2,.025))
        text=bpy.context.object;text.data.body=label;text.data.align_x='CENTER';text.data.size=.60 if followup else .33
        text.data.materials.append(materials['Detail'])
    bpy.ops.mesh.primitive_plane_add(size=130 if followup else 40,location=(0,0,-.04))
    ground=bpy.context.object
    if residential_ground:ground.location.z=-.625
    ground_material=bpy.data.materials.new('PreviewGround');ground_material.diffuse_color=(.22,.24,.25,1);ground_material.use_nodes=True
    ground_material.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.22,.24,.25,1)
    ground.data.materials.append(ground_material)
    scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.world.color=(.18,.18,.18);scene.view_settings.view_transform='Standard'
    bpy.ops.object.light_add(type='AREA',location=(0,-15,45) if followup else (0,-8,20));bpy.context.object.data.energy=45000 if followup else 6500;bpy.context.object.data.size=60 if followup else 18
    bpy.ops.object.light_add(type='AREA',location=(35,10,45) if followup else (-12,8,15));bpy.context.object.data.energy=30000 if followup else 2200;bpy.context.object.data.size=60 if followup else 14
    bpy.ops.object.camera_add(location=(35,-60,50) if followup else (12,-22,19));camera=bpy.context.object
    camera.rotation_euler=(Vector((0,3,2.1) if followup else (0,0,2.1))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=120 if followup else 25;scene.camera=camera
    if residential_ground:
        camera.location=(17,-26,17)
        camera.rotation_euler=(Vector((0,0,3))-camera.location).to_track_quat('-Z','Y').to_euler()
        camera.data.ortho_scale=28
    scene.render.filepath=str(PREVIEW);bpy.ops.render.render(write_still=True)
    if followup and not residential_ground:
        for obj in scene.objects:
            if obj.type in ('MESH','FONT'):obj.hide_render=True
        for obj,x in ((assets[8],-.85),(assets[9],.85)):
            obj.hide_render=False;obj.scale=(1,1,1);obj.location=(x,0,0)
        ground.hide_render=False
        bpy.ops.object.light_add(type='AREA',location=(0,-3,5));bpy.context.object.data.energy=500;bpy.context.object.data.size=4
        camera.location=(2.3,-4.5,3)
        camera.rotation_euler=(Vector((0,0,.6))-camera.location).to_track_quat('-Z','Y').to_euler()
        camera.data.ortho_scale=3.8
        scene.render.resolution_x=1200;scene.render.resolution_y=700
        scene.render.filepath=str(ROOT/'Artifacts/ArtPreview/D66-Refinement-Wreaths-Blender.png')
        bpy.ops.render.render(write_still=True)
print('CITYSIM_SEASONAL_ART_READY',json.dumps(entries))
