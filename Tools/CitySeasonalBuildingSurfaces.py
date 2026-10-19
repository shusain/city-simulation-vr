"""Keep the low-home lawn/driveway/wreath geometry with D79 wall/roof surfaces."""
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
def apply_home(obj):
    with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityBuildingSurfacePrototype.blend'),link=False) as (available,loaded):
        loaded.materials=['ResidentialBrick','Roof','RoofShingles']
    assert all(loaded.materials)
    old=[p.material_index for p in obj.data.polygons]
    colors=obj.data.color_attributes.get('WindowMask');alpha=[v.color[3] for v in colors.data]
    obj.data.materials.clear()
    for material in loaded.materials:obj.data.materials.append(material)
    uv=obj.data.uv_layers.active or obj.data.uv_layers.new(name='UVMap')
    roof_count=0
    for p,index in zip(obj.data.polygons,old):
        is_roof=index in (1,2) and p.normal.z>.2 and all(5.999<obj.data.vertices[v].co.z<8.001 for v in p.vertices)
        p.material_index=2 if is_roof else index
        roof_count+=is_roof
        axis=max(range(3),key=lambda a:abs(p.normal[a]));sign=1 if p.normal[axis]>0 else -1
        tangent=p.normal.cross(Vector((1,0,0))).normalized() if is_roof else None
        for i in p.loop_indices:
            c=obj.data.vertices[obj.data.loops[i].vertex_index].co
            if is_roof:uv.data[i].uv=(c.x/4.8,c.dot(tangent)/4.8)
            elif index==0:
                a,b=(sign*c.y,c.z) if axis==0 else (-sign*c.x,c.z) if axis==1 else (sign*c.x,c.y)
                uv.data[i].uv=(a/2.4,b/2.4)
    assert roof_count==2 and alpha==[v.color[3] for v in colors.data]
    return {name:'/Game/CitySim/Art/Materials/MI_'+name for name in ('ResidentialBrick','Roof','RoofShingles')}
