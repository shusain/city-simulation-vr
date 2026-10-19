"""Shared connected-part park refinement; importing creates no data or files.

Review and integration LOD profiles preserve the existing two tessellation levels.
Source loading, scene setup, exports, rendering and promotion remain caller-owned.
"""
from dataclasses import dataclass
from pathlib import Path
import bpy
import bmesh
import math
from mathutils import Vector


@dataclass(frozen=True)
class ParkDetail:
    cylinder_segments: int = 48
    crown_subdivisions: int = 3
    ramp_steps: int = 32

    def __post_init__(self):
        if any(type(value) is not int for value in (self.cylinder_segments, self.crown_subdivisions, self.ramp_steps)):
            raise ValueError('Park detail counts must be integers')
        if self.cylinder_segments < 12 or self.cylinder_segments % 12:
            raise ValueError('Cylinder segments must be a positive multiple of 12')
        if self.crown_subdivisions < 1 or self.ramp_steps < 1:
            raise ValueError('Crown subdivisions and ramp steps must be positive')


REVIEW_DETAIL = ParkDetail()
INTEGRATION_LOD_DETAIL = ParkDetail(cylinder_segments=24, crown_subdivisions=2, ramp_steps=16)
ENVIRONMENT_SOURCE = Path(__file__).resolve().parents[1]/'ArtSource/Blender/CityEnvironmentKit.blend'


def append(source, name):
    with bpy.data.libraries.load(str(source), link=False) as (available, target):
        assert name in available.objects, (source, name)
        target.objects = [name]
    obj = target.objects[0]
    bpy.context.scene.collection.objects.link(obj)
    obj.location = (0, 0, 0)
    return obj


def components(mesh):
    neighbors = [set() for _ in mesh.vertices]
    for edge in mesh.edges:
        a, b = edge.vertices
        neighbors[a].add(b); neighbors[b].add(a)
    remaining = set(range(len(neighbors)))
    result = []
    while remaining:
        stack = [min(remaining)]; found = set(stack)
        while stack:
            for other in neighbors[stack.pop()] - found:
                found.add(other); stack.append(other)
        remaining -= found
        result.append(sorted(found))
    return result


def bounds(obj):
    return [[min(v.co[i] for v in obj.data.vertices) for i in range(3)],
            [max(v.co[i] for v in obj.data.vertices) for i in range(3)]]


