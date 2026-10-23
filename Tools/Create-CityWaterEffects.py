from pathlib import Path
import unreal, json, time, traceback, hashlib
O=Path(unreal.Paths.project_dir()).resolve().parent;L=unreal.MaterialEditingLibrary;A=unreal.AssetToolsHelpers.get_asset_tools()
def create(name,cls,factory):
    p='/Game/CitySim/Art/Water/'+name
    return unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else A.create_asset(name,'/Game/CitySim/Art/Water',cls,factory)
def node(m,cls,**props):
    n=L.create_material_expression(m,getattr(unreal,'MaterialExpression'+cls))
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def link(a,b,p='',out=''):assert L.connect_material_expressions(a,out,b,p),(a.get_name(),b.get_name(),p)
def c(m,v):return node(m,'Constant',r=v)
def v(m,x,y,z):return node(m,'Constant3Vector',constant=unreal.LinearColor(x,y,z,1))
def par(m,name,val):return node(m,'ScalarParameter',parameter_name=name,default_value=val)
def op(m,cls,a,b):
    n=node(m,cls);link(a,n,'A');link(b,n,'B');return n
def add(m,a,b):return op(m,'Add',a,b)
def sub(m,a,b):return op(m,'Subtract',a,b)
def mul(m,a,b):return op(m,'Multiply',a,b)
def unary(m,cls,a):
    n=node(m,cls);link(a,n);return n
def sat(m,a):return unary(m,'Saturate',a)
def mask(m,a,ch):
    n=node(m,'ComponentMask',r=ch==0,g=ch==1,b=ch==2,a=ch==3);link(a,n);return n
def blend(m,a,b,alpha):
    n=node(m,'LinearInterpolate');link(a,n,'A');link(b,n,'B');link(alpha,n,'Alpha');return n
def prop(a,p):
    if p=='WORLD_POSITION_OFFSET':
        m=a.get_outer()
        axes=[node(m,'VectorParameter',parameter_name='EffectAxis'+name,default_value=unreal.LinearColor(*value,0)) for name,value in [('X',(1,0,0)),('Y',(0,1,0)),('Z',(0,0,1))]]
        a=add(m,add(m,mul(m,mask(m,a,0),axes[0]),mul(m,mask(m,a,1),axes[1])),mul(m,mask(m,a,2),axes[2]))
    assert L.connect_material_property(a,'',getattr(unreal.MaterialProperty,'MP_'+p))
def save(a):assert unreal.EditorAssetLibrary.save_loaded_asset(a)
def clock(m):return add(m,mul(m,node(m,'Time'),par(m,'UseRealTime',0)),par(m,'PreviewTime',0))
def distance(m):return sub(m,mul(m,mask(m,node(m,'TextureCoordinate',coordinate_index=1),0),c(m,8)),c(m,4))
def arrival(m,t):
    along=mask(m,node(m,'TextureCoordinate',coordinate_index=1),1)
    variation=add(m,mul(m,unary(m,'Sine',mul(m,along,c(m,7))),c(m,.07)),mul(m,unary(m,'Sine',mul(m,along,c(m,19))),c(m,.035)))
    return sub(m,op(m,'Divide',t,par(m,'WavePeriodSeconds',6)),variation)
def smooth(m,value,start,end):
    x=sat(m,mul(m,sub(m,value,c(m,start)),c(m,1/(end-start))))
    return mul(m,mul(m,x,x),sub(m,c(m,3),mul(m,x,c(m,2))))
def offshore_blend(m,d):
    offshore=mul(m,d,c(m,-1))
    depth=mul(m,mask(m,node(m,'TextureCoordinate',coordinate_index=2),0),c(m,8))
    # Either distance or depth can take the surface offshore; both fades have
    # zero slope at their endpoints. No boundary/opacity/mesh swap is involved.
    coastal=mul(m,sub(m,c(m,1),smooth(m,offshore,3,12)),sub(m,c(m,1),smooth(m,depth,.8,2.2)))
    return sub(m,c(m,1),coastal)
def open_wave(m,t):
    uv=node(m,'TextureCoordinate',coordinate_index=0);x=mask(m,uv,0);y=mask(m,uv,1)
    cycle=op(m,'Divide',t,par(m,'WavePeriodSeconds',6))
    p1=sub(m,add(m,mul(m,x,c(m,.42)),mul(m,y,c(m,.18))),cycle)
    p2=add(m,sub(m,add(m,mul(m,x,c(m,-.17)),mul(m,y,c(m,.31))),mul(m,cycle,c(m,2))),c(m,.17))
    return add(m,mul(m,unary(m,'Sine',p1),c(m,.65)),mul(m,unary(m,'Sine',p2),c(m,.35)))
def wave(m,d,t):
    phase=add(m,sub(m,mul(m,d,c(m,.2)),arrival(m,t)),c(m,.25))
    s=unary(m,'Sine',phase)
    taper=sat(m,mul(m,d,c(m,-.5)))
    coast_height=mul(m,mul(m,s,taper),par(m,'WaveHeightCm',14))
    open_height=mul(m,open_wave(m,t),par(m,'OpenWaveHeightCm',8))
    # Keep the displaced surface above the actual bed, including very shallow
    # river corners. The 25% depth margin survives triangle interpolation.
    height=blend(m,coast_height,open_height,offshore_blend(m,d))
    bed_limit=mul(m,mask(m,node(m,'TextureCoordinate',coordinate_index=2),0),c(m,-600))
    return s,op(m,'Max',height,bed_limit)
