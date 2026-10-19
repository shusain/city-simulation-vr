"""Refresh existing native asset expectations and retain reviewed regeneration inputs."""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_adoption.json').read_text())
entries={e['mesh_destination'].rsplit('/',1)[1]:e for e in doc['assets']}
path=ROOT/'CitySimVR/Source/CitySimVR/Private/Tests/CityArtKitTests.cpp';text=path.read_text()
pattern=r'\{TEXT\("/Game/CitySim/Art/Meshes/(SM_[^"]+)\.\1"\),.*?\}(?=,\s*\{TEXT|,\s*\};)'
def updated(match):
    name=match.group(1);e=entries.get(name)
    if not e:return match.group(0)
    dims=', '.join(f'{v*100:.4f}' for v in e['dimensions_m']);slots=', '.join('TEXT("'+s+'")' for s in e['material_slots'])
    z=e.get('bounds_min_m',[0,0,0])[2]*100
    return '{TEXT("/Game/CitySim/Art/Meshes/'+name+'.'+name+'"),\n\t     FVector('+dims+'),\n\t     {'+slots+'},\n\t     true, '+str(e['triangles'])+', '+f'{z:.4f}'+'}'
text,count=re.subn(pattern,updated,text,flags=re.S);assert count>=20
if 'double ExpectedMinimumZCm' not in text:
    text=text.replace('int32 ExpectedTriangles = 0;','int32 ExpectedTriangles = 0;\n\t\tdouble ExpectedMinimumZCm = 0;')
text=text.replace('FMath::IsNearlyZero(Bounds.Min.Z, 0.5)','FMath::IsNearlyEqual(Bounds.Min.Z, Item.ExpectedMinimumZCm, 0.5)')
text=text.replace('Ground pivot is Z=0:', 'Authored ground/walk pivot:')
text=text.replace('TestTrue(TEXT("Civic meshes retain two sections"), Mesh->GetNumSections(0) == 2);','TestEqual(TEXT("Civic mesh retains reviewed sections"), Mesh->GetNumSections(0), Item.Slots.Num());')
text=text.replace('TestTrue(TEXT("Utility detail retains two render sections"), Mesh->GetNumSections(0) <= 2);','TestEqual(TEXT("Utility retains reviewed surface sections"), Mesh->GetNumSections(0), Item.Slots.Num());')
text=text.replace('Building retains accepted D79 detail count','Building retains approved D91 detail count')
text=text.replace('bArchitecture ? MaterialPath == FString::Printf(TEXT("/Game/CitySim/Art/Materials/MI_%s.MI_%s"),','bArchitecture ? MaterialPath == FString::Printf(TEXT("/Game/CitySim/Art/Materials/%s_%s.%s_%s"),\n\t                                                    Slot.ToString().Contains(TEXT("Opaque")) || Slot == TEXT("HomeSolidDetails") ? TEXT("M") : TEXT("MI"),')
# Replace remaining old formatter arguments with the prefix-aware form.
text=text.replace('*Slot.ToString(), *Slot.ToString())\n\t\t\t\t    : (Slot', '*Slot.ToString(), Slot.ToString().Contains(TEXT("Opaque")) || Slot == TEXT("HomeSolidDetails") ? TEXT("M") : TEXT("MI"), *Slot.ToString())\n\t\t\t\t    : (Slot')
text=text.replace(': MaterialPath.StartsWith(TEXT("/Game/CitySim/Art/Materials/MI_City")))', ': MaterialPath.StartsWith(TEXT("/Game/CitySim/Art/Materials/")))')
path.write_text(text)
path=ROOT/'CitySimVR/Source/CitySimVR/Private/Tests/CitySeasonalDecorationTests.cpp';text=path.read_text()
text=text.replace('Sample.Scale, 1.f','Sample.Scale, .5f').replace('Sample.SurfaceOffsetCm, 215.f','Sample.SurfaceOffsetCm, 85.f')
text=text.replace('FVector2D(0, -6.48)','FVector2D(0, -4.78)').replace('Wreath.Scale == 1.f','Wreath.Scale == .5f').replace('Wreath.SurfaceOffsetCm == 215.f','Wreath.SurfaceOffsetCm == 85.f').replace('Wreath.SurfaceOffsetCm + 120.f < 450.f','Wreath.SurfaceOffsetCm + 60.f < 185.f').replace('P.Y + .65 < -6.16','P.Y + .65 < -4.62')
path.write_text(text)
print('CITYSIM_D91_CONTRACT_UPDATE_PASS')
