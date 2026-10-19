"""D91 shared opaque window treatment for actual architectural panes.

Semantic box dimensions come from the existing civic authoring helpers.
Emergency lamps retain their original faces, colors, materials and masks.
"""
import bpy
from mathutils import Vector

def base_name(material):return material.name.split('.')[0]

def select_panes(obj,asset_id):
    mesh=obj.data;attr=mesh.color_attributes.get('WindowMask')
    if asset_id.startswith(('building.residential.','building.shop.','building.factory.')) or asset_id=='SM_Building_Residential_SeasonalSpace_A':
        return [p.index for p in mesh.polygons if attr and base_name(mesh.materials[p.material_index])=='Roof' and all(attr.data[i].color[3]<.01 for i in p.loop_indices)],set()
    sizes=[]
    if asset_id.startswith(('building.police.','building.fire.')):sizes=[(2,.12,h) for h in (1.3,1.5,1.6)]
    elif asset_id=='building.school.a':sizes=[(1.8,.1,1.6)]
    elif asset_id.startswith(('building.primaryschool.','building.secondaryschool.')):sizes=[(1.6,.12,1.5)]
    elif asset_id.startswith('building.college.'):sizes=[(1.4,.15,1.4)]
    elif asset_id.startswith('building.hospital.'):sizes=[(1.8,d,1.8) for d in (.1,.12)]
    elif asset_id.startswith(('building.wastedepot.','building.recycling.')):sizes=[(2.6,.12,1)]
    if not sizes:return [],set()
    links=[set() for _ in mesh.vertices]
    for e in mesh.edges:
        a,b=e.vertices;links[a].add(b);links[b].add(a)
    remaining=set(range(len(mesh.vertices)));components=[]
    while remaining:
        start=remaining.pop();todo=[start];component={start}
        while todo:
            for adjacent in links[todo.pop()]:
                if adjacent in remaining:remaining.remove(adjacent);component.add(adjacent);todo.append(adjacent)
        components.append(component)
    panes=[];window_faces=set()
    for component in components:
        if len(component)!=8:continue
        points=[mesh.vertices[i].co for i in component]
        low=Vector(tuple(min(p[i] for p in points) for i in range(3)));high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
        size=high-low
        if not any(all(abs(a-b)<.002 for a,b in zip(size,wanted)) for wanted in sizes):continue
        faces=[p for p in mesh.polygons if set(p.vertices).issubset(component)]
        assert len(faces)==6
        center=(low+high)/2
        # All current box windows lie on a front(-Y) or rear(+Y) facade.
        outward=1 if center.y>5 else -1
        pane=next(p for p in faces if p.normal.y*outward>.99)
        panes.append(pane.index);window_faces.update(p.index for p in faces)
    return panes,window_faces

def add_frames(obj,panes,window_faces,material):
    mesh=obj.data;colors=mesh.color_attributes.get('WindowMask')
    if not colors:
        colors=mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
        for value in colors.data:value.color=(1,1,1,1)
    uv=mesh.uv_layers.active or mesh.uv_layers.new(name='UVMap')
    slot=len(mesh.materials);mesh.materials.append(material)
    for fi in window_faces-set(panes):
        for li in mesh.polygons[fi].loop_indices:
            rgba=list(colors.data[li].color);rgba[3]=1;colors.data[li].color=rgba
    vertices=[];faces=[];specs=[]
    for fi in panes:
        pane=mesh.polygons[fi];assert len(pane.vertices)==4
        normal=pane.normal.normalized();up=(Vector((0,0,1))-normal*normal.z).normalized();right=up.cross(normal).normalized()
        points=[mesh.vertices[i].co.copy() for i in pane.vertices]
        center=sum(points,Vector())/4
        width=max(p.dot(right) for p in points)-min(p.dot(right) for p in points)
        height=max(p.dot(up) for p in points)-min(p.dot(up) for p in points)
        assert width>.5 and height>.5
        origin=center-right*width/2-up*height/2
        pane.material_index=slot
        for li in pane.loop_indices:
            d=mesh.vertices[mesh.loops[li].vertex_index].co-origin
            uv.data[li].uv=(d.dot(right)/width,d.dot(up)/height)
            rgba=list(colors.data[li].color);rgba[3]=0;colors.data[li].color=rgba
        def quad(points):
            start=len(vertices);vertices.extend(tuple(origin+right*x+up*z+normal*d) for x,z,d in points);faces.append(tuple(range(start,start+4)))
        border=min(.12,width*.075,height*.075);bar=min(.035,min(width,height)*.025)
        outer=[(0,0),(width,0),(width,height),(0,height)]
        inner=[(border,border),(width-border,border),(width-border,height-border),(border,height-border)]
        for i in range(4):
            j=(i+1)%4
            quad([(*outer[i],.015),(*outer[j],.015),(*inner[j],.095),(*inner[i],.095)])
            quad([(*inner[i],.095),(*inner[j],.095),(*inner[j],.008),(*inner[i],.008)])
        for x0,x1,z0,z1 in ((width/2-bar,width/2+bar,border,height-border),(border,width/2-bar,height/2-bar,height/2+bar),(width/2+bar,width-border,height/2-bar,height/2+bar)):
            quad([(x0,z0,.045),(x1,z0,.045),(x1,z1,.045),(x0,z1,.045)])
        for points in (((-.04,-.10,.015),(width+.04,-.10,.015),(width+.04,-.10,.18),(-.04,-.10,.18)),
            ((-.04,-.10,.18),(width+.04,-.10,.18),(width+.04,.02,.18),(-.04,.02,.18)),
            ((-.04,.02,.18),(width+.04,.02,.18),(width+.04,.02,.015),(-.04,.02,.015)),
            ((-.04,-.10,.015),(-.04,-.10,.18),(-.04,.02,.18),(-.04,.02,.015)),
            ((width+.04,-.10,.18),(width+.04,-.10,.015),(width+.04,.02,.015),(width+.04,.02,.18))):quad(points)
        specs.append(dict(polygon=fi,width_m=width,height_m=height,normal=list(normal),original_vertices=list(pane.vertices)))
    frame_mesh=bpy.data.meshes.new(obj.name+'_Frames');frame_mesh.from_pydata(vertices,[],faces);frame_mesh.update();frame_mesh.materials.append(material)
    frame_uv=frame_mesh.uv_layers.new(name='UVMap')
    for p in frame_mesh.polygons:
        axis=max(range(3),key=lambda i:abs(p.normal[i]));axes=[i for i in range(3) if i!=axis]
        for li in p.loop_indices:
            c=frame_mesh.vertices[frame_mesh.loops[li].vertex_index].co;frame_uv.data[li].uv=(c[axes[0]],c[axes[1]])
    attr=frame_mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
    for value in attr.data:value.color=(1,1,1,1)
    frames=bpy.data.objects.new(obj.name+'_Frames',frame_mesh);bpy.context.scene.collection.objects.link(frames)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);frames.select_set(True);bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
    return specs
