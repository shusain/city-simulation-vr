"""Carry the approved pavilion asphalt onto its existing D83 mobile LOD."""
from pathlib import Path
import bpy,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import Materials,export_checked,snapshot
from CityCivicBuildingSurfaces import select_surfaces,make_surface,apply_surfaces
doc=json.loads((ROOT/'ArtSource/Manifests/city_civic_surface_study.json').read_text())
entries=[e for e in doc['assets'] if e['source_entry'].get('native_lod_export')];assert len(entries)==4
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
materials=Materials();records=[]
for entry in entries:
    bpy.ops.object.select_all(action='DESELECT');bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['source_entry']['native_lod_export']))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    slots=materials.restore(obj,ROOT/entry['source_entry']['source']);before=snapshot(obj)
    walls,roofs,parts=select_surfaces(obj,entry['asset_id']);assert roofs
    info=doc['textures']['CivicAsphalt'];roof=materials.load(ROOT/doc['source'],'CivicAsphaltRoof')
    wall_materials={int(old):materials.load(ROOT/doc['source'],t['new_material']) for old,t in entry['tints'].items()}
    apply_surfaces(obj,walls,roofs,wall_materials,roof)
    slots += [entry['tints'][str(old)]['new_material'] for old in wall_materials]+['CivicAsphaltRoof'];after=snapshot(obj)
    assert before['vertices']==after['vertices'] and before['faces']==after['faces'] and before['rgba']==after['rgba']
    for p in obj.data.polygons:
        if p.index not in set(roofs)|set(walls):
            assert before['slots'][p.index]==after['slots'][p.index]
            for li in p.loop_indices:assert before['uv'][li]==after['uv'][li]
    name=entry['mesh_destination'].rsplit('/',1)[1]+'_LOD1';path=ROOT/'ArtSource/Exports/CivicSurfaceStudy'/(name+'.fbx')
    record=export_checked(obj,path,slots);assert record['triangles']==entry['source_entry']['native_lod_triangles']
    obj.name=name;record.update(asset_id=entry['asset_id'],export=path.relative_to(ROOT).as_posix(),source='ArtSource/Blender/CityCivicSurfaceLODs.blend',material_slots=entry['material_slots'],roof_faces=roofs,wall_faces=list(walls),geometry_rgba_preserved=True,unselected_uv_material_preserved=True);records.append(record)
    obj.location.x=len(records)*60
bpy.data.orphans_purge(do_recursive=True);bpy.ops.file.pack_all()
source=ROOT/'ArtSource/Blender/CityCivicSurfaceLODs.blend';bpy.ops.wm.save_as_mainfile(filepath=str(source))
out=ROOT/'Artifacts/D91/Integration';out.mkdir(parents=True,exist_ok=True)
(out/'LODs.json').write_text(json.dumps(records,indent=2)+'\n')
(out/'PavilionLOD.json').write_text(json.dumps(next(e for e in records if e['asset_id']=='building.park.city'),indent=2)+'\n')
print('CITYSIM_D91_BUILDING_LODS_PASS',len(records),'paired LODs;geometry/RGBA and unselected UV/materials preserved')
