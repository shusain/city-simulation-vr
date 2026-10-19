"""Bake recorded Unreal Chaos poses into ordinary static Blender/FBX meshes."""
from pathlib import Path
import json,sys,importlib.util,hashlib,math
import bpy
from mathutils import Vector,Quaternion,Matrix
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DestructionStudy';sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx
spec=importlib.util.spec_from_file_location('CityChaosBakeBuilder',ROOT/'Tools/Create-CityLowDensityVariants.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def main():
    doc=json.loads((OUT/'Fragments.json').read_text());simulation=json.loads((OUT/'Simulation.json').read_text())
    bpy.ops.wm.open_mainfile(filepath=str(OUT/'CityDestructionStudy.blend'));mirror=Matrix.Diagonal((1,-1,1));rows=[]
    for index,snapshot in enumerate(simulation['snapshots'][1:]):
        builder=module.Builder(bpy.data.objects[doc['fragments'][0]['name']])
        for row in snapshot['transforms']:
            assert all(math.isfinite(v) for v in row['translation_cm']+row['rotation_xyzw']+row['scale'])
            obj=bpy.data.objects[row['name']];builder.source=obj
            x,y,z,w=row['rotation_xyzw'];rotation=mirror@Quaternion((w,x,y,z)).to_matrix()@mirror
            scale=Vector(row['scale']);translation=mirror@Vector(row['translation_cm'])/100
            def transform(v,rotation=rotation,scale=scale,translation=translation):return rotation@Vector((v.x*scale.x,v.y*scale.y,v.z*scale.z))+translation
            builder.copy(range(len(obj.data.polygons)),transform)
        name=['SM_Study_Home_EarlyCollapse','SM_Study_Home_Collapsed','SM_Study_Home_Settled'][index];obj=builder.make(name)
        for a in obj.data.color_attributes:
            for c in a.data:c.color=(c.color[0],c.color[1],c.color[2],1)
        obj.data.calc_loop_triangles();path=OUT/(name+'.fbx')
        with checked_fbx(obj,path,doc['slots'],verify_alpha=True):pass
        rows.append(dict(name=name,seconds=snapshot['seconds'],export=path.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),triangles=len(obj.data.loop_triangles),dimensions_m=list(obj.dimensions),awake_pieces=sum(r['awake'] for r in snapshot['transforms'])))
        obj.location=(50+index*25,0,0)
    bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CityDestructionBaked.blend'))
    bindings={s:'/Game/CitySim/Art/Materials/'+('MI_CharredBrick' if s in ('ResidentialBrick','FractureInterior') else 'MI_CharredRoofShingles' if s=='RoofShingles' else 'MI_CharredTrim') for s in doc['slots']}
    (OUT/'Baked.json').write_text(json.dumps(dict(source_simulation='Simulation.json',physics_engine=simulation['physics_engine'],meshes=rows,slots=doc['slots'],material_bindings=bindings,static_only=True,production_damage_replaced=False,animation_frames_retained=len(simulation['frames'])),indent=2)+'\n')
    print('CITYSIM_CHAOS_STATIC_BAKE_PASS three static meshes; recorded physics transforms; production stand-ins retained')
if __name__=='__main__':main()
