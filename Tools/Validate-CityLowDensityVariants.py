"""Read saved D103 meshes, exact body scale, UV/color/packed image closure."""
from pathlib import Path
import argparse,json,sys,hashlib,math
import bpy
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import digest,snapshot


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory',type=Path,default=ROOT/'Artifacts/D103')
    parser.add_argument('--first',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    out=args.output_directory.absolute()
    study=json.loads((out/'Study.json').read_text())
    source=ROOT/study['source']
    assert digest(source)==study['source_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(source))
    objects={}
    for record in study['assets']:
        obj=bpy.data.objects[record['name']]
        mesh=obj.data
        mesh.calc_loop_triangles()
        assert len(mesh.loop_triangles)==record['triangles']
        assert [m.name.split('.')[0] for m in mesh.materials]==record['slots']
        assert all(p.area>1e-7 for p in mesh.polygons)
        assert mesh.uv_layers.active and mesh.color_attributes.get('WindowMask')
        assert all(math.isfinite(x) for v in mesh.vertices for x in v.co)
        baseline=bpy.data.objects[record['category']+'_A_Current']
        for p,q in zip(list(mesh.polygons)[:6],list(baseline.data.polygons)[:6]):
            assert all((mesh.vertices[i].co-baseline.data.vertices[j].co).length<1e-6 for i,j in zip(p.vertices,q.vertices))
        signature=hashlib.sha256(json.dumps(snapshot(obj),sort_keys=True).encode()).hexdigest()
        objects[obj.name]=signature
        ruler=bpy.data.objects['Ruler_'+obj.name]
        assert abs(ruler.scale.z*(max(v.co.z for v in ruler.data.vertices)-min(v.co.z for v in ruler.data.vertices))-1.43175)<.001
        if study.get('revision',1)>=3:
            feet=ruler.location.z-obj.location.z+ruler.scale.z*min(v.co.z for v in ruler.data.vertices)
            assert abs(feet-record['bounds_min_m'][2])<.001,(obj.name,'reference feet')
        if record['export']:assert digest(ROOT/record['export'])==record['sha256']
    images=[]
    for image in bpy.data.images:
        if image.source=='FILE' and image.users:
            assert image.packed_file or image.packed_files,(image.name,'unpacked external image')
            assert image.size[0]>0 and image.size[1]>0
            images.append(image.name)
    assert all(digest(ROOT/p)==h for p,h in study['protected'].items())
    revisions={}
    if study.get('revision',1)>=2:
        def face_bounds(obj,faces):
            points=[obj.data.vertices[i].co for p in faces for i in p.vertices]
            return [[min(p[i] for p in points) for i in range(3)],
                    [max(p[i] for p in points) for i in range(3)]]
        porch=bpy.data.objects['Home_C_CrossGable']
        porch_front=[p for p in porch.data.polygons if len(p.vertices)==3 and
                     all(abs(porch.data.vertices[i].co.y+6.25)<.001 for i in p.vertices)]
        assert len(porch_front)==1 and porch_front[0].material_index==1
        assert all(porch.data.color_attributes['WindowMask'].data[i].color[3]>.99 for i in porch_front[0].loop_indices)
        revisions['C1_painted_trim']=True
        shop=bpy.data.objects['Shop_A_Current']
        low,high=face_bounds(shop,list(shop.data.polygons)[12:18])
        assert abs(low[2]-8.7)<.001 and low[2]>8.6+.09
        revisions['A2_sign_gutter_clearance_m']=round(low[2]-8.6,3)
        shop=bpy.data.objects['Shop_C_SteppedFront']
        sign=[p for p in shop.data.polygons if p.material_index==3]
        low,high=face_bounds(shop,sign)
        assert abs(low[0]+high[0])<.001 and low[0]>-2.525 and high[0]<2.525
        assert low[2]>8.0 and high[2]<11.6 and high[1]<-7.025-.04
        revisions['C2_sign_fits_middle_step']=True
        awning=bpy.data.objects['Shop_D_Canopy']
        added=list(awning.data.polygons)
        # Inspect the unique new roof slope, excluding the actual flat roof.
        slope=[p for p in added if p.material_index==2 and len(p.vertices)==4 and
               min(awning.data.vertices[i].co.z for i in p.vertices)>4.7 and
               max(awning.data.vertices[i].co.z for i in p.vertices)<5.6]
        assert len(slope)==1
        low,high=face_bounds(awning,slope)
        assert low[0]>-7.5 and high[0]<7.5 and high[1]<-7.0
        revisions['D2_awning_front_only']=True
        for x in (-7.25,7.25):
            pillar_faces=[p for p in added if p.material_index==0 and all(
                abs(awning.data.vertices[i].co.x-x)<=.116 and
                abs(awning.data.vertices[i].co.y+8.3)<=.116 for i in p.vertices)]
            plow,phigh=face_bounds(awning,pillar_faces)
            assert abs(phigh[2]-4.72)<.001 and phigh[2]>4.64
            assert plow[1]<-8.315<phigh[1]
        revisions['D2_posts_seat_front_beam']=True
        for record,count in zip([r for r in study['assets'] if r['category']=='Factory' and r['variant']!='A current'],(1,2,3)):
            pipes=record['roof_penetrations']
            assert len(pipes)==count
            for pipe in pipes:
                cx,cy=pipe['center_m'];margin=pipe['radius_m']+.32
                if pipe['roof']=='sawtooth-front':
                    assert -7.5<cy-margin<cy+margin<-2.5
                    surface=lambda x,y:7+(y+7.5)/5*1.65
                elif pipe['roof'].startswith('twin-gable'):
                    assert -7.5<cy-margin<cy+margin<7.5
                    assert abs(cx)%4==0 and cx-margin>-8 and cx+margin<8
                    surface=lambda x,y,c=cx:9.25-abs(x-c)*2.25/4
                else:
                    assert cx-margin>3.0 and cx+margin<8 and -7<cy-margin<cy+margin<7
                    surface=lambda x,y:7.6
                assert abs(pipe['roof_z_m']-surface(cx,cy))<1e-6
                assert all(abs(z-surface(x,y)-.035)<1e-6 for x,y,z in pipe['flashing_corners'])
                assert pipe['top_m']>pipe['roof_z_m']+2 and pipe['top_m']<17
            revisions[record['name']]=dict(pipe_count=count,roof_flashing_fits=True)
    if study.get('revision',1)>=3:
        entrances={}
        for record in study['assets']:
            entry=record.get('entrance')
            if record['category']=='Home':
                assert entry is None
                continue
            obj=bpy.data.objects[record['name']]
            if record['category']=='Shop':
                assert entry['door_leaf_count']==2 and 2.20<entry['personnel_clear_width_m']<2.50
                assert 2.2<=entry['door_height_m']<=2.5 and entry['door_front_y_m']<-7.05
                assert all(obj.data.polygons[i].material_index!=0 for i in entry['faces'])
                assert sum(all(obj.data.color_attributes['WindowMask'].data[li].color[3]<.01
                               for li in obj.data.polygons[i].loop_indices) for i in entry['faces'])>=2
            else:
                person=entry['personnel'];freight=entry['freight']
                assert 1.0<=person['width_m']<=1.2 and 1.9<=person['height_m']<=2.2
                for bay in freight:
                    assert bay['height_m']>=3.5 and bay['center_x_m']+bay['width_m']/2+.18<person['center_x_m']-person['width_m']/2-.11
                    assert bay['floor_m']+bay['height_m']+.18<5.2,'Bay must clear strip windows'
                assert abs(person['path_bounds_m'][0][1]+10.4)<.001
                if entry['dock']:
                    dock=entry['dock']
                    assert all(abs(bay['floor_m']-dock['platform_top_m'])<1e-6 for bay in freight)
                    assert abs(dock['step_count']*dock['step_rise_m']-dock['platform_top_m'])<1e-6
                    assert dock['platform_bounds_m'][1][0]<person['path_bounds_m'][0][0]
            entrances[record['name']]=dict(style=entry['style'],body_scale_retained=True,entry_clearance_pass=True)
        assert len(entrances)==8
        assert len({r['entrance']['style'] for r in study['assets'] if r['category']=='Shop'})==4
        if not study['exports_authored']:assert not any(r['export'] for r in study['assets'])
        revisions['entrances']=entrances
        revisions['blender_only_no_exports']=not study['exports_authored']
    report=dict(task='D103',source_sha256=digest(source),objects=objects,packed_images=images,
                verified_body_scale=True,verified_reference_height=True,verified_uv_color_layers=True,
                protected_unchanged=True,geometry_pass=True,adjustment_checks=revisions)
    path=out/('FirstSourceGeometry.json' if args.first else 'SourceVerification.json')
    path.write_text(json.dumps(report,indent=2)+'\n')
    print('CITYSIM_D103_SOURCE_PASS',len(objects),'saved meshes;',len(images),'packed images; production inputs unchanged')


if __name__=='__main__':main()