def build(kind):
    m=create('M_'+kind,unreal.Material,unreal.MaterialFactoryNew());L.delete_all_material_expressions(m)
    for k,value in dict(shading_model=unreal.MaterialShadingModel.MSM_UNLIT,blend_mode=unreal.BlendMode.BLEND_MASKED if kind=='Splash' else unreal.BlendMode.BLEND_TRANSLUCENT,two_sided=True,disable_depth_test=False).items():m.set_editor_property(k,value)
    t=clock(m);white=v(m,.85,.94,1.)
    if kind=='Splash':
        uv=node(m,'TextureCoordinate');birth=mask(m,uv,0)
        phase=unary(m,'Frac',sub(m,arrival(m,t),birth))
        life=sat(m,mul(m,phase,c(m,1/.16)))
        visibility=sat(m,mul(m,sub(m,c(m,.16),phase),c(m,100)))
        # UV V flips on FBX import; density seeds are symmetric in distribution.
        density=sat(m,mul(m,sub(m,par(m,'SplashDensity',.45),mask(m,uv,1)),c(m,100)))
        prop(mul(m,visibility,density),'OPACITY_MASK');prop(white,'EMISSIVE_COLOR')
        vc=node(m,'VertexColor');xy=sub(m,vc,v(m,.5,.5,0))
        travel=mul(m,mul(m,xy,v(m,1,1,0)),mul(m,life,par(m,'SplashTravelCm',70)))
        arc=mul(m,mul(m,life,sub(m,c(m,1),life)),c(m,4))
        up=mul(m,v(m,0,0,1),mul(m,mul(m,arc,mask(m,vc,2)),par(m,'SplashHeightCm',45)))
        prop(add(m,travel,up),'WORLD_POSITION_OFFSET')
    else:
        d=distance(m);s,h=wave(m,d,t);prop(mul(m,v(m,0,0,1),h),'WORLD_POSITION_OFFSET')
        if kind=='Mask':
            prop(blend(m,v(m,.8,.28,.035),v(m,.025,.22,.75),offshore_blend(m,d)),'EMISSIVE_COLOR');prop(c(m,1),'OPACITY')
        elif kind=='Water':
            crest=sat(m,mul(m,sub(m,s,c(m,.78)),c(m,4.5)))
            crest=mul(m,crest,mul(m,mul(m,sat(m,add(m,c(m,1),mul(m,d,c(m,.10)))),par(m,'CrestWhite',.24)),sub(m,c(m,1),offshore_blend(m,d))))
            coast_colour=add(m,c(m,.65),mul(m,s,c(m,.10)))
            open_colour=add(m,c(m,.65),mul(m,open_wave(m,t),c(m,.065)))
            water=mul(m,node(m,'VertexColor'),blend(m,coast_colour,open_colour,offshore_blend(m,d)))
            prop(blend(m,water,white,crest),'EMISSIVE_COLOR');prop(c(m,.72),'OPACITY')
        else:
            cycle=arrival(m,t);envelope=sat(m,unary(m,'Sine',cycle))
            front=add(m,c(m,-.65),mul(m,envelope,par(m,'WashReachM',1.4)))
            along=mask(m,node(m,'TextureCoordinate',coordinate_index=1),1)
            shape=add(m,mul(m,unary(m,'Sine',mul(m,along,c(m,11))),c(m,.5)),add(m,mul(m,unary(m,'Sine',add(m,mul(m,along,c(m,29)),cycle)),c(m,.3)),mul(m,unary(m,'Sine',sub(m,mul(m,along,c(m,67)),cycle)),c(m,.2))))
            front=add(m,front,mul(m,shape,c(m,.3)))
            diff=sub(m,d,front)
            edge=sat(m,sub(m,c(m,1),op(m,'Divide',unary(m,'Abs',diff),par(m,'FoamWidthM',.28))))
            patches=sat(m,unary(m,'Sine',sub(m,mul(m,d,c(m,1.7)),add(m,mul(m,along,c(m,47)),cycle))))
            trail=mul(m,sat(m,mul(m,sub(m,front,d),c(m,3))),sat(m,add(m,c(m,1),diff)))
            froth=op(m,'Max',mul(m,edge,sat(m,add(m,c(m,.4),mul(m,shape,c(m,1.6))))),mul(m,mul(m,trail,patches),c(m,.40)))
            film=mul(m,sat(m,mul(m,sub(m,front,d),c(m,4))),sat(m,mul(m,add(m,c(m,4),diff),c(m,.25))))
            opacity=op(m,'Max',mul(m,froth,c(m,.92)),mul(m,film,c(m,.36)))
            prop(blend(m,v(m,.025,.24,.32),white,sat(m,mul(m,froth,c(m,2)))),'EMISSIVE_COLOR');prop(opacity,'OPACITY')
    L.layout_material_expressions(m);L.recompile_material(m);save(m);return m

# Approved #215 R4 recipe; production adds board-space WPO axes only.
files=[]
for kind in ('Water','Foam','Splash'):
    master=build(kind)
    mi=create('MI_'+kind,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    L.set_material_instance_parent(mi,master);save(mi)
    for name in ('M_'+kind,'MI_'+kind):files.append('CitySimVR/Content/CitySim/Art/Water/'+name+'.uasset')
out=O/'Artifacts/CIAssetPreparation';out.mkdir(parents=True,exist_ok=True)
(out/'Files.txt').write_text('\n'.join(files)+'\n')
(out/'Water331.json').write_text(json.dumps(dict(source='Approved #215 R4',files=files,board_space_axes=True,controlled_clock=True),indent=2))
