"""Shared Blender starter primitives, facade details and mesh recipes.

Call initialize_materials() once per authoring run. Importing this module does
not reset the scene, create materials, save sources, export FBXs or edit manifests.
Geometry, WindowMask semantics and the existing Unreal FBX settings stay here;
review/promotion policy belongs to each generator.
"""
import bpy
import os
import math
from mathutils import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BLEND_PATH = os.path.join(ROOT, "ArtSource", "Blender", "CityStarterKit.blend")
EXPORT_DIR = os.path.join(ROOT, "ArtSource", "Exports")


def material(name, color):
    value = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1.0)
    value.use_nodes = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Roughness"].default_value = .85
    return value


MATERIALS = {}


def initialize_materials():
    """Start one authoring run with the shared palette; import alone creates no data."""
    palette = {
        "Residential": material("Residential", (0.24, 0.62, 0.34)),
        "Commercial": material("Commercial", (0.14, 0.44, 0.82)),
        "Industrial": material("Industrial", (0.86, 0.64, 0.10)),
        "Power": material("Power", (0.76, 0.24, 0.08)),
        "Service": material("Service", (0.08, 0.56, 0.60)),
        "Park": material("Park", (0.18, 0.62, 0.22)),
        "Roof": material("Roof", (0.18, 0.23, 0.28)),
        "Road": material("Road", (0.09, 0.10, 0.12)),
        "RoadMarking": material("RoadMarking", (0.94, 0.78, 0.24)),
        "Trim": material("Trim", (0.82, 0.73, 0.48)),
        "Traffic": material("Traffic", (0.88, 0.88, 0.86)),
    }
    MATERIALS.clear()
    MATERIALS.update(palette)
    return MATERIALS



def box(name, dimensions, location, slot):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.data.materials.append(MATERIALS[slot])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return obj


def beam(name, start, end, width, slot):
    a, b = Vector(start), Vector(end)
    obj = box(name, (width, width, (b-a).length), (a+b)*0.5, slot)
    obj.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    return obj


def cylinder(name, radius, depth, location, slot):
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(MATERIALS[slot])
    return obj


def wheel(name, radius, width, location):
    obj = cylinder(name, radius, width, location, "Roof")
    # Blender cylinders point along Z by default; vehicle axles run across Y.
    obj.rotation_euler[0] = 1.57079632679
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    return obj


def entrance_parts(name, front_y, slot):
    # The authored front is -Y. The path reaches the edge of the neighboring
    # eight-meter road cell, so quarter-turn placement has a readable portal.
    road_edge_y = -10.4
    path_length = front_y - road_edge_y
    return [
        box(f"{name}_EntrancePath", (3.0, path_length, 0.16),
            (0, (front_y + road_edge_y) * 0.5, 0.08), slot),
        box(f"{name}_EntranceDoor", (3.0, 0.3, 4.0),
            (0, front_y - 0.15, 2.0), slot),
    ]


