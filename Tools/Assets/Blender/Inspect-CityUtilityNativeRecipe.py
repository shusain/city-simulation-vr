"""Read-only source socket/material/mesh facts for the native recipe.

Writes recipe DATA only; never writes a blend, FBX, image or native asset.
Indexed sockets retain Blender's duplicate Math Value inputs without loss.
"""
from pathlib import Path
from collections import Counter
import bpy,json,hashlib,importlib.util
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('utility_prepare',ROOT/'Tools/Assets/Blender/Prepare-CityUtilityIntegration.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
def graph(material):
 result=[]
 for node in material.node_tree.nodes:
  r=dict(name=node.name,type=node.bl_idname,inputs=[dict(index=i,name=s.name,default=p.scalar(s.default_value) if hasattr(s,'default_value') else None) for i,s in enumerate(node.inputs)],outputs=[s.name for s in node.outputs])
  for field in ('operation','blend_type','use_clamp','noise_dimensions','noise_type','normalize','invert','space','uv_map','vector_type'):
   if hasattr(node,field):r[field]=getattr(node,field)
  if node.type=='TEX_IMAGE':
   assert node.image and node.image.packed_file
   r['image_sha256']=hashlib.sha256(bytes(node.image.packed_file.data)).hexdigest()
  result.append(r)
 return dict(region=material.name,nodes=result,links=[dict(from_node=l.from_node.name,from_index=list(l.from_node.outputs).index(l.from_socket),to_node=l.to_node.name,to_index=list(l.to_node.inputs).index(l.to_socket)) for l in material.node_tree.links])
def meshinfo(obj):
 obj.data.calc_loop_triangles();counts=Counter(obj.data.materials[t.material_index].name for t in obj.data.loop_triangles)
 # Selected per-material surface probes let native readback verify transform,
 # UV orientation and imported normals without enormous full-buffer manifests.
 samples={}
 for material in obj.data.materials:
  corners=[]
  for poly in obj.data.polygons:
   if obj.data.materials[poly.material_index]!=material:continue
   for vi,li in zip(poly.vertices,poly.loop_indices):
    corners.append(dict(position_m=list(obj.data.vertices[vi].co),normal=list(obj.data.corner_normals[li].vector),uv0=list(obj.data.uv_layers[0].data[li].uv)))
  if corners:samples[material.name]=[corners[i] for i in sorted(set([0,len(corners)-1]+[round(i*(len(corners)-1)/15) for i in range(16)]))]
 return dict(triangles_by_material=dict(counts),material_slots=[m.name for m in obj.data.materials],uv_channels=len(obj.data.uv_layers),uv_names=[uv.name for uv in obj.data.uv_layers],
  render_uv_channel=next(i for i,uv in enumerate(obj.data.uv_layers) if uv.active_render),bounds_m=p.measure(obj)['bounds_m'],samples=samples)
def load(path):
 with bpy.data.libraries.load(str(path),link=False) as (available,loaded):loaded.objects=list(available.objects)
 for obj in loaded.objects:
  if obj.type=='MESH':bpy.context.scene.collection.objects.link(obj)
 bpy.context.view_layer.update();return {o.name:o for o in loaded.objects if o.type=='MESH'}
def main():
 base=p.read(ROOT/'ArtSource/Manifests/city_utility_integration.json');extension=p.read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json');assets=[]
 for a in base['assets']:
  bpy.ops.wm.read_factory_settings(use_empty=True);objects=load(ROOT/a['prepared_source']);obj=objects[a['lods'][0]['object']]
  assets.append(dict(issue=a['issue'],version=a['version'],key=a['key'],graphs=[graph(m) for m in obj.data.materials],
   lod_facts=[dict(lod=lod['lod'],**meshinfo(objects[lod['object']])) for lod in a['lods']]))
 bpy.ops.wm.read_factory_settings(use_empty=True);objects=load(ROOT/extension['wind']['source'])
 wind=[dict(object=role['object'],role=role['role'],lod=role['lod'],**meshinfo(objects[role['object']])) for role in extension['wind']['roles']]
 bpy.ops.wm.read_factory_settings(use_empty=True);objects=load(ROOT/extension['foundation']['source'])
 foundations=[dict(object=g['object'],key=a['key'],**meshinfo(objects[g['object']])) for a in extension['foundation']['assets'] for g in a['groups']]
 output=ROOT/'Artifacts/UtilitySourcePreparation/NativeRecipeInspection.json'
 p.write(output,dict(blender_version=bpy.app.version_string,assets=assets,wind_roles=wind,foundations=foundations,source_meshes_or_images_written=False))
 print('UTILITY_NATIVE_RECIPE_READONLY_INSPECTION_PASS',len(assets),'assets,indexed sockets,54 LOD/source mesh facts')
if __name__=='__main__':main()
