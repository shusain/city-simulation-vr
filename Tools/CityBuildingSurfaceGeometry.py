"""Focused D79 source geometry edits, kept separate from production exports."""
import math
import bpy
import bmesh

REFINED_FORMS = {('Commercial','Medium'),('Industrial','Medium'),('Industrial','High')}


def components(obj):
    bm=bmesh.new(); bm.from_mesh(obj.data); bm.verts.ensure_lookup_table()
    remaining=set(bm.verts); result=[]
    while remaining:
        pending=[remaining.pop()]; vertices=[]
        while pending:
            vertex=pending.pop(); vertices.append(vertex)
            for edge in vertex.link_edges:
                other=edge.other_vert(vertex)
                if other in remaining: remaining.remove(other); pending.append(other)
        low=[min(v.co[i] for v in vertices) for i in range(3)]
        high=[max(v.co[i] for v in vertices) for i in range(3)]
        result.append(dict(vertices=[v.index for v in vertices],low=low,high=high,
                           size=[high[i]-low[i] for i in range(3)]))
    bm.free(); return result


def refine(obj, category, density, damaged, intact=None):
    """Move selected components and append windows; retain all other source faces.

    Damaged source vertices keep their existing vertical deformation factor.
    Added hall windows lie below the source damage threshold in both factories.
    """
    mesh=obj.data
    if damaged:
        assert intact['topology']==[tuple(sorted(p.vertices)) for p in mesh.polygons]
        before=[v.co.copy() for v in mesh.vertices]
        edited=intact['edited']; original=intact['original']; modified=intact['modified']
    else:
        original=[v.co.copy() for v in mesh.vertices]
        topology=[tuple(sorted(p.vertices)) for p in mesh.polygons]
        edited=set(); parts=components(obj)
        def match(size):
            found=[p for p in parts if all(abs(a-b)<.002 for a,b in zip(p['size'],size))]
            assert len(found)==1, (category,density,size,len(found))
            return found[0]
        def place(part,center,size=None,bottom=None):
            size=size or part['size']
            old_center=[(a+b)/2 for a,b in zip(part['low'],part['high'])]
            for index in part['vertices']:
                v=mesh.vertices[index]
                for axis in (0,1):
                    v.co[axis]=center[axis]+(v.co[axis]-old_center[axis])*size[axis]/part['size'][axis]
                if bottom is not None:
                    # Seat equipment on the roof without increasing its top.
                    v.co.z=bottom+(v.co.z-part['low'][2])*(part['high'][2]-bottom)/part['size'][2]
                edited.add(index)
        if category=='Commercial':
            sign=match((7,1,3))
            for index in sign['vertices']:
                mesh.vertices[index].co.y+=1.1
                mesh.vertices[index].co.z+=1
                edited.add(index)
        else:
            roof=13 if density=='Medium' else 18
            box=match((5,5,8) if density=='Medium' else (7,7,14))
            place(box,(4.4,4) if density=='Medium' else (3.9,3.75),
                  size=(5,5,8) if density=='Medium' else (6.2,5.5,14),bottom=roof)
            stacks=sorted([p for p in parts if p['size'][0]<2.5 and p['size'][1]<2.5 and p['size'][2]>10],
                          key=lambda p:p['low'][0])
            assert len(stacks)==(2 if density=='Medium' else 3)
            centers=((-4.5,4.5),(0,4.5)) if density=='Medium' else ((-5,3),(-1.5,4.5),(-5,6))
            for part,center in zip(stacks,centers): place(part,center,bottom=roof)
            vents=[p for p in parts if all(abs(a-b)<.002 for a,b in zip(p['size'],(2.3,6,.8)))]
            assert len(vents)==3
            for part in vents:
                center_x=(part['low'][0]+part['high'][0])/2
                place(part,(center_x,-2.1 if density=='Medium' else -2.5))
            # Open the existing twelve clerestory panes about their top edge.
            colors=mesh.color_attributes.get('WindowMask')
            upper=[p for p in parts if abs(p['size'][2]-1.2)<.002 and min(p['size'][:2])<.001]
            assert len(upper)==12
            for part in upper:
                center=[(a+b)/2 for a,b in zip(part['low'],part['high'])]
                axis=0 if part['size'][0]<.001 else 1
                outward=1 if center[axis]>0 else -1
                for index in part['vertices']:
                    vertex=mesh.vertices[index]; drop=part['high'][2]-vertex.co.z
                    vertex.co[axis]+=outward*drop*math.sin(math.radians(20))
                    vertex.co.z=part['high'][2]-drop*math.cos(math.radians(20))
                    edited.add(index)
        modified=[v.co.copy() for v in mesh.vertices]
        intact=dict(original=original,modified=modified,edited=edited,topology=topology)
    if damaged:
        for index in edited:
            v=mesh.vertices[index]; v.co.x=modified[index].x; v.co.y=modified[index].y
            v.co.z=modified[index].z*before[index].z/original[index].z if original[index].z else modified[index].z
    mesh.update()
    scope=dict(edited_source_vertices=sorted(edited),source_vertex_count=len(mesh.vertices),source_face_count=len(mesh.polygons))
    if category=='Commercial': return intact,dict(sign_bottom_m=10,sign_roof_inset_m=1.1,**scope)

    # Author explicit window/opening/frame faces using the existing trim section.
    # A third source semantic slot marks exposed-metal frames (same final HVAC MI).
    if not any(m.name.split('.')[0]=='MetalDetail' for m in mesh.materials):
        metal=bpy.data.materials.get('MetalDetail') or bpy.data.materials.new('MetalDetail')
        mesh.materials.append(metal)
    metal_index=next(i for i,m in enumerate(mesh.materials) if m.name.split('.')[0]=='MetalDetail')
    attr=mesh.color_attributes.get('WindowMask')
    vertices=[tuple(v.co) for v in mesh.vertices]; faces=[tuple(p.vertices) for p in mesh.polygons]
    materials=[p.material_index for p in mesh.polygons]
    colors=[tuple(attr.data[i].color) for p in mesh.polygons for i in p.loop_indices]
    original_faces=len(faces); window_count=0
    def face(points, material_index=1, window=False):
        start=len(vertices); vertices.extend(points); faces.append(tuple(range(start,start+len(points))))
        materials.append(material_index)
        colors.extend([(1,1,1,1 if damaged or not window else 0)]*len(points))
    def world(x,depth,z,side):
        angle=side*math.pi/2
        return (x*math.cos(angle)+depth*math.sin(angle),x*math.sin(angle)-depth*math.cos(angle),z)
    rows=(3.4,7.2) if density=='Medium' else (3.4,7.8,11.6)
    upper_center=10.8 if density=='Medium' else 15.8
    for side in range(4):
        depth=(7 if side%2==0 else 7.5)+.035
        for x in (-4.6,0,4.6):
            for z in rows:
                if side==0 and x==0 and z-.9<=(6 if density=='Medium' else 7): continue
                window_x=x if side!=0 or x==0 else math.copysign(5.5,x)
                face([world(a,depth,b,side) for a,b in ((window_x-1.1,z-.9),(window_x+1.1,z-.9),
                                                     (window_x+1.1,z+.9),(window_x-1.1,z+.9))],window=True)
                window_count+=1
            # A dark fixed opening sits behind the tilted existing pane.
            face([world(a,depth-.008,b,side) for a,b in ((x-1.1,upper_center-.6),(x+1.1,upper_center-.6),
                                                        (x+1.1,upper_center+.6),(x-1.1,upper_center+.6))],window=True)
            outer=[(x-1.2,upper_center-.7),(x+1.2,upper_center-.7),(x+1.2,upper_center+.7),(x-1.2,upper_center+.7)]
            inner=[(x-1.1,upper_center-.6),(x+1.1,upper_center-.6),(x+1.1,upper_center+.6),(x-1.1,upper_center+.6)]
            for i in range(4):
                j=(i+1)%4
                face([world(a,depth+.012,b,side) for a,b in (outer[i],outer[j],inner[j],inner[i])],metal_index)
    rebuilt=bpy.data.meshes.new(mesh.name+'_Refined')
    rebuilt.from_pydata(vertices,[],faces)
    for m in mesh.materials: rebuilt.materials.append(m)
    for p,index in zip(rebuilt.polygons,materials): p.material_index=index
    mask=rebuilt.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
    assert len(mask.data)==len(colors)
    for c,value in zip(mask.data,colors): c.color=value
    obj.data=rebuilt; rebuilt.update()
    return intact,dict(added_facade_windows=window_count,open_upper_windows=12,open_angle_degrees=20,
                       source_faces_preserved=original_faces,added_faces=len(faces)-original_faces,**scope)
