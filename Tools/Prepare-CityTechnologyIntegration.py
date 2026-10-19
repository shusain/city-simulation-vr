"""Read exact approved Blender sources and record native material/UV inputs; no source edits."""
from pathlib import Path
import bpy,json,hashlib,sys,math
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=Path(sys.argv[sys.argv.index('--')+1])
names={101:('DigitalExchange',5),102:('ComputingCenter',4),103:('ResearchCampus',3),104:('AIInstitute',3),105:('EnterpriseAICenter',4)}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=[]
for issue,(name,version) in names.items():
 folder=ROOT/f'ArtSource/Generated/blender/issue-{issue}/v{version}';before=sha(folder/'asset.blend')
 bpy.ops.wm.open_mainfile(filepath=str(folder/'asset.blend'),load_ui=False)
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and len(o.data.polygons)>400]
 assert len(objects)==1,(name,[(o.name,len(o.data.polygons)) for o in objects])
 obj=objects[0];obj.data.calc_loop_triangles();materials=[]
 for material in obj.data.materials:
  nodes=[];nt=material.node_tree
  for n in nt.nodes:
   inputs={}
   for inp in n.inputs:
    value=getattr(inp,'default_value',None)
    try:value=list(value)
    except TypeError:pass
    if isinstance(value,(list,int,float,str,bool)) or value is None:inputs[inp.name]=value
   nodes.append({'name':n.name,'type':n.type,'inputs':inputs,'image':n.image.name if n.type=='TEX_IMAGE' and n.image else None})
  p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED')
  images=[{'name':n.image.name,'sha256':hashlib.sha256(bytes(n.image.packed_file.data)).hexdigest() if n.image.packed_file else None,'colorspace':n.image.colorspace_settings.name} for n in nt.nodes if n.type=='TEX_IMAGE' and n.image]
  materials.append({'slot':material.name,'diffuse':list(material.diffuse_color),'principled':{k:list(p.inputs[k].default_value) if k=='Base Color' else p.inputs[k].default_value for k in ['Base Color','Metallic','Roughness']},'nodes':nodes,'links':[[l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name] for l in nt.links],'images':images})
 uv=[]
 for layer in obj.data.uv_layers:
  finite=all(math.isfinite(x) for u in layer.data for x in u.uv);assert finite
  uv.append({'name':layer.name,'finite':finite,'loops':len(layer.data)})
 assert sha(folder/'asset.blend')==before
 rows.append({'issue':issue,'version':version,'definition':name,'blendSha256':before,'triangles':len(obj.data.loop_triangles),'materials':materials,'uvs':uv})
OUTPUT.parent.mkdir(parents=True,exist_ok=True);OUTPUT.write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('CITYSIM_TECHNOLOGY_SOURCE_READBACK '+str(len(rows)))
