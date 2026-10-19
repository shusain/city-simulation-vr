"""Shared native-only guard, scoped saves and retained receipts for #157."""
from pathlib import Path
import hashlib,json,re
import unreal
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,SCOPE,CONCRETE,sha,write,checked_path
OUT=ROOT/'Artifacts/CIAssetPreparation'
LIB=unreal.MaterialEditingLibrary
def recipe_hashes():
 names=['Tools/Assets/Unreal/CityApprovedUtilityPlan.py','Tools/Assets/Unreal/CityApprovedUtilityNative.py','Tools/Assets/Unreal/CityApprovedUtilityMaterials.py','Tools/Assets/Unreal/CityApprovedUtilityMeshes.py','Tools/Assets/Unreal/CityApprovedUtilityTemplates.py','Tools/Assets/Unreal/CityApprovedUtilitySolar.py','Tools/Assets/Unreal/CityApprovedUtilityRender.py','Tools/Import-CityApprovedUtilityMaterials.py','Tools/Import-CityApprovedUtilityMeshes.py']
 return {name:sha(ROOT/name) for name in names}
def package(path):
 assert path.startswith('/Game/') and '..' not in path
 return ROOT/('CitySimVR/Content/'+path.removeprefix('/Game/')+'.uasset')
def scoped(path):
 assert path.startswith(SCOPE+'/') and re.fullmatch(r'/Game/[A-Za-z0-9_/]+',path),path
 return path
def object_path(obj):return obj.get_path_name().split('.')[0]
def emit_files(paths):
 OUT.mkdir(parents=True,exist_ok=True);listing=OUT/'Files.txt'
 previous=listing.read_text(encoding='utf-8').splitlines() if listing.exists() else []
 added=[]
 for path in paths:
  file=package(scoped(path));assert file.is_file();added.append(file.relative_to(ROOT).as_posix())
 listing.write_text('\n'.join(dict.fromkeys(previous+added))+'\n',encoding='utf-8',newline='\n')
 return [dict(path=p,file=package(p).relative_to(ROOT).as_posix(),sha256=sha(package(p)),bytes=package(p).stat().st_size) for p in paths]
def save(obj):
 scoped(object_path(obj));assert unreal.EditorAssetLibrary.save_loaded_asset(obj);emit_files([object_path(obj)])
def create(path,kind,factory):
 scoped(path)
 obj=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
 assert obj and isinstance(obj,kind),(path,kind)
 return obj
def textures_and_guard(plan):
 assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
 assert unreal.SystemLibrary.get_engine_version().startswith('5.7.4'), 'Recipe audited against installed UE5.7.4'
 textures={};facts=[];protected={}
 for t in plan['textures']:
  expected=checked_path(t['source']);assert sha(expected)==t['sha256']
  path=t['native'];assert path.startswith('/Game/CitySim/Art/Textures/')
  file=package(path);assert file.is_file();protected[file]=sha(file)
  # UObject::SourceFileTagName() returns "AssetImportData" in UE5.7 Obj.cpp;
  # UTexture emits the FAssetImportInfo JSON under that hidden registry tag.
  tags={str(k):str(v) for k,v in unreal.EditorAssetLibrary.get_tag_values(path).items()}
  imported=json.loads(tags.get('AssetImportData','null'));assert isinstance(imported,list) and len(imported)==1,('Missing strict texture source provenance',path)
  assert imported[0]['FileMD5'].lower()==hashlib.md5(expected.read_bytes()).hexdigest(),('Shared texture import source differs',path)
  texture=unreal.load_asset(path);assert isinstance(texture,unreal.Texture2D)
  assert texture.get_editor_property('srgb')==(t['kind']=='color')
  compression={'color':unreal.TextureCompressionSettings.TC_DEFAULT,'normal':unreal.TextureCompressionSettings.TC_NORMALMAP,'orm':unreal.TextureCompressionSettings.TC_MASKS}[t['kind']]
  assert texture.get_editor_property('compression_settings')==compression
  if t['kind']=='normal':assert not texture.get_editor_property('flip_green_channel'),path
  textures[t['sha256']]=texture
  facts.append(dict(native=path,pinned_source=t['source'],source_sha256=t['sha256'],native_import_source_md5=imported[0]['FileMD5'],native_package_sha256=protected[file],srgb=t['kind']=='color',compression=str(compression)))
 concrete=unreal.load_asset(CONCRETE);assert isinstance(concrete,unreal.MaterialInterface)
 protected[package(CONCRETE)]=sha(package(CONCRETE))
 nuclear=ROOT/'CitySimVR/Content/CitySim/Art/Utilities/Nuclear'
 for p in nuclear.rglob('*.uasset'):protected[p]=sha(p)
 return textures,concrete,facts,protected
def unchanged(protected):assert all(path.is_file() and sha(path)==pin for path,pin in protected.items()),'Read-only shared/nuclear input changed'
def finish_material(material):
 LIB.layout_material_expressions(material);LIB.recompile_material(material);save(material)
 statistics=LIB.get_statistics(material)
 result={k:statistics.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
 assert result['num_pixel_shader_instructions']>0,object_path(material)
 return result
def parameter(material,kind,name,value):
 node=LIB.create_material_expression(material,kind);node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value);return node
