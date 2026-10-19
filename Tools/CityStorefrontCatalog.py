"""Cooked D79 presentation references; leaves gameplay definitions unchanged."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import json,unreal
ROOT=Path(__file__).resolve().parents[1]
BUSINESSES=('market','bakery','cafe','books','pharmacy','repairs','outfitters','tech')

def configure(catalog):
    manifest=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
    if not any(has_capability(e, 'building_surfaces') for e in manifest['assets']):return []
    before=[e.export_text() for e in catalog.get_editor_property('definitions')]
    entries=[]
    report=[]
    for business in BUSINESSES:
        variant=unreal.CityCommercialPresentationVariant()
        variant.set_editor_property('business_id',business)
        row=dict(business=business)
        for damaged,property_name in ((False,'density_meshes'),(True,'damaged_density_meshes')):
            paths=['/Game/CitySim/Art/Meshes/SM_Commercial'+density+'_Store'+business.title()+('_Damaged' if damaged else '') for density in ('Low','Medium','High')]
            meshes=[unreal.load_asset(path) for path in paths]
            assert all(meshes),paths
            variant.set_editor_property(property_name,meshes)
            row[property_name]=[m.get_path_name() for m in meshes]
        entries.append(variant);report.append(row)
    catalog.set_editor_property('commercial_presentation_variants',entries)
    assert [e.export_text() for e in catalog.get_editor_property('definitions')]==before,'Presentation changed gameplay definitions'
    assert len(catalog.get_editor_property('commercial_presentation_variants'))==8
    return report
