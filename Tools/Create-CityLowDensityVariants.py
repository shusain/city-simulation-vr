"""D103 editable low-density alternatives at accepted scale; no promotion.

Run in Blender. Source objects, production exports/manifests and native content
are read-only inputs. New geometry retains the accepted semantic materials,
WindowMask and UV conventions. Study FBXs use the shared readback transaction.
"""
from pathlib import Path
import argparse, json, math, sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityBuildingStudyIO import load_object, checked_fbx, digest, window_faces
from CityParkGeometry import bounds
from CityBuildingEntrances import store_entrance,industrial_entrances

OUT = ROOT/'Artifacts/D103'
SOURCE = ROOT/'ArtSource/Blender/Studies/CityLowDensityVariants.blend'
EXPORT = OUT/'Exports'
IDS = ('building.residential.a', 'building.shop.a', 'building.factory.a')


class Builder:
    def __init__(self, source):
        self.source = source
        self.vertices, self.faces, self.slots, self.colors, self.uvs = [], [], [], [], []

    def face(self, points, slot, color=(1, 1, 1, 1), uv=None):
        start = len(self.vertices)
        self.vertices.extend(tuple(p) for p in points)
        self.faces.append(tuple(range(start, start+len(points))))
        self.slots.append(slot)
        self.colors.append([color]*len(points) if len(color)==4 and isinstance(color[0], (int,float)) else color)
        if uv is None:
            normal = (Vector(points[1])-Vector(points[0])).cross(Vector(points[2])-Vector(points[0])).normalized()
            axis = max(range(3), key=lambda i: abs(normal[i]))
            sign = 1 if normal[axis]>0 else -1
            repeat = 2.4 if slot==0 else 4.8
            uv = [((sign*p[1],p[2]) if axis==0 else (-sign*p[0],p[2]) if axis==1 else (p[0],p[1])) for p in points]
            uv = [(a/repeat,b/repeat) for a,b in uv]
        self.uvs.append(uv)

    def copy(self, indices, transform=lambda p:p, tint=None):
        mesh = self.source.data
        attr = mesh.color_attributes['WindowMask']
        uv = mesh.uv_layers.active
        for i in indices:
            p = mesh.polygons[i]
            self.face([transform(mesh.vertices[v].co.copy()) for v in p.vertices], p.material_index,
                      [tint or tuple(attr.data[li].color) for li in p.loop_indices],
                      [tuple(uv.data[li].uv) for li in p.loop_indices])

    def box(self, center, size, slot=1, color=(1,1,1,1)):
        x,y,z = center
        a,b,c = [v/2 for v in size]
        p = [(x-a,y-b,z-c),(x+a,y-b,z-c),(x+a,y+b,z-c),(x-a,y+b,z-c),
             (x-a,y-b,z+c),(x+a,y-b,z+c),(x+a,y+b,z+c),(x-a,y+b,z+c)]
        for f in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)):
            self.face([p[i] for i in f],slot,color)

    def tube(self,x,y,bottom,top,radius,slot=2):
        """Open steel flue with wall thickness and an annular rim."""
        rings=[]
        for z,r in ((bottom,radius),(top,radius),(top,radius*.80),(bottom,radius*.80)):
            rings.append([(x+r*math.cos(i*math.tau/12),y+r*math.sin(i*math.tau/12),z) for i in range(12)])
        for a,b in ((0,1),(1,2),(2,3),(3,0)):
            for i in range(12):
                j=(i+1)%12
                self.face([rings[a][i],rings[a][j],rings[b][j],rings[b][i]],slot)

    def roof_stack(self,x,y,radius,top,surface,style):
        """A seated curb and roof-following flashing around a through-roof flue."""
        outer=radius+.32
        inner=radius+.10
        z=surface(x,y)
        # Four flashing strips follow the actual roof (including a ridge).
        flashing=[]
        for x0,x1,y0,y1 in ((-outer,outer,-outer,-inner),(-outer,outer,inner,outer),
                             (-outer,-inner,-inner,inner),(inner,outer,-inner,inner)):
            xs=sorted({x0,x1,0} if x0<0<x1 else {x0,x1})
            for a,b in zip(xs,xs[1:]):
                points=[(x+a,y+y0,surface(x+a,y+y0)+.035),(x+b,y+y0,surface(x+b,y+y0)+.035),
                        (x+b,y+y1,surface(x+b,y+y1)+.035),(x+a,y+y1,surface(x+a,y+y1)+.035)]
                self.face(points,2)
                flashing.extend(points)
        high=max(surface(x+dx,y+dy) for dx in (-inner,0,inner) for dy in (-inner,0,inner))
        low=min(surface(x+dx,y+dy) for dx in (-inner,inner) for dy in (-inner,inner))-.20
        self.box((x,y,(low+high+.32)/2),(inner*2,inner*2,high+.32-low),0 if style=='boiler' else 2)
        self.tube(x,y,high+.25,top,radius)
        if style=='capped':
            for dx in (-radius*.60,radius*.60):
                self.box((x+dx,y,top+.13),(.07,.07,.30),2)
            self.box((x,y,top+.30),(radius*2.6,radius*2.6,.12),2)
        return dict(center_m=[x,y],radius_m=radius,roof_z_m=z,top_m=top,style=style,
                    flashing_corners=flashing,roof_panel_clearance_m=.25)

    def gable(self, x0,x1,y0,y1,eaves,ridge,slot,trim=1,across=False,trim_color=(1,1,1,1)):
        p = [(x0,y0,eaves),(x1,y0,eaves),(x1,y1,eaves),(x0,y1,eaves),
             ((x0+x1)/2,y0,ridge),((x0+x1)/2,y1,ridge)]
        if across:
            p = [(y,x,z) for x,y,z in p]
        for f, material in (((0,3,2,1),trim),((0,1,4),trim),((3,5,2),trim),((0,4,5,3),slot),((4,1,2,5),slot)):
            self.face([p[i] for i in f],material,trim_color if material==trim else (1,1,1,1))

    def hip(self, x0,x1,y0,y1,eaves,ridge,slot,trim=1):
        # Ridge follows the long axis; short canopies must not fold over.
        width,depth=x1-x0,y1-y0
        if width>depth:
            self.hip(y0,y1,x0,x1,eaves,ridge,slot,trim)
            # Transpose just the five new faces and restore outward winding.
            for face in self.faces[-5:]:
                for i in face:
                    x,y,z=self.vertices[i]
                    self.vertices[i]=(y,x,z)
            self.faces[-5:]=[tuple(reversed(f)) for f in self.faces[-5:]]
            # Recompute roof UVs in the final world axes.
            for j in range(len(self.faces)-5,len(self.faces)):
                self.uvs[j]=[(self.vertices[i][0]/4.8,self.vertices[i][1]/4.8) for i in self.faces[j]]
            return
        inset = width/2
        p = [(x0,y0,eaves),(x1,y0,eaves),(x1,y1,eaves),(x0,y1,eaves),
             ((x0+x1)/2,y0+inset*.65,ridge),((x0+x1)/2,y1-inset*.65,ridge)]
        for f,s in (((0,3,2,1),trim),((0,1,4),slot),((3,5,2),slot),((0,4,5,3),slot),((4,1,2,5),slot)):
            self.face([p[i] for i in f],s)

    def make(self, name):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        for m in self.source.data.materials: mesh.materials.append(m)
        color = mesh.color_attributes.new(name='WindowMask', type='BYTE_COLOR', domain='CORNER')
        uv = mesh.uv_layers.new(name='UVMap')
        color = mesh.color_attributes['WindowMask']
        uv = mesh.uv_layers['UVMap']
        for p,slot,colors,uvs in zip(mesh.polygons,self.slots,self.colors,self.uvs):
            p.material_index = slot
            for li,c,t in zip(p.loop_indices,colors,uvs):
                color.data[li].color = c
                uv.data[li].uv = t
        obj = bpy.data.objects.new(name,mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def home(source, kind):
    b = Builder(source)
    # The exact accepted wall, door, pane/frame and ground-walk dimensions remain.
    keep = [i for i in range(len(source.data.polygons)) if i not in set(range(6,17))|set(range(40,46))|set(range(287,333))]
    b.copy(keep)
    pale = (.76,.73,.64,1)
    roof = 2
    if kind==0:
        b.hip(-5.28,5.28,-5.28,5.28,3,4.6,roof)
        b.hip(-2.0,2.0,-6.35,-4.25,2.35,2.95,roof)
        for x in (-1.72,1.72):
            b.box((x,-6.02,1.14),(.16,.16,2.28),3,pale)
        b.box((0,-5.35,-.10),(4.05,1.6,.20),1,(.48,.46,.40,1))
        label = 'Home_B_HipPorch'
    elif kind==1:
        b.gable(-5.28,5.28,-5.28,5.28,3,4.6,roof)
        # A broad entrance gable is seated into the main roof slope.
        b.gable(-2.02,2.02,-6.25,-1.20,2.6,4.30,roof,trim=1,trim_color=pale)
        for x in (-1.75,1.75):
            b.box((x,-5.98,1.23),(.18,.18,2.46),3,pale)
        b.box((0,-5.32,-.10),(4.05,1.42,.20),1,(.48,.46,.40,1))
        label = 'Home_C_CrossGable'
    else:
        b.gable(-5.28,5.28,-5.28,5.28,3,4.6,roof,across=True)
        # Full-width shallow veranda; roof remains beneath the existing eaves.
        b.face([(-4.72,-4.30,2.72),(4.72,-4.30,2.72),(4.72,-6.3,2.38),(-4.72,-6.3,2.38)],roof)
        b.box((0,-6.23,2.30),(9.50,.14,.18),3,pale)
        for x in (-4.4,-1.7,1.7,4.4):
            b.box((x,-6.22,1.09),(.15,.15,2.18),3,pale)
        b.box((0,-5.43,-.10),(9.35,1.72,.20),1,(.48,.46,.40,1))
        label = 'Home_D_Veranda'
    return b.make(label)


def shop(source, kind):
    b = Builder(source)
    removed = set(range(6,18))|set(range(41,59))
    b.copy([i for i in range(len(source.data.polygons)) if i not in removed])
    # Keep the sign artwork/UVs; C2 fits it onto the central stepped panel.
    sign_x = (-4.8,4.8,0)[kind]
    if kind==1:
        b.copy(range(12,18), lambda p:Vector((p.x*.64,-7.18+(p.y+6.5)*.18,9.82+(p.z-10)*.64)))
    else:
        b.copy(range(12,18), lambda p:p+Vector((sign_x,0,0)))
    if kind==0:
        # Three separately framed awnings give a neighborhood market rhythm.
        for x in (-4.6,0,4.6):
            b.box((x,-7.55,4.85),(3.8,1.75,.30))
            b.box((x,-8.38,4.63),(3.8,.10,.20))
        for x in (-7.15,-2.0,2.0,7.15):
            b.box((x,-7.10,3.90),(.28,.32,7.8),0)
        b.box((0,0,8.12),(15.4,14.4,.24),2)
        b.box((3,3,8.72),(3,3,1.2))
        label = 'Shop_B_MarketAwnings'
    elif kind==1:
        b.box((0,-7.55,5.0),(15.4,1.75,.45))
        b.box((0,-7.02,7.50),(15.25,.20,.30))
        # Stepped parapet, contained within the existing sign height.
        for x in (-5,0,5):
            b.box((x,-6.75,8.6 if x else 9.8),(5.05,.55,1.2 if x else 3.6),0)
        b.box((0,6.75,8.40),(15.4,.55,.8))
        b.box((-4,3,8.60),(3,3,1.2))
        label = 'Shop_C_SteppedFront'
    else:
        # A front-only sloping awning with closed end caps: nothing wraps
        # through the side walls/windows, and the rear edge seats on -Y.
        b.face([(-7.4,-7.06,5.55),(-7.4,-8.38,4.78),(7.4,-8.38,4.78),(7.4,-7.06,5.55)],2)
        b.box((0,-8.38,4.71),(14.8,.13,.14),1)
        b.box((0,-7.065,5.56),(14.8,.07,.08),1)
        for x in (-7.4,7.4):
            points=[(x,-7.06,5.55),(x,-8.38,4.78),(x,-7.06,4.78)]
            if x<0:points.reverse()
            b.face(points,2)
        b.box((0,0,8.08),(15.4,14.4,.16),2)
        for x in (-7.25,7.25):
            b.box((x,-8.30,2.36),(.23,.23,4.72),0)
        b.box((-4,3,8.60),(3,3,1.2))
        label = 'Shop_D_Canopy'
    return b.make(label)


def factory(source, kind):
    b = Builder(source)
    removed = set(range(6,12))|set(range(18,46))|set(range(70,88))
    # Loading bay, strip windows and path stay; each roof gets its own
    # exhaust layout with roof-following flashing, not copied stack positions.
    b.copy([i for i in range(len(source.data.polygons)) if i not in removed])
    penetrations=[]
    if kind==0:
        # Three north-light sawtooth roof bands; a glazed face on each band.
        for y0,y1 in ((-7.5,-2.5),(-2.5,2.5),(2.5,7.5)):
            b.face([(-8,y0,7),(8,y0,7),(8,y1,8.65),(-8,y1,8.65)],3)
            b.face([(-8,y1,8.65),(8,y1,8.65),(8,y1,7),(-8,y1,7)],3)
            for x in (-8,8):
                points=[(x,y0,7),(x,y1,7),(x,y1,8.65)]
                if x<0: points.reverse()
                b.face(points,0)
            for x in (-5,0,5):
                pane=[(x-1.7,y1+.025,7.22),(x+1.7,y1+.025,7.22),(x+1.7,y1+.025,8.35),(x-1.7,y1+.025,8.35)]
                b.face(list(reversed(pane)),4,(1,1,1,0),[(0,1),(1,1),(1,0),(0,0)])
                for z in (7.22,8.35): b.box((x,y1+.05,z),(3.5,.10,.10),4)
                for edge in (x-1.7,x+1.7): b.box((edge,y1+.05,7.785),(.10,.10,1.23),4)
        label = 'Factory_B_Sawtooth'
        surface=lambda x,y:7+(y+7.5)/5*1.65
        penetrations.append(b.roof_stack(-5.0,-4.3,.86,16.1,surface,'boiler'))
        penetrations[-1]['roof']='sawtooth-front'
    elif kind==1:
        for x0,x1 in ((-8,0),(0,8)):
            b.gable(x0,x1,-7.5,7.5,7,9.25,3,trim=0)
        b.box((0,-7.66,5.0),(6.4,.8,.25),3)
        label = 'Factory_C_TwinGable'
        for x,y,top in ((-4,1.5,14.4),(4,3.5,12.6)):
            surface=lambda px,py,c=x:9.25-abs(px-c)*2.25/4
            penetrations.append(b.roof_stack(x,y,.60,top,surface,'capped'))
            penetrations[-1]['roof']='twin-gable-left' if x<0 else 'twin-gable-right'
    else:
        b.box((0,0,7.30),(16,15,.60),3)
        b.box((0,0,8.08),(5.5,14.0,1.55),0)
        b.gable(-3.0,3.0,-7.2,7.2,8.86,9.60,3,trim=0)
        # Clone accepted pane+frame groups into the clerestory sides.
        for x in (-2.78,2.78):
            for y in (-4.6,0,4.6):
                center = Vector((7.535,-4.6,5.8))
                target = Vector((x,y,8.10))
                def move(p, center=center,target=target,x=x):
                    d=p-center
                    return target+Vector((d.x*(1 if x>0 else -1),d.y,d.z*.72))
                b.copy([61]+list(range(100+3*16,100+4*16)),move)
        label = 'Factory_D_Clerestory'
        for y,r,top in ((-3.4,.38,11.3),(0,.46,12.5),(3.4,.38,10.8)):
            penetrations.append(b.roof_stack(5.25,y,r,top,lambda x,y:7.6,'plain'))
            penetrations[-1]['roof']='clerestory-flat-right'
    obj=b.make(label)
    obj['D103_roof_penetrations']=json.dumps(penetrations)
    return obj


def face_key(mesh,polygon):
    return tuple(sorted(tuple(round(c,5) for c in mesh.vertices[i].co) for i in polygon.vertices))


def replace_entrances(obj,category,variant,banned):
    b=Builder(obj)
    keep=[p.index for p in obj.data.polygons if face_key(obj.data,p) not in banned]
    removed=len(obj.data.polygons)-len(keep)
    assert removed==(6 if category=='Shop' else 30),(obj.name,'old entrance scope',removed)
    b.copy(keep)
    entrance=store_entrance(b,variant) if category=='Shop' else industrial_entrances(b,variant)
    old_name=obj.name
    result=b.make(old_name+'_EntranceStudy')
    for key in obj.keys():result[key]=obj[key]
    result['D103_entrance']=json.dumps(entrance)
    bpy.data.objects.remove(obj,do_unlink=True)
    result.name=old_name
    return result


def main():
    global OUT,EXPORT,SOURCE
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory',type=Path,default=OUT)
    parser.add_argument('--source-file',type=Path,default=SOURCE)
    parser.add_argument('--skip-exports',action='store_true',help='Blender-only appearance review; author no FBXs')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    OUT=args.output_directory.absolute();EXPORT=OUT/'Exports';SOURCE=args.source_file.absolute()
    for p in (OUT,SOURCE.parent): p.mkdir(parents=True,exist_ok=True)
    if not args.skip_exports:EXPORT.mkdir(parents=True,exist_ok=True)
    manifest_path = ROOT/'ArtSource/Manifests/city_starter_kit.json'
    manifest = json.loads(manifest_path.read_text())
    entries = [next(e for e in manifest['assets'] if e['asset_id']==identity) for identity in IDS]
    protect = {manifest_path,ROOT/'ArtSource/Blender/CityResidentialScale.blend'}
    for e in entries:
        protect.update((ROOT/e['source'],ROOT/e['export'],(ROOT/'CitySimVR/Content'/e['mesh_destination'].removeprefix('/Game/')).with_suffix('.uasset')))
    protected = {p:digest(p) for p in protect if p.is_file()}
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene=bpy.context.scene
    scene.name='D103_LowDensityReview'
    scene.unit_settings.system='METRIC'
    scene.unit_settings.scale_length=1
    records=[]
    for row,(entry,recipe,category) in enumerate(zip(entries,(home,shop,factory),('Home','Shop','Factory'))):
        baseline=load_object(ROOT/entry['source'],entry['mesh_destination'].rsplit('/',1)[1])
        baseline.name=category+'_A_Current'
        slots=list(entry['material_slots'])
        assert [m.name.split('.')[0] for m in baseline.data.materials]==slots
        # Certain old default materials have a UI swatch but untouched node color.
        # Match the established study loader for actual Blender surface previews.
        for m in baseline.data.materials:
            if m.use_nodes and len(m.node_tree.nodes)==2:
                m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=m.diffuse_color
        candidates=[baseline]+[recipe(baseline,i) for i in range(3)]
        if category=='Shop':
            # A2 is a proposed correction to the current shop, in this study.
            sign_vertices={v for p in list(baseline.data.polygons)[12:18] for v in p.vertices}
            for i in sign_vertices:baseline.data.vertices[i].co.z+=.70
            baseline.data.update()
            baseline['D103_adjustment']='A2 sign raised .70m above roof gutter; production input preserved'
        if category!='Home':
            indices=range(24,30) if category=='Shop' else list(range(12,18))+list(range(46,58))+list(range(88,100))
            banned={face_key(baseline.data,baseline.data.polygons[i]) for i in indices}
            candidates=[replace_entrances(obj,category,col,banned) for col,obj in enumerate(candidates)]
            baseline=candidates[0]
        for col,obj in enumerate(candidates):
            obj.location=(0,0,0)
            bpy.context.view_layer.update()
            lo,hi=bounds(obj)
            obj.data.calc_loop_triangles()
            assert all(p.area>1e-7 for p in obj.data.polygons),(obj.name,'zero-area face')
            assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
            assert lo[2]>=-.201 and hi[2]<=entry['dimensions_m'][2]+(.701 if category=='Shop' and col==0 else .001)
            assert all(abs(s-1)<1e-7 for s in obj.scale),'Scale must be baked into mesh'
            # Recipes copy the six body faces first: exact accepted body scale.
            for p,q in zip(list(obj.data.polygons)[:6],list(baseline.data.polygons)[:6]):
                assert len(p.vertices)==len(q.vertices)
                assert all((obj.data.vertices[i].co-baseline.data.vertices[j].co).length<1e-6
                           for i,j in zip(p.vertices,q.vertices)),(obj.name,'body scale changed')
            path=EXPORT/(obj.name+'.fbx')
            if not args.skip_exports:
                with checked_fbx(obj,path,slots,verify_alpha=True) as readback:
                    assert window_faces(obj,slots[-1:])==window_faces(readback,slots[-1:])
                    rlo,rhi=bounds(readback)
                    assert all(abs(a-b)<.002 for a,b in zip(lo+hi,rlo+rhi))
            obj['D103_category']=category
            obj['D103_variant']=('A current','B','C','D')[col]
            obj['D103_front']='-Y'
            obj['D103_logical_lot_m']='16 x 16; existing 2 x 2 cells'
            obj['D103_review_only']=True
            records.append(dict(name=obj.name,category=category,variant=('A current','B','C','D')[col],
                                reference=entry['asset_id'],source_entry=entry if col==0 else None,
                                dimensions_m=list(obj.dimensions),bounds_min_m=lo,bounds_max_m=hi,
                                triangles=len(obj.data.loop_triangles),sections=len(slots),slots=slots,
                                windows=window_faces(obj,slots[-1:]),export=path.relative_to(ROOT).as_posix() if not args.skip_exports else None,
                                sha256=digest(path) if not args.skip_exports else None,roundtrip_pass=not args.skip_exports,
                                adjustment=obj.get('D103_adjustment'),
                                entrance=json.loads(obj.get('D103_entrance','null')),
                                roof_penetrations=json.loads(obj.get('D103_roof_penetrations','[]'))))
            obj.location=(col*26,-row*32,0)
    # One independently selectable citizen ruler beside each gallery object.
    citizen=load_object(ROOT/'ArtSource/Blender/CityResidentialScale.blend','ScaleReference_Street')
    citizen.name='D103_CitizenRuler'
    citizen.scale=(1.15,)*3
    citizen.rotation_euler.z=math.pi/2
    min_z=min(v.co.z for v in citizen.data.vertices)
    citizen.location=(-4,-6,-.20-min_z*1.15)
    citizen['height_m']=1.43175
    for index,record in enumerate(records):
        obj=bpy.data.objects[record['name']]
        ruler=citizen if index==0 else citizen.copy()
        if index: scene.collection.objects.link(ruler)
        ruler.name='Ruler_'+record['name']
        ruler.location=obj.location+Vector((-4,record['bounds_min_m'][1]-.6,record['bounds_min_m'][2]-min_z*1.15))
    scene.world.color=(.10,.13,.18)
    scene.render.engine='CYCLES'
    scene.cycles.samples=24
    for area in bpy.context.screen.areas if bpy.context.screen else []:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_distance=115
            area.spaces.active.region_3d.view_location=(39,-30,2)
            area.spaces.active.shading.type='MATERIAL'
    bpy.data.orphans_purge(do_recursive=True)
    bpy.ops.file.pack_all()
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
    assert all(digest(p)==h for p,h in protected.items()),'Read-only production input changed'
    report=dict(task='D103',revision=3,exports_authored=not args.skip_exports,review_only=True,production_imported=False,source=SOURCE.relative_to(ROOT).as_posix(),
                source_sha256=digest(SOURCE),assets=records,citizen_height_m=1.43175,
                scale_contract='Accepted body/window scale, roof and entrance direction retained; revised store/personnel/freight doors at citizen scale. All use existing 16m logical lot.',
                protected={p.relative_to(ROOT).as_posix():h for p,h in protected.items()})
    (OUT/'Study.json').write_text(json.dumps(report,indent=2)+'\n')
    print('CITYSIM_D103_VARIANTS_PASS 9 variants; 3 references;', 'Blender only / no FBX exports' if args.skip_exports else '12 FBX roundtrips')


if __name__=='__main__': main()