def gable(name):
    # Solid pitched roof inside the previous roof envelope.
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(-8,-8,6), (8,-8,6), (8,8,6), (-8,8,6),
                      (0,-8,8), (0,8,8)], [],
                     [(0,3,2,1), (0,1,4), (3,5,2), (0,4,5,3), (4,1,2,5)])
    mesh.materials.append(MATERIALS["Roof"])
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def facade(name, half_x, half_y, center_y, rows, slot, columns=(-4.2, 0, 4.2), height=1.8):
    # Two triangles per window, no transparency and no extra material sections.
    vertices, faces = [], []
    for side in range(4):
        depth = half_y if side % 2 == 0 else half_x
        for z in rows:
            for x in columns:
                if side == 0 and z < 4.5 and abs(x) < 1.6:
                    continue  # The center-front door stays unobstructed.
                start = len(vertices)
                angle = side * math.pi / 2
                for px, py, pz in ((x-1.1,-depth-0.035,z-height/2),
                                   (x+1.1,-depth-0.035,z-height/2),
                                   (x+1.1,-depth-0.035,z+height/2),
                                   (x-1.1,-depth-0.035,z+height/2)):
                    vertices.append((px*math.cos(angle)-py*math.sin(angle),
                                     px*math.sin(angle)+py*math.cos(angle)+center_y, pz))
                faces.append(tuple(range(start, start+4)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(MATERIALS[slot])
    colors = mesh.color_attributes.new(name="WindowMask", type='BYTE_COLOR', domain='CORNER')
    for color in colors.data:
        color.color = (1.0, 1.0, 1.0, 0.0)  # Inverse alpha identifies window panes only.
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def architectural_details(name):
    if "Construction" in name:
        return []
    level = 2 if "High" in name else 1 if "Medium" in name else 0
    parts = []
    if "Building_Residential" in name:
        if level == 0:
            parts += [facade(name+"_Windows", 7, 7, 0, (3.4,), "Roof"),
                      box(name+"_Porch", (4.6, 1.8, 0.4), (0,-7.7,4.7), "Roof")]
        elif level == 1:
            parts += [facade(name+"_Windows", 7, 7, 0, (3.2,7.2,11.2), "Roof"),
                      facade(name+"_UpperWindows", 5.5,5.5,0.5,(16.5,19.5),"Roof",(-3.4,0,3.4))]
        else:
            parts += [facade(name+"_Windows",6.5,6.5,0,range(4,33,4),"Roof")]
        if level:
            # High windows are centered on these floors. Keep the slab bottom
            # above the 0.9m window half-height, as on the medium facade.
            for z in ((8.0,12.0) if level == 1 else (9.2,17.2,25.2,33.2)):
                for x in (-4.2,4.2):
                    parts.append(box(name+f"_Balcony_{x}_{z}",(3.0,1.2,0.35),(x,-7.2,z),"Roof"))
    elif "Building_Shop" in name:
        parts += [facade(name+"_Storefront",7.5,7,0,(2.7,),"Trim",(-4.6,0,4.6),3.0)]
        if level:
            parts += [facade(name+"_OfficeWindows",5.5,5,1,range(13,22 if level==1 else 37,4),"Trim",(-3.5,0,3.5),2.5)]
        else:
            parts += [box(name+"_ParapetFront",(15.4,0.6,0.6),(0,-6.85,8.3),"Trim"),
                      box(name+"_ParapetBack",(15.4,0.6,0.6),(0,6.85,8.3),"Trim"),
                      box(name+"_Vent",(3,3,1.2),(3,2,8.6),"Trim")]
    elif "Building_Factory" in name:
        roof = (8,13,18)[level]
        parts += [facade(name+"_Clerestory",7.5,7,0,(roof-2.2,),"Trim",(-4.6,0,4.6),1.2)]
        for x in (-4.8,0,4.8):
            parts.append(box(name+f"_RoofVent_{x}",(2.3,6.0,0.8),(x,-1.0,roof+0.4),"Trim"))
        for x in (-4.8,4.8):
            parts.append(box(name+f"_DockGuard_{x}",(0.5,0.6,2.0),(x,-7.4,1.0),"Trim"))
    return parts


def combine(name, objects, *, add_architectural_details=True):
    if add_architectural_details:
        objects += architectural_details(name)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        if not obj.data.color_attributes.get("WindowMask"):
            colors = obj.data.color_attributes.new(name="WindowMask", type='BYTE_COLOR', domain='CORNER')
            for color in colors.data: color.color = (1.0, 1.0, 1.0, 1.0)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    if len(objects) > 1:
        bpy.ops.object.join()
    result = bpy.context.object
    result.name = name
    result.data.name = name
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    for polygon in result.data.polygons:
        polygon.use_smooth = False
    return result


def export_one(obj, export_dir=EXPORT_DIR):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(export_dir, f"{obj.name}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        mesh_smooth_type="FACE",
        axis_forward="-Y",
        axis_up="Z",
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="AUTO",
    )
    print(f"CITYSIM_FBX_EXPORTED {path}")


def residential(name):
    return combine(name, [
        box(f"{name}_Body", (14.0, 14.0, 6.0), (0, 0, 3.0), "Residential"),
        gable(f"{name}_Roof"),
        box(f"{name}_Chimney", (1.8, 1.8, 3.0), (4.2, 3.0, 7.5), "Roof"),
    ] + entrance_parts(name, -7.0, "Roof"))


def residential_medium(name):
    return combine(name, [
        box(f"{name}_Body", (14.0, 14.0, 14.0), (0, 0, 7.0), "Residential"),
        box(f"{name}_Upper", (11.0, 11.0, 7.0), (0, 0.5, 17.5), "Residential"),
        box(f"{name}_Roof", (13.0, 13.0, 1.0), (0, 0.5, 21.5), "Roof"),
        box(f"{name}_BalconyA", (12.0, 1.0, 0.5), (0, -7.0, 9.0), "Roof"),
        box(f"{name}_BalconyB", (12.0, 1.0, 0.5), (0, -7.0, 13.0), "Roof"),
    ] + entrance_parts(name, -7.0, "Roof"))


def residential_high(name):
    parts = [
        box(f"{name}_Tower", (13.0, 13.0, 34.0), (0, 0, 17.0), "Residential"),
        box(f"{name}_Roof", (14.0, 14.0, 2.0), (0, 0, 35.0), "Roof"),
        # Offset rooftop mechanical housing, still inside the existing roof
        # envelope and 41m height. The two existing slots cover all equipment.
        box(f"{name}_EquipmentPad", (5.8, 4.8, .5), (2.4, 1.8, 36.25), "Roof"),
        box(f"{name}_Crown", (5.4, 4.4, 4.0), (2.4, 1.8, 38.5), "Roof"),
        box(f"{name}_IntakePanel", (3.8, .08, 2.3), (2.4, -.455, 38.45), "Residential"),
        box(f"{name}_ServicePanel", (.08, 1.5, 2.3), (5.145, 2.7, 38.2), "Residential"),
        box(f"{name}_ServiceInset", (.10, 1.25, 2.05), (5.20, 2.7, 38.2), "Roof"),
        box(f"{name}_Conduit", (.3, .3, 3.0), (-.48, 2.5, 38.0), "Residential"),
        box(f"{name}_CableTray", (5.0, .45, .25), (-2.25, 2.5, 36.2), "Roof"),
        # Wide bands sit between window rows, clear of the small shade slabs.
        box(f"{name}_SetbackA", (14.0, 1.0, 0.5), (0, -7.0, 14.0), "Roof"),
        box(f"{name}_SetbackB", (14.0, 1.0, 0.5), (0, -7.0, 30.0), "Roof"),
    ] + entrance_parts(name, -6.5, "Roof")
    for z in (37.6, 38.0, 38.4, 38.8, 39.2):
        parts.append(box(f"{name}_IntakeLouver{z}", (3.5, .18, .16), (2.4, -.53, z), "Roof"))
    for x in (1.0, 3.8):
        parts += [cylinder(f"{name}_FanCollar{x}", .9, .3, (x, 1.8, 40.65), "Roof"),
                  cylinder(f"{name}_FanWell{x}", .73, .06, (x, 1.8, 40.83), "Residential"),
                  cylinder(f"{name}_FanHub{x}", .20, .14, (x, 1.8, 40.91), "Roof")]
        for angle in (-math.pi/4, math.pi/4):
            grille = box(f"{name}_FanGrille{x}_{angle}", (1.5, .12, .08), (x, 1.8, 40.96), "Roof")
            grille.rotation_euler.z = angle
            bpy.context.view_layer.objects.active = grille
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
            parts.append(grille)
    return combine(name, parts)


def residential_construction(name):
    parts = [
        box(f"{name}_Foundation", (15.0, 15.0, 0.8), (0, 0, 0.4), "Roof"),
        box(f"{name}_FloorA", (14.0, 14.0, 0.5), (0, 0, 4.0), "Trim"),
        box(f"{name}_FloorB", (14.0, 14.0, 0.5), (0, 0, 8.0), "Trim"),
        box(f"{name}_FloorC", (14.0, 14.0, 0.5), (0, 0, 12.0), "Trim"),
        box(f"{name}_CraneMast", (1.0, 1.0, 20.0), (0, 0, 10.0), "Trim"),
    ]
    for x in (-5.0, 5.0):
        for y in (-5.0, 5.0):
            parts.append(box(f"{name}_Column_{x}_{y}", (0.8, 0.8, 12.0), (x, y, 6.0), "Trim"))
    # Keep the central front gate and its existing entrance path unobstructed.
    for x in (-7.0, 7.0):
        parts.append(box(f"{name}_SideBarrier{x}", (.24, 13.6, 1.0), (x, 0, 1.3), "Trim"))
    parts.append(box(f"{name}_BackBarrier", (14.2, .24, 1.0), (0, 7.0, 1.3), "Trim"))
    for x in (-4.9, 4.9):
        parts.append(box(f"{name}_FrontBarrier{x}", (4.4, .24, 1.0), (x, -7.0, 1.3), "Trim"))
    for index, x in enumerate((-3.2, 3.2)):
        parts.append(box(f"{name}_Pallet{index}", (2.6, 1.6, .22), (x, 5.8, .91), "Trim"))
        for row in range(3):
            parts.append(box(f"{name}_Bundle{index}_{row}", (2.3, .4, .5), (x, 5.25+row*.5, 1.27), "Roof"))
    for x in (-5.0, 5.0):
        parts.append(beam(f"{name}_SideBrace{x}", (x,-4.6,4.4), (x,4.6,7.6), .22, "Roof"))
    # An unfinished site has an open gate, not the finished-building door panel.
    parts.append(box(f"{name}_EntrancePath", (3.0, 2.9, .16), (0, -8.95, .08), "Trim"))
    return combine(name, parts)


def crane_jib(name):
    # Preserve the mast-top hinge and swept bounds used by pooled animation.
    parts = [
        box(f"{name}_Boom", (14.0, 0.6, 0.6), (2.0, 0, 0), "Trim"),
        box(f"{name}_Counterweight", (2.0, 1.8, 1.8), (-4.5, 0, -0.2), "Roof"),
    ]
    for side in (-.55, .55):
        parts.append(box(f"{name}_TopRail{side}", (13.2,.12,.12), (2.0,side,.56), "Trim"))
        for index in range(5):
            x = -4.4 + index*2.55
            parts.append(beam(f"{name}_Lattice{side}_{index}", (x,side,-.18), (x+2.4,side,.50), .10, "Roof"))
    return combine(name, parts)


def crane_hook(name):
    # Keep origin at cable attachment; the U-shaped hook hangs below the block.
    return combine(name, [
        box(f"{name}_Block", (1.0, .8, .65), (0, 0, -.325), "Trim"),
        box(f"{name}_HookLeft", (.16,.24,.7), (-.26,0,-.95), "Roof"),
        box(f"{name}_HookRight", (.16,.24,.7), (.26,0,-.95), "Roof"),
        box(f"{name}_HookBase", (.7,.24,.16), (0,0,-1.30), "Roof"),
    ])


def shop(name):
    return combine(name, [
        box(f"{name}_Body", (15.0, 14.0, 8.0), (0, 0, 4.0), "Commercial"),
        box(f"{name}_Awning", (15.4, 2.2, 1.0), (0, -7.1, 5.0), "Trim"),
        box(f"{name}_Sign", (5.0, 1.0, 4.0), (0, -6.5, 10.0), "Trim"),
    ] + entrance_parts(name, -7.0, "Trim"))


def shop_medium(name):
    return combine(name, [
        box(f"{name}_Podium", (15.0, 14.0, 10.0), (0, 0, 5.0), "Commercial"),
        box(f"{name}_Upper", (11.0, 10.0, 12.0), (0, 1.0, 16.0), "Commercial"),
        box(f"{name}_Roof", (12.0, 11.0, 1.0), (0, 1.0, 22.5), "Trim"),
        box(f"{name}_Awning", (15.4, 2.2, 1.0), (0, -7.1, 5.5), "Trim"),
        box(f"{name}_Sign", (7.0, 1.0, 3.0), (0, -6.5, 10.5), "Trim"),
    ] + entrance_parts(name, -7.0, "Trim"))


def shop_high(name):
    return combine(name, [
        box(f"{name}_Podium", (15.0, 14.0, 9.0), (0, 0, 4.5), "Commercial"),
        box(f"{name}_Tower", (11.0, 10.0, 28.0), (0, 1.0, 23.0), "Commercial"),
        box(f"{name}_Crown", (12.0, 11.0, 2.0), (0, 1.0, 38.0), "Trim"),
        box(f"{name}_Awning", (15.4, 2.2, 1.0), (0, -7.1, 5.5), "Trim"),
        box(f"{name}_Sign", (8.0, 1.0, 4.0), (0, -6.5, 11.0), "Trim"),
    ] + entrance_parts(name, -7.0, "Trim"))


def factory(name):
    return combine(name, [
        box(f"{name}_Hall", (15.0, 14.0, 7.0), (0, 0, 3.5), "Industrial"),
        box(f"{name}_Roof", (16.0, 15.0, 1.0), (0, 0, 7.5), "Industrial"),
        box(f"{name}_LoadingBay", (6.0, 1.0, 5.0), (0, -7.0, 2.5), "Trim"),
        cylinder(f"{name}_StackA", 1.0, 10.0, (-4.5, 3.0, 12.0), "Industrial"),
        cylinder(f"{name}_StackB", 1.0, 8.0, (4.5, 3.0, 11.0), "Industrial"),
    ] + entrance_parts(name, -7.0, "Trim"))


def factory_medium(name):
    return combine(name, [
        box(f"{name}_Hall", (15.0, 14.0, 12.0), (0, 0, 6.0), "Industrial"),
        box(f"{name}_Roof", (16.0, 15.0, 1.0), (0, 0, 12.5), "Industrial"),
        box(f"{name}_LoadingBay", (7.0, 1.0, 6.0), (0, -7.0, 3.0), "Trim"),
        box(f"{name}_Tank", (5.0, 5.0, 8.0), (4.0, 2.5, 16.0), "Industrial"),
        cylinder(f"{name}_StackA", 1.1, 16.0, (-4.5, 3.0, 20.0), "Industrial"),
        cylinder(f"{name}_StackB", 1.1, 13.0, (0.0, 3.0, 18.5), "Industrial"),
    ] + entrance_parts(name, -7.0, "Trim"))


def factory_high(name):
    return combine(name, [
        box(f"{name}_Hall", (15.0, 14.0, 17.0), (0, 0, 8.5), "Industrial"),
        box(f"{name}_Roof", (16.0, 15.0, 1.0), (0, 0, 17.5), "Industrial"),
        box(f"{name}_LoadingBay", (8.0, 1.0, 7.0), (0, -7.0, 3.5), "Trim"),
        box(f"{name}_Processor", (7.0, 7.0, 14.0), (3.5, 2.0, 25.0), "Industrial"),
        cylinder(f"{name}_StackA", 1.2, 24.0, (-5.0, 3.0, 29.0), "Industrial"),
        cylinder(f"{name}_StackB", 1.2, 20.0, (-1.5, 3.0, 27.0), "Industrial"),
        cylinder(f"{name}_StackC", 1.2, 17.0, (2.0, 3.0, 25.5), "Industrial"),
    ] + entrance_parts(name, -7.0, "Trim"))


def power_station(name):
    parts = [
        box(f"{name}_Base", (16.0, 16.0, 5.0), (0, 0, 2.5), "Power"),
        box(f"{name}_GeneratorA", (5.0, 8.0, 4.0), (-4.0, -2.0, 7.0), "Power"),
        box(f"{name}_GeneratorB", (5.0, 8.0, 4.0), (4.0, -2.0, 7.0), "Power"),
        box(f"{name}_StackA", (2.4, 2.4, 11.0), (-4.0, 3.0, 10.5), "Trim"),
        box(f"{name}_StackB", (2.4, 2.4, 11.0), (4.0, 3.0, 10.5), "Trim"),
    ] + entrance_parts(name, -8.0, "Trim")
    for x in (-4.0, 4.0):
        parts.append(box(f"{name}_StackBand{x}", (2.7, 2.7, .5), (x, 3, 14.5), "Power"))
        parts.append(box(f"{name}_Vent{x}", (3.8, 5.5, .3), (x, -2, 9.15), "Trim"))
        for y in (-4, -2, 0):
            parts.append(box(f"{name}_VentRib{x}_{y}", (3.5, .3, .25), (x, y, 9.425), "Power"))
    parts.extend([
        box(f"{name}_Transformer", (4, 2, 2), (0, 5.8, 6), "Trim"),
        box(f"{name}_Busbar", (9, .4, .4), (0, 5.8, 7.4), "Power"),
    ])
    for x in (-4.0, 4.0):
        # Broad louvers and collars read at tabletop scale without new slots.
        parts.append(box(f"{name}_LouverPanel{x}", (3.8, .12, 2.4), (x, -6.06, 6.8), "Trim"))
        for z in (6.0, 6.8, 7.6):
            parts.append(box(f"{name}_Louver{x}_{z}", (3.5, .25, .18), (x, -6.2, z), "Power"))
        parts.append(box(f"{name}_StackCollar{x}", (2.7, 2.7, .35), (x, 3, 10.0), "Power"))
        parts.append(box(f"{name}_StackMouth{x}", (1.75, 1.75, .04), (x, 3, 16.02), "Power"))
    for x in (-1.3, 0, 1.3):
        parts.append(cylinder(f"{name}_Insulator{x}", .23, .6, (x, 5.8, 7.3), "Trim"))
    parts.extend([
        box(f"{name}_Switchgear", (2.0, 1.2, 2.0), (6.1, 6.0, 6.0), "Trim"),
        box(f"{name}_SwitchgearInset", (1.5, .10, 1.35), (6.1, 5.35, 6.0), "Power"),
        box(f"{name}_CableTray", (6.0, .35, .25), (3.0, 5.8, 5.2), "Power"),
    ])
    return combine(name, parts)


def water_treatment(name):
    parts = [
        box(f"{name}_Plant", (16.0, 14.0, 4.0), (0, 0, 2.0), "Service"),
        cylinder(f"{name}_ClarifierA", 3.0, 2.0, (-4.0, 2.5, 5.0), "Service"),
        cylinder(f"{name}_ClarifierB", 3.0, 2.0, (4.0, 2.5, 5.0), "Service"),
        cylinder(f"{name}_Tank", 2.2, 8.0, (0, -1.5, 8.0), "Trim"),
        box(f"{name}_Cap", (5.2, 5.2, 1.0), (0, -1.5, 12.5), "Trim"),
    ] + entrance_parts(name, -7.0, "Trim")
    for x in (-4.0, 4.0):
        # Raised disk/rim and bridge remain inside the existing tank footprint.
        parts.append(cylinder(f"{name}_BasinRim{x}", 3.0, .3, (x, 2.5, 6.15), "Trim"))
        parts.append(cylinder(f"{name}_BasinSurface{x}", 2.6, .08, (x, 2.5, 6.34), "Service"))
        parts.append(box(f"{name}_Bridge{x}", (5.8, .45, .25), (x, 2.5, 6.55), "Trim"))
    parts.extend([
        box(f"{name}_ControlCabinet", (2.5, 2, 2.5), (-5.5, -3.5, 5.25), "Trim"),
        box(f"{name}_FeedPipe", (10, .5, .5), (0, .2, 4.4), "Trim"),
    ])
    for x in (-4.0, 4.0):
        parts.append(cylinder(f"{name}_BasinHub{x}", .42, .45, (x, 2.5, 6.85), "Service"))
        # Maintenance bridge guardrails, broad enough to survive ordinary zoom.
        for y in (2.14, 2.86):
            parts.append(box(f"{name}_BridgeRail{x}_{y}", (5.6, .12, .12), (x, y, 7.35), "Trim"))
            for offset in (-2.5, 0, 2.5):
                parts.append(box(f"{name}_RailPost{x}_{y}_{offset}", (.12, .12, .7), (x+offset, y, 6.95), "Trim"))
        parts.append(box(f"{name}_PipeBranch{x}", (.45, 1.25, .45), (x, .8, 4.4), "Trim"))
    parts.extend([
        box(f"{name}_ControlFace", (1.9, .12, 1.6), (-5.5, -4.56, 5.35), "Service"),
        box(f"{name}_PumpSkid", (3.1, 2.0, .25), (4.8, -3.5, 4.15), "Trim"),
        cylinder(f"{name}_PumpMotor", .6, 1.3, (4.8, -3.5, 4.9), "Service"),
        box(f"{name}_PumpOutlet", (.5, 3.2, .5), (4.8, -1.6, 4.4), "Trim"),
    ])
    return combine(name, parts)


def pocket_park(name):
    parts = [
        box(f"{name}_Lawn", (22.0, 22.0, 0.35), (0, 0, 0.175), "Park"),
        box(f"{name}_PathNS", (3.0, 22.0, 0.12), (0, 0, 0.41), "Trim"),
        box(f"{name}_PathEW", (22.0, 3.0, 0.12), (0, 0, 0.41), "Trim"),
        cylinder(f"{name}_Fountain", 2.4, 0.8, (0, 0, 0.85), "Trim"),
    ]
    for index, (x, y) in enumerate(((-7, -7), (7, -7), (-7, 7), (7, 7))):
        parts.append(cylinder(f"{name}_TreeTrunk{index}", 0.45, 4.0, (x, y, 2.4), "Trim"))
        parts.append(cylinder(f"{name}_TreeCrown{index}", 2.2, 3.5, (x, y, 5.6), "Park"))
    return combine(name, parts)


def traffic_car(name):
    return combine(name, [
        box(f"{name}_Body", (3.4, 1.55, 0.75), (0, 0, 0.55), "Traffic"),
        box(f"{name}_Cabin", (1.65, 1.35, 0.65), (-0.25, 0, 1.22), "Traffic"),
        wheel(f"{name}_WheelFL", 0.32, 0.22, (1.05, -0.82, 0.34)),
        wheel(f"{name}_WheelFR", 0.32, 0.22, (1.05, 0.82, 0.34)),
        wheel(f"{name}_WheelRL", 0.32, 0.22, (-1.05, -0.82, 0.34)),
        wheel(f"{name}_WheelRR", 0.32, 0.22, (-1.05, 0.82, 0.34)),
    ])


def traffic_truck(name):
    return combine(name, [
        box(f"{name}_Cargo", (4.4, 1.7, 1.35), (-0.55, 0, 1.05), "Traffic"),
        box(f"{name}_Cab", (1.35, 1.7, 1.25), (2.25, 0, 0.98), "Traffic"),
        wheel(f"{name}_WheelFL", 0.38, 0.25, (2.1, -0.9, 0.38)),
        wheel(f"{name}_WheelFR", 0.38, 0.25, (2.1, 0.9, 0.38)),
        wheel(f"{name}_WheelRL", 0.38, 0.25, (-1.55, -0.9, 0.38)),
        wheel(f"{name}_WheelRR", 0.38, 0.25, (-1.55, 0.9, 0.38)),
    ])


def citizen(name):
    return combine(name, [
        cylinder(f"{name}_Body", 0.24, 0.85, (0, 0, 0.55), "Trim"),
        cylinder(f"{name}_Head", 0.22, 0.38, (0, 0, 1.18), "Trim"),
    ])


def road(name, arms):
    # Extend slightly across the logical four-meter cell edge so adjacent pieces
    # overlap instead of exposing a headset-visible seam from raster precision.
    arm_length = 2.6
    arm_offset = 2.9
    parts = [box(f"{name}_Center", (3.2, 3.2, 0.18), (0, 0, 0.09), "Road")]
    for direction in arms:
        if direction == "N":
            dims, loc = (3.2, arm_length, 0.18), (0, arm_offset, 0.09)
        elif direction == "S":
            dims, loc = (3.2, arm_length, 0.18), (0, -arm_offset, 0.09)
        elif direction == "E":
            dims, loc = (arm_length, 3.2, 0.18), (arm_offset, 0, 0.09)
        else:
            dims, loc = (arm_length, 3.2, 0.18), (-arm_offset, 0, 0.09)
        parts.append(box(f"{name}_{direction}", dims, loc, "Road"))
        # Continuous runtime graph markings own all lane paint; no baked raised dashes.
    return combine(name, parts)
