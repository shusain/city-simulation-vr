"""Read-only utility source geometry inventory. Run in Blender without autoexec."""
from pathlib import Path
import bpy,json,sys
ROOT=Path(__file__).resolve().parents[3]
def main():
 records=[]
 for a in json.loads((ROOT/'ArtSource/Manifests/city_utility_preparation_inputs.json').read_bytes())['assets']:
  bpy.ops.wm.read_factory_settings(use_empty=True)
  source=ROOT/a['export_directory']
  with bpy.data.libraries.load(str(source/'asset.blend'),link=False) as (available,loaded):loaded.objects=list(available.objects)
  objs=[o for o in loaded.objects if o.type=='MESH' and not o.name.startswith('ReviewGround')]
  assert len(objs)==1,(a['issue'],[(o.name,o.type) for o in loaded.objects])
  obj=objs[0];bpy.context.scene.collection.objects.link(obj);bpy.context.view_layer.update()
  v=json.loads((source/'validation.json').read_bytes());cs=v.get('components',v.get('components_before_join'))
  obj.data.calc_loop_triangles()
  records.append(dict(issue=a['issue'],object=obj.name,vertices=len(obj.data.vertices),triangles=len(obj.data.loop_triangles),
   components=len(cs),vertex_groups=len(obj.vertex_groups),names_missing=[c['name'] for c in cs if c['name'] not in obj.vertex_groups],
   transform=[list(obj.location),list(obj.rotation_euler),list(obj.scale)],slots=[m.name for m in obj.data.materials],
   attributes=[(x.name,x.domain,x.data_type) for x in obj.data.attributes],smooth=sum(p.use_smooth for p in obj.data.polygons)))
  print('SOURCE_INVENTORY',a['issue'],records[-1]['triangles'],'components/groups',len(cs),len(obj.vertex_groups),'missing',len(records[-1]['names_missing']),flush=True)
 output=ROOT/'Artifacts/UtilitySourcePreparation';output.mkdir(parents=True,exist_ok=True)
 (output/'SourceInspection.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
