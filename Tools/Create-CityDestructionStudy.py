"""Closed masonry/roof fragments for an offline Unreal Chaos collapse study."""
from pathlib import Path
import json,random,sys,hashlib
import bpy
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import load_object,checked_fbx
OUT=ROOT/'Artifacts/D104/DestructionStudy';OUT.mkdir(parents=True,exist_ok=True)
RNG=random.Random(104106)
def prism(points,depth):
    n=(points[1]-points[0]).cross(points[2]-points[0]).normalized();count=len(points)
    vertices=points+[p-n*depth for p in points]
    faces=[list(range(count)),list(reversed(range(count,count*2)))]+[[i,i+count,(i+1)%count+count,(i+1)%count] for i in range(count)]
    mesh=bpy.data.meshes.new('Prism');mesh.from_pydata(vertices,[],faces);mesh.update();return mesh
def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text());source=ROOT/doc['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==doc['source_sha256']
    original=load_object(source,'SM_LowDensity_Home_A');standin=load_object(source,'SM_LowDensity_Home_A_Damaged')
    original.name='Reference_Intact';standin.name='Reference_StandIn';standin.location=(25,0,0)
    entry=next(e for e in doc['assets'] if e['name']=='SM_LowDensity_Home_A');slots=entry['slots']+['FractureInterior']
    materials=list(original.data.materials);core=bpy.data.materials.new('FractureInterior');core.use_nodes=True
    shader=core.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=(.20,.15,.11,1);shader.inputs['Roughness'].default_value=.9;materials.append(core)
    walls=sorted([p for p in original.data.polygons if p.material_index==0 and abs(p.normal.z)<1e-4],key=lambda p:p.area,reverse=True)[:4]
    roofs=[p for p in original.data.polygons if (p.material_index==2 and p.normal.z>.1 and p.area>1) or (p.material_index==1 and abs(p.normal.z)<.1 and p.area>5)]
    structural={p.index for p in walls+roofs}
    # The rendered house body is a box shell. Its hidden top/bottom caps are
    # not giant debris panels to attach to one wall's convex collision hull.
    structural|={p.index for p in original.data.polygons if p.material_index==0 and abs(p.normal.z)>.95 and p.area>20}
    structural|={p.index for p in original.data.polygons if p.material_index==1 and p.normal.z<-.9 and p.area>20}
    fragments=[]
    for polygon in walls+roofs:
        points=[original.data.vertices[v].co.copy() for v in polygon.vertices];wall=polygon in walls;depth=.24 if wall else .14
        base=prism(points,depth);normal=polygon.normal
        seeds=[]
        if wall:
            bottom=min(points,key=lambda p:p.z);axis=max(points,key=lambda p:(p-bottom).length if abs(p.z-bottom.z)<.01 else -1)-bottom
            height=max(p.z for p in points)-bottom.z
            for row in range(3):
                for col in range(5):seeds.append(bottom+axis*((col+.5+RNG.uniform(-.22,.22))/5)+Vector((0,0,height*(row+.5+RNG.uniform(-.18,.18))/3))-normal*depth*.5)
        else:
            triangles=[[points[i] if isinstance(i,int) else i for i in triangle] for triangle in tessellate_polygon([points])]
            for i in range(3):
                a,b,c=RNG.choice(triangles);u=RNG.random()**.5;v=RNG.random();seeds.append(a*(1-u)+b*(u*(1-v))+c*(u*v)-normal*depth*.5)
        cells=[]
        if wall:
            xs=[0]+[(i+RNG.uniform(-.14,.14))/5 for i in range(1,5)]+[1]
            for row in range(3):
                for col in range(5):
                    p=[bottom+axis*xs[col]+Vector((0,0,height*row/3)),bottom+axis*xs[col+1]+Vector((0,0,height*row/3)),bottom+axis*xs[col+1]+Vector((0,0,height*(row+1)/3)),bottom+axis*xs[col]+Vector((0,0,height*(row+1)/3))]
                    if (p[1]-p[0]).cross(p[2]-p[0]).dot(normal)<0:p.reverse()
                    cells.append((p,sum(p,Vector())/4-normal*depth*.5))
        else:
            for triangle in tessellate_polygon([points]):
                a,b,c=[points[i] if isinstance(i,int) else i for i in triangle]
                ab=(a+b)*.5;bc=(b+c)*.5;ca=(c+a)*.5
                for p in ([a,ab,ca],[ab,b,bc],[ca,bc,c],[ab,bc,ca]):cells.append((p,sum(p,Vector())/3-normal*depth*.5))
        for points,seed in cells:
            print('FRACTURE_CELL',len(fragments),tuple(seed),flush=True)
            mesh=prism(points,depth)
            volume=mesh.polygons[0].area*depth
            for m in materials:mesh.materials.append(m)
            mesh.polygons.foreach_set('material_index',[polygon.material_index]+[len(slots)-1]*(len(mesh.polygons)-1))
            o=bpy.data.objects.new('Fragment_'+str(len(fragments)).zfill(3),mesh);bpy.context.scene.collection.objects.link(o)
            mesh.uv_layers.new(name='UVMap');mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
            # Adding a corner attribute can reallocate CustomData. Reacquire
            # layer handles after both allocations rather than using stale RNA.
            uv=mesh.uv_layers['UVMap'];colors=mesh.color_attributes['WindowMask']
            for p in mesh.polygons:
                axis=max(range(3),key=lambda a:abs(p.normal[a]));sign=1 if p.normal[axis]>0 else -1
                for li in p.loop_indices:
                    v=mesh.vertices[mesh.loops[li].vertex_index].co
                    pair=(sign*v.y,v.z) if axis==0 else (-sign*v.x,v.z) if axis==1 else (v.x,v.y)
                    uv.data[li].uv=(pair[0]/2.4,pair[1]/2.4)
                    colors.data[li].color=original.data.color_attributes['WindowMask'].data[polygon.loop_start].color if p.index==0 else (1,1,1,1)
            fragments.append(dict(object=o,seed=seed,volume=volume,wall=wall,static=wall and seed.z<1,core=mesh.copy(),normal=normal.copy()))
        bpy.data.meshes.remove(base)
    # Preserve authored windows, doors, porch and trim by attaching their faces
    # to the nearest structural fragment. Core fracture cells remain solids.
    for p in original.data.polygons:
        if p.index in structural:continue
        f=min(fragments,key=lambda f:(f['seed']-p.center).length_squared)
        f.setdefault('details',[]).append(p.index)
    import importlib.util
    spec=importlib.util.spec_from_file_location('CityDestructionBuilder',ROOT/'Tools/Create-CityLowDensityVariants.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    rows=[];exports=OUT/'Fragments';exports.mkdir(exist_ok=True)
    for f in fragments:
        o=f['object'];b=module.Builder(o);b.copy(range(len(o.data.polygons)))
        b.source=original;b.copy(f.get('details',[]));b.source=o;combined=b.make(o.name+'_WithDetails');old=o.data;o.data=combined.data;bpy.data.objects.remove(combined,do_unlink=True)
        if not old.users:bpy.data.meshes.remove(old)
        center=sum((v.co for v in o.data.vertices),Vector())/len(o.data.vertices)
        for v in o.data.vertices:v.co-=center
        path=exports/(o.name+'.fbx')
        with checked_fbx(o,path,slots,verify_alpha=True):pass
        # A dedicated UCX hull uses only the structural solid, excluding
        # attached windows, paths and trim from physics. Shorten X-wall ends
        # inside Y-walls to avoid overlapping corner bodies at activation.
        proxy=bpy.data.objects.new('UCX_'+o.name+'_00',f['core']);bpy.context.scene.collection.objects.link(proxy)
        for v in proxy.data.vertices:
            if f['wall'] and abs(f['normal'].x)>.9:
                if v.co.y>4.5-.001:v.co.y-=.24
                if v.co.y<-4.5+.001:v.co.y+=.24
            v.co-=center
        from CityBuildingStudyIO import _restore_names
        saved=[(m,m.name) for m in bpy.data.materials]
        for i,(m,n) in enumerate(saved):m.name='_D104CollisionExport_'+str(i)
        for m,label in zip(o.data.materials,slots):m.name=label
        try:
            bpy.ops.object.select_all(action='DESELECT');o.select_set(True);proxy.select_set(True);bpy.context.view_layer.objects.active=o
            bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
        finally:_restore_names(saved)
        bpy.data.objects.remove(proxy,do_unlink=True)
        o.location=center;rows.append(dict(name=o.name,export=path.relative_to(ROOT).as_posix(),center_m=list(center),static=f['static'],volume_m3=f['volume'],wall=f['wall']))
    original.location=(-25,0,0);bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CityDestructionStudy.blend'))
    bindings=dict(entry['material_bindings']);bindings['FractureInterior']='/Game/CitySim/Art/Materials/MI_CharredBrick'
    report=dict(task='D104',study='Closed masonry/roof subdivisions / offline Chaos rigid-body collapse',source=doc['source'],source_sha256=doc['source_sha256'],fragments=rows,slots=slots,materials=bindings,seed=104106,source_geometry_details_retained=True,wall_thickness_m=.24,roof_thickness_m=.14,production_damage_replaced=False)
    (OUT/'Fragments.json').write_text(json.dumps(report,indent=2)+'\n');print('CITYSIM_DESTRUCTION_SOLIDS_PASS',len(rows),'closed structural fragments with retained authored details')
if __name__=='__main__':main()
