"""Derive uniformly scaled, mip-safe room textures from exact human-approved exports."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
from pathlib import Path
import hashlib,json
from PIL import Image
from Assets.Manifests.CityApprovedSources import verified_export

def main():
    root=repository_root(__file__)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    pins=json.loads((root/'docs/evidence/D122-room-exports.json').read_text(encoding='utf-8-sig'))
    out=root/'ArtSource/Derived/StorefrontRooms';out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for pin in pins:
        source=root/pin['directory']
        verified_export(source,pin['issue'],1,pin['approval_sha256'])
        assert sha(source/'asset.png')==pin['png_sha256']
        im=Image.open(source/'asset.png').convert('RGB');assert im.size==(768,768)
        crop=im.crop((109,9,659,759));assert crop.size==(550,750)
        # Both axes use exactly .88. Repeat edge pixels into padding, never stretch
        # merchandise to the power-of-two texture bounds. UVs address only the inset.
        room=crop.resize((484,660),Image.Resampling.LANCZOS)
        atlas=Image.new('RGB',(512,1024));atlas.paste(room,(14,182))
        atlas.paste(room.crop((0,0,1,660)).resize((14,660)),(0,182))
        atlas.paste(room.crop((483,0,484,660)).resize((14,660)),(498,182))
        atlas.paste(atlas.crop((0,182,512,183)).resize((512,182)),(0,0))
        atlas.paste(atlas.crop((0,841,512,842)).resize((512,182)),(0,842))
        target=out/(pin['key']+'.png');atlas.save(target)
        key=pin['key']
        meshes=['/Game/CitySim/Art/Meshes/SM_Commercial'+d+'_Store'+key.capitalize() for d in ('Low','Medium','High')]
        # The established catalog uses the base A/B/C/D meshes for market.
        meshes+=['/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_'+v+('' if key=='market' else '_'+key) for v in 'ABCD']
        for mesh in meshes:assert (root/('CitySimVR/Content/'+mesh.removeprefix('/Game/')+'.uasset')).is_file(),mesh
        rows.append(dict(**pin,file=target.relative_to(root).as_posix(),sha256=sha(target),source_crop=[109,9,659,759],uniform_scale=.88,inner_size=[484,660],inner_offset=[14,182],size=[512,1024],pane_cm=[220,300],native_texture='/Game/CitySim/Art/Textures/T_StorefrontRoom_'+key,native_material='/Game/CitySim/Art/Materials/MI_StorefrontRoom_'+key,native_meshes=meshes))
    (out/'manifest.json').write_text(json.dumps(dict(task='D122',human_acceptance='Shaun accepts eight-store v1 closeups972 and requests medium/high coverage,2026-10-16',geometry_added=0,assets=rows),indent=2)+'\n')
    print('CITYSIM_STOREFRONT_ROOMS_DERIVED_PASS',len(rows),'uniform11:15 room textures')


if __name__ == '__main__':
    main()
