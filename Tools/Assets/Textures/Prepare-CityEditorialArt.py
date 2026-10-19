"""Derive only the human-reviewed newspaper masks and repair display atlas."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
from pathlib import Path
import hashlib,json,argparse
import numpy as np
from PIL import Image
from Assets.Manifests.CityApprovedSources import verified_export
from Assets.Manifests.CityEditorialArt import EDITORIAL_USAGES, editorial_entries


def main():
    root=repository_root(__file__)
    parser=argparse.ArgumentParser()
    parser.add_argument('--verified-source-root',type=Path)
    args=parser.parse_args()
    out=root/'ArtSource/Derived/EditorialArt';out.mkdir(parents=True,exist_ok=True)
    sha=lambda b:hashlib.sha256(b).hexdigest()
    pins={'housing':(33,1,'3c376285ecda9e828116b56f8e9ce1af56d098212a0f6424ef2298443960c5b7'),
          'commerce':(34,1,'076f0bce24f36b90dd6d1d270e5ac1953c59f970827ea3cc253187ab4868d0e9'),
          'automation':(35,1,'12beb2f621b47ab721355300395deeb336f8127150ac950b0df8791a6870bdfe'),
          'repairs':(36,2,'46e554ee2cfeefa2388779ce4ed24406f7b533ec53b02a9a949acbd76f59b4fa')}
    records=[]
    for key,(n,version,approval_sha) in pins.items():
        usage=EDITORIAL_USAGES[key]
        relative=f'ArtSource/Generated/comfy/issue-{n}/v{version}'
        source=(args.verified_source_root/f'issue-{n}-v{version}') if args.verified_source_root else root/relative
        verified_export(source,n,version,approval_sha)
        image=Image.open(source/'asset.png').convert('RGB')
        if usage=='newspaper_illustration':
            # Same threshold, luminance coefficients, crop threshold and padding as
            # the accepted browser proof. White RGB permits native Slate ink tint.
            p=np.asarray(image,dtype=np.float64)
            y=p[:,:,0]*.2126+p[:,:,1]*.7152+p[:,:,2]*.0722
            a=np.clip((218-y)/(218-40),0,1)
            yy,xx=np.where(a>.14)
            box=(max(0,int(xx.min())-12),max(0,int(yy.min())-12),min(768,int(xx.max())+13),min(768,int(yy.max())+13))
            rgba=np.full((768,768,4),255,dtype=np.uint8);rgba[:,:,3]=np.floor(a*255+.5).astype(np.uint8)
            derived=Image.fromarray(rgba).crop(box)
            name=key.title()
            path=out/f'Newspaper{name}.png';derived.save(path)
            records.append(dict(issue=n,version=version,approval_sha256=approval_sha,source=relative+'/asset.png',source_sha256=sha((source/'asset.png').read_bytes()),file=path.relative_to(root).as_posix(),sha256=sha(path.read_bytes()),crop=list(box),size=list(derived.size),native='/Game/CitySim/UI/Newspaper/T_'+name,role='white RGB soft ink alpha; UI; clamp; no mipmaps'))
        else:
            # Remove the generated outer frame; preserve the reviewed panel contents.
            boxes=[(78,188,375,372),(408,188,702,372),(78,402,375,592),(411,402,702,592)]
            # Every regular pane in the seven reviewed meshes is220x300cm.
            #242x330 pixels has that exact aspect. Use one scale for both axes;
            # transparent unused area lets the original glass show through.
            atlas=Image.new('RGBA',(512,1024))
            fits=[]
            for i,box in enumerate(boxes):
                panel=image.crop(box).convert('RGBA')
                scale=min(242/panel.width,330/panel.height)
                tx=(242-panel.width*scale)/2;ty=(330-panel.height*scale)/2
                tile=panel.transform((242,330),Image.Transform.AFFINE,
                    (1/scale,0,-tx/scale,0,1/scale,-ty/scale),
                    Image.Resampling.BICUBIC,fillcolor=(0,0,0,0))
                ox=(i%2)*256;oy=(i//2)*512
                atlas.paste(tile,(ox+7,oy+91))
                atlas.paste(tile.crop((0,0,1,330)).resize((7,330)),(ox,oy+91))
                atlas.paste(tile.crop((241,0,242,330)).resize((7,330)),(ox+249,oy+91))
                atlas.paste(atlas.crop((ox,oy+91,ox+256,oy+92)).resize((256,91)),(ox,oy))
                atlas.paste(atlas.crop((ox,oy+420,ox+256,oy+421)).resize((256,91)),(ox,oy+421))
                assert abs(220/242-300/330)<1e-12
                fits.append(dict(panel=i,uniform_scale=scale,translation=[tx,ty],
                                 draw_size=[panel.width*scale,panel.height*scale],
                                 physical_cm_per_pixel=220/242))
            path=out/'RepairDisplays.png';atlas.save(path)
            records.append(dict(issue=n,version=version,approval_sha256=approval_sha,source=relative+'/asset.png',source_sha256=sha((source/'asset.png').read_bytes()),file=path.relative_to(root).as_posix(),sha256=sha(path.read_bytes()),panel_crops=boxes,size=[512,1024],tile_size=[256,512],inner_size=[242,330],inner_offset=[7,91],pane_cm=[220,300],fits=fits,native='/Game/CitySim/Art/Textures/T_RepairDisplays',role='sRGB RGBA; uniform contain fit; transparent unused glass; not stock/inventory'))
        records[-1].update(key=key,usage=usage)
    doc=dict(task='D122',recipe='soft ink cutoff218 black40; frame-free repairs uniformly contained in220x300cm pane ratio',source_acceptance='Shaun accepts composite/repairs v2; requests entrance exclusion and preserved aspect in chat2026-10-16',assets=records)
    editorial_entries(doc)
    (out/'manifest.json').write_text(json.dumps(doc,indent=2)+'\n')
    print('CITYSIM_EDITORIAL_ART_DERIVED_PASS 3 exact-source ink masks and repair atlas')


if __name__ == '__main__':
    main()
