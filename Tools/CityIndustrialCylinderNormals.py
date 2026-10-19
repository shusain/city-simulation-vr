"""Smooth only circular industrial flue walls, including disconnected face corners."""
import math
from mathutils import Vector

def hollow_original_chimneys(obj):
    """Replace only solid top caps with annular rims and a closed inner liner."""
    import importlib.util
    from pathlib import Path
    spec=importlib.util.spec_from_file_location('CityOriginalFactoryBuilder',Path(__file__).with_name('Create-CityLowDensityVariants.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    groups=smooth_cylinders(obj)['cylinders'];solid=[r for r in groups if len(r['side_faces'])==12]
    if not solid:return 0
    remove=set();liners=[]
    for r in solid:
        cap=max(r['hard_faces'],key=lambda i:obj.data.polygons[i].center.z);remove.add(cap)
        outer=[obj.data.vertices[v].co.copy() for v in obj.data.polygons[cap].vertices]
        center=Vector((*r['center_m'],0));bottom=min(obj.data.vertices[v].co.z for i in r['side_faces'] for v in obj.data.polygons[i].vertices)+.05
        top=[Vector((center.x+(v.x-center.x)*.8,center.y+(v.y-center.y)*.8,v.z)) for v in outer]
        low=[Vector((v.x,v.y,bottom)) for v in top];liners.append((outer,top,low))
    b=module.Builder(obj);b.copy([p.index for p in obj.data.polygons if p.index not in remove])
    for outer,top,low in liners:
        for i in range(len(outer)):
            j=(i+1)%len(outer)
            b.face([outer[i],outer[j],top[j],top[i]],2)
            b.face([top[i],top[j],low[j],low[i]],2)
        b.face(low,2)
    temp=b.make(obj.name+'_HollowCopy');old=obj.data;obj.data=temp.data
    import bpy
    bpy.data.objects.remove(temp,do_unlink=True)
    if not old.users:bpy.data.meshes.remove(old)
    return len(solid)

def smooth_cylinders(obj, mechanical_slot=2):
    mesh=obj.data;mesh.update()
    faces=[p for p in mesh.polygons if p.material_index==mechanical_slot]
    coords={p.index:{tuple(round(float(a),5) for a in mesh.vertices[v].co) for v in p.vertices} for p in faces}
    todo=set(coords);cylinders=[];selected=set()
    while todo:
        group={todo.pop()};points=set().union(*(coords[i] for i in group))
        while True:
            hits={i for i in todo if points&coords[i]}
            if not hits:break
            todo-=hits;group|=hits
            for i in hits:points|=coords[i]
        xy={p[:2] for p in points}
        if len(xy)<8:continue
        center=tuple((min(p[a] for p in xy)+max(p[a] for p in xy))/2 for a in (0,1))
        radii=[math.hypot(p[0]-center[0],p[1]-center[1]) for p in xy]
        outer=max(radii)
        # Solid cylinders or annular tubes: circular XY rings, no square curbs.
        if any(min(abs(r-outer),abs(r-outer*.8))>2e-5 for r in radii):continue
        sides={i for i in group if len({p[:2] for p in coords[i]})==2}
        assert len(sides) in (12,24),(obj.name,len(sides))
        selected|=sides;cylinders.append(dict(center_m=list(center),radius_m=outer,side_faces=sorted(sides),hard_faces=sorted(group-sides)))
    assert cylinders,obj.name
    # These authoring meshes have separate vertices per face. Smooth flags alone
    # cannot average across those seams; provide the averaged loop normals.
    sums={}
    for i in selected:
        p=mesh.polygons[i]
        for li in p.loop_indices:
            key=tuple(round(float(a),5) for a in mesh.vertices[mesh.loops[li].vertex_index].co)
            sums[key]=sums.get(key,Vector())+p.normal
    normals=[None]*len(mesh.loops)
    for p in mesh.polygons:
        p.use_smooth=p.index in selected
        for li in p.loop_indices:
            key=tuple(round(float(a),5) for a in mesh.vertices[mesh.loops[li].vertex_index].co)
            normals[li]=tuple(sums[key].normalized() if p.index in selected else p.normal)
    mesh.normals_split_custom_set(normals);mesh.update()
    assert mesh.has_custom_normals
    return dict(cylinders=cylinders,smoothed_faces=len(selected),custom_corner_normals=True,hard_caps=True)