def triangles(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def mesh_corners(obj):
    """Position/color/UV/material/smooth tuples survive FBX vertex reordering."""
    mesh = obj.data
    attr = mesh.color_attributes.active_color
    uv = mesh.uv_layers.active
    return sorted((tuple(round(c, 4) for c in mesh.vertices[mesh.loops[i].vertex_index].co),
                   tuple(round(c, 3) for c in attr.data[i].color) if attr else (),
                   tuple(round(c, 4) for c in uv.data[i].uv) if uv else (),
                   mesh.materials[p.material_index].name.split('.')[0], p.use_smooth)
                  for p in mesh.polygons for i in p.loop_indices)


def refine(obj, asset_id, *, detail=REVIEW_DETAIL, environment_source=ENVIRONMENT_SOURCE):
    """Refine curves at the chosen detail; retain unrelated faces and attributes.

    The simple park's pruned crown/trunk retain their fixed eight/twelve sides.
    Other cylinders, crowns and ramps use the supplied immutable detail profile.
    """
    mesh = obj.data
    verts, faces, slots, smooth, rgba, uv = [], [], [], [], [], []
    attr = mesh.color_attributes.active_color
    uv_layer = mesh.uv_layers.active
    changes = []
    simple = asset_id=='building.park.a'
    tree_materials={}
    if simple:
        with bpy.data.libraries.load(str(environment_source),link=False) as (available,target):
            target.materials=['Bark','Canopy']
        for material in target.materials:
            tree_materials[material.name.split('.')[0]]=len(mesh.materials)
            mesh.materials.append(material)
    for comp in components(mesh):
        comp_set = set(comp)
        polygons = [p for p in mesh.polygons if p.vertices[0] in comp_set]
        coords = [mesh.vertices[i].co.copy() for i in comp]
        low = Vector(tuple(min(v[i] for v in coords) for i in range(3)))
        high = Vector(tuple(max(v[i] for v in coords) for i in range(3)))
        center = (low+high)/2
        size = high-low
        cylinder = len(comp) == 24 and len(polygons) == 14
        tree = ('.park.' in asset_id and center.z > 3
                and mesh.materials[polygons[0].material_index].name.split('.')[0] == 'Park'
                and (cylinder or len(comp) == 12))
        ramp = '.skate' in asset_id and len(comp) == 16 and abs(center.y) < .01
        trunk = simple and cylinder and center.z<3 and abs(center.x)>6 and abs(center.y)>6
        start = len(verts)
        generated = []
        generated_smooth = []
        if tree:
            # A single symmetric clipped crown: flat facets, no intersecting lobe.
            if simple:
                n=8;bottom=len(verts);radius=size.x*.35
                verts.append((center.x,center.y,low.z))
                for height,reach in ((.17,.60),(.40,1),(.65,.93),(.83,.54)):
                    verts.extend((center.x+radius*reach*math.cos(j*math.tau/n),
                                  center.y+radius*reach*math.sin(j*math.tau/n),
                                  low.z+size.z*height) for j in range(n))
                top=len(verts);verts.append((center.x,center.y,high.z))
                generated=[(bottom,bottom+1+(j+1)%n,bottom+1+j) for j in range(n)]
                for ring in range(3):
                    a=bottom+1+ring*n;b=a+n
                    generated += [(a+j,a+(j+1)%n,b+(j+1)%n,b+j) for j in range(n)]
                last=bottom+1+3*n
                generated += [(last+j,last+(j+1)%n,top) for j in range(n)]
                generated_smooth=[False]*len(generated)
                changes.append(dict(kind='pruned_crown',vertices_before=len(comp),lobes=1,triangles_after=64,
                                    radial_segments=8,symmetry='centered eightfold crown',flat_shaded=True))
            else:
                temp=bmesh.new();bmesh.ops.create_icosphere(temp,subdivisions=detail.crown_subdivisions,radius=1)
                temp.verts.ensure_lookup_table();temp.verts.index_update()
                points=[v.co.copy() for v in temp.verts]
                sphere_low=Vector(tuple(min(v[i] for v in points) for i in range(3)))
                sphere_high=Vector(tuple(max(v[i] for v in points) for i in range(3)))
                verts.extend(tuple(center[i]+((v[i]-sphere_low[i])/(sphere_high[i]-sphere_low[i])*2-1)*size[i]/2
                                   for i in range(3)) for v in points)
                generated=[tuple(start+v.index for v in f.verts) for f in temp.faces]
                temp.free();generated_smooth=[True]*len(generated)
                changes.append(dict(kind='crown',vertices_before=len(comp),lobes=1,triangles_after=sum(len(f)-2 for f in generated)))
        elif cylinder:
            # Source cylinders have twelve corresponding lower/upper ring vertices.
            # Keep their exact angles and radial extent; insert three angular samples
            # between each pair. Damaged Z profiles interpolate their existing shape.
            caps = [p for p in polygons if len(p.vertices)==12]
            assert len(caps)==2
            rings = [[mesh.vertices[i].co.copy() for i in p.vertices] for p in caps]
            rings.sort(key=lambda ring: sum(v.z for v in ring)/12)
            for ring in rings:
                ring.sort(key=lambda v: math.atan2(v.y-center.y,v.x-center.x))
            assert all(abs((v-center).xy.length-(rings[0][0]-center).xy.length)<.002 for v in coords)
            n = 12 if trunk else detail.cylinder_segments
            for ring_index,ring in enumerate(rings):
                for j, a in enumerate(ring):
                    b = ring[(j+1)%12]
                    angle = math.atan2(a.y-center.y, a.x-center.x)
                    delta = (math.atan2(b.y-center.y, b.x-center.x)-angle+math.pi) % math.tau-math.pi
                    radius = (a-center).xy.length
                    if trunk:radius=.32 if ring_index==0 else .17
                    for k in range(n//12):
                        t = k/(n//12)
                        verts.append((center.x+radius*math.cos(angle+delta*t),
                                      center.y+radius*math.sin(angle+delta*t), a.z*(1-t)+b.z*t))
            generated = [tuple(start+j for j in range(n-1,-1,-1)),
                         tuple(start+n+j for j in range(n))]
            generated += [(start+j,start+(j+1)%n,start+n+(j+1)%n,start+n+j) for j in range(n)]
            generated_smooth = [False,False]+[not trunk]*n
            changes.append(dict(kind='pruned_trunk' if trunk else 'cylinder', radial_segments_before=12, radial_segments_after=n,
                                center=list(center), radius=radius, height=size.z))
        elif ramp:
            assert 'damaged' not in asset_id, 'Use accepted damaged source independently'
            sign = 1 if center.x > 0 else -1
            steps = detail.ramp_steps
            profile = [(8+5*i/steps, .36+.12*(5*i/steps)**2) for i in range(steps+1)]
            profile += [(13,.16),(8,.16)]
            n = len(profile)
            verts.extend((sign*x, y, z) for y in (-3,3) for x,z in profile)
            generated = [tuple(start+j for j in range(n-1,-1,-1)),tuple(start+n+j for j in range(n))]
            generated += [(start+i,start+(i+1)%n,start+n+(i+1)%n,start+n+i) for i in range(n)]
            generated_smooth = [False,False]+[i < steps for i in range(n)]
            changes.append(dict(kind='ramp', riding_panels_before=5, riding_panels_after=steps,
                                maximum_curve_error_m=.12*(5/steps)**2/4))
        if generated:
            p = polygons[0]
            color = tuple(attr.data[p.loop_start].color) if attr else (1,1,1,1)
            faces.extend(generated); smooth.extend(generated_smooth)
            slot=tree_materials['Canopy'] if simple and tree else tree_materials['Bark'] if trunk else p.material_index
            slots.extend([slot]*len(generated))
            rgba.extend([[color]*len(f) for f in generated])
            uv.extend([[(0,0)]*len(f) for f in generated])
        else:
            index = {old:start+i for i,old in enumerate(comp)}
            verts.extend(coords)
            for p in polygons:
                faces.append(tuple(index[i] for i in p.vertices))
                slots.append(p.material_index); smooth.append(p.use_smooth)
                rgba.append([tuple(attr.data[i].color) if attr else (1,1,1,1) for i in p.loop_indices])
                uv.append([tuple(uv_layer.data[i].uv) if uv_layer else (0,0) for i in p.loop_indices])
    result = bpy.data.meshes.new(obj.name+'Refined')
    result.from_pydata(verts, [], faces)
    for mat in mesh.materials: result.materials.append(mat)
    color = result.color_attributes.new(name=attr.name if attr else 'WindowMask',type='BYTE_COLOR',domain='CORNER')
    result.color_attributes.active_color = color
    uv_out = result.uv_layers.new(name=uv_layer.name) if uv_layer else None
    for p,slot,shade,colors,tex in zip(result.polygons,slots,smooth,rgba,uv):
        p.material_index = slot; p.use_smooth = shade
        for i,c,t in zip(p.loop_indices,colors,tex):
            color.data[i].color = c
            if uv_out: uv_out.data[i].uv = t
    bm = bmesh.new(); bm.from_mesh(result)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    assert not any(f.calc_area()<1e-9 for f in bm.faces), obj.name
    assert not any(e.is_boundary for e in bm.edges), obj.name
    bm.to_mesh(result); bm.free()
    obj.data = result
    assert changes, 'No curves matched: '+obj.name
    return changes
