"""Index production asset previews and actual packaged world review images."""
from pathlib import Path
import hashlib,json,sys,html
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/Integration'
sys.path.insert(0,str(ROOT/'Tools'))
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text())
    report=json.loads((OUT/'Unreal/Report.json').read_text());assert len(report['views'])==26
    records={e['name']:e for e in doc['assets']}
    jobs=[r for r in report['views'] if not r['night'] and not r['detail'] and not r.get('business') and r['name'] in records and not records[r['name']]['damaged']]
    assert len(jobs)==12
    for r in report['views']:assert digest(OUT/'Unreal'/r['image'])==r['sha256']
    views=[dict(object=r['name'].replace('SM_LowDensity_',''),view='perspective / day',image=r['image'],sha256=r['sha256'],dimensions_m=records[r['name']]['dimensions_m'],triangles=r['triangles']) for r in jobs]
    manifest=dict(schema_version=1,source_unchanged=True,source=doc['source'],source_sha256=doc['source_sha256'],scene='Production low-density assets',frame=1,engine='Unreal D3D11 forward',renderer_label='Unreal',render_note='perspective / refined room reflections',resolution=1000,views=views)
    path=OUT/'Unreal/PreviewManifest.json';path.write_text(json.dumps(manifest,indent=2)+'\n')
    from Assets.Preview import CityPreviewSheets as module
    index=module.compose(path,OUT/'Unreal',4,3,400,'D104 | Integrated low-density buildings and materials')
    game=OUT/'Desktop-Final-User/Saved/Automation/LowDensity'
    if not game.is_dir():game=OUT/'Desktop-Packaged-User/Saved/Automation/LowDensity'
    images=sorted(game.glob('*.png'));assert len(images)==17
    for p in images:
        with Image.open(p) as im:assert im.size==(1280,800)
    world_keys=[f'{category}_{letter}_{surface}_Day.png' for category in ('Home','Shop','Factory') for letter,surface in [('A','Brick'),('B','Timber'),('C','Clapboard'),('D','Corrugated')]]
    face=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
    sheet=Image.new('RGB',(1680,1010),'#17212b');draw=ImageDraw.Draw(sheet);draw.text((20,16),'D104 | Actual packaged game (click gallery images for full resolution)',font=face,fill='white')
    world_refs={}
    for i,name in enumerate(world_keys):
        p=game/name;assert p.is_file();x=20+i%4*410;y=70+i//4*310
        im=Image.open(p).convert('RGB');im.thumbnail((400,250),Image.Resampling.LANCZOS);sheet.paste(im,(x,y))
        label=f'{chr(65+i%4)}{i//4+1} · '+name.replace('_Day.png','').replace('_',' ')
        draw.text((x,y+260),label,font=face,fill='white')
        world_refs[f'P01-{chr(65+i%4)}{i//4+1}']=dict(image=p.relative_to(OUT).as_posix(),sha256=digest(p),label=label)
    sheet.save(OUT/'GamePreviewSheet.png')
    (OUT/'GamePreviewSheets.json').write_text(json.dumps(dict(references=world_refs),indent=2)+'\n')
    cards=[]
    for p in images:
        rel=p.relative_to(OUT).as_posix();label=p.stem.replace('_',' ')
        cards.append(f'<figure><figcaption>{html.escape(label)}</figcaption><a href="{rel}"><img src="{rel}" alt="{html.escape(label)}"></a></figure>')
    asset_cards=[]
    for r in report['views']:
        rel='Unreal/'+r['image'];label=r['image'].replace('.png','').replace('SM_LowDensity_','').replace('_',' ')
        asset_cards.append(f'<figure><figcaption>{html.escape(label)}</figcaption><a href="{rel}"><img src="{rel}" alt="{html.escape(label)}"></a></figure>')
    page='''<!doctype html><html lang="en"><meta charset="utf-8"><title>D104 integrated building review</title><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{background:#17212b;color:#eaf2f8;font:16px/1.5 system-ui;max-width:1600px;margin:auto;padding:24px}a{color:#8fd5ff}img{width:100%;display:block}figure{margin:0;padding:12px;background:#253644;border-radius:8px}figcaption{margin-bottom:8px;font-weight:600}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px}summary{cursor:pointer;font-size:22px}p{max-width:1100px}@media(max-width:850px){.grid{grid-template-columns:1fr 1fr}}@media(max-width:550px){.grid{grid-template-columns:1fr}}</style>
<h1>D104 · Integrated building variants</h1><p>Four coordinated low-density looks per family: A brick, B timber, C siding, D corrugated panels. Approved shapes, entrances and scale are retained. Painted panels stay nonmetallic; bare industrial steel keeps its normal/roughness variation with reduced roughness and HQ reflections. Shop business signs remain independent of shape. Damage/repair and saved entity identity retain the selected appearance.</p>
<p>The maintained Windows game includes these assets; use the existing Desktop launcher. The inspector now uses perspective and the baked room reflection environment. No live city reflection recapture is introduced.</p>
<p><a href="GamePreviewSheet.png">Actual game sheet</a> · <a href="Unreal/PreviewSheet-01.png">Production asset sheet with ruler</a> · <a href="GamePreviewSheets.json">Game references</a> · <a href="Unreal/PreviewSheets.json">Asset references</a> · <a href="Implementation.json">Delivery checks</a>.</p>
<h2>Actual packaged game</h2><div class="grid">GAMECARDS</div><details><summary>Production asset, night-window, sign and damage review</summary><div class="grid">ASSETCARDS</div></details>
<p>Automated world/inspector/save-reload/damage checks use an isolated test city. Physical input, PCVR/Quest headset appearance and subjective material review remain unconfirmed. The shared Quest package is validated separately; no device installation or publication occurred.</p></html>'''
    page=page.replace('GAMECARDS',''.join(cards)).replace('ASSETCARDS',''.join(asset_cards))
    (OUT/'Review.html').write_text(page,encoding='utf-8')
    (OUT/'Review.json').write_text(json.dumps(dict(native_views=26,game_views=17,native_index=index,game_images=[dict(path=p.relative_to(OUT).as_posix(),sha256=digest(p)) for p in images],sheet_sha256=digest(OUT/'GamePreviewSheet.png')),indent=2)+'\n')
    print('CITYSIM_D104_INTEGRATION_REVIEW_PASS 26 asset views +17 packaged game views')
if __name__=='__main__':main()
