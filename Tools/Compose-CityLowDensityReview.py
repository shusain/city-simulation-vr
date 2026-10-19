"""Compose D103 grid references and a portable review gallery; verify delivery."""
from pathlib import Path
import argparse,html,json,hashlib,sys
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Preview.CityPreviewSheets import compose


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory',type=Path,default=ROOT/'Artifacts/D103')
    args=parser.parse_args()
    out=args.output_directory.absolute()
    study=json.loads((out/'Study.json').read_text())
    source=ROOT/study['source']
    assert digest(source)==study['source_sha256']
    verification=json.loads((out/'SourceVerification.json').read_text())
    assert verification['geometry_pass'] and verification['source_sha256']==digest(source)
    receipts={}
    view_names=['iso','front','back']+(['entrance'] if study.get('revision',1)>=3 else [])
    for view in view_names:
        directory=out/'Preview'/view
        manifest=directory/'PreviewManifest.json'
        data=json.loads(manifest.read_text())
        assert data['source_sha256']==digest(source)
        count=8 if view=='entrance' else 12
        assert len(data['views'])==count
        label='markets row1, industrial row2' if view=='entrance' else 'A references with revised entrances' if study.get('revision',1)>=3 else 'A reference (A2 sign fixed)' if study.get('revision',1)>=2 else 'A current'
        report=compose(manifest,directory,columns=4,rows=3,tile_size=420,
                       title=f'D103 Low-density variants | {view} | {label}, B/C/D alternatives')
        assert len(report['pages'])==1 and len(report['references'])==count
        for row,category in enumerate(('Shop','Factory') if view=='entrance' else ('Home','Shop','Factory'),1):
            for column in ('A','B','C','D'):
                assert report['references'][f'P01-{column}{row}']['object'].startswith(category+'_')
        receipts[view]=dict(sheet=str((directory/'PreviewSheet-01.png').relative_to(ROOT)),
                            sheet_sha256=digest(directory/'PreviewSheet-01.png'),
                            references=str((directory/'PreviewSheets.json').relative_to(ROOT)),
                            manifest_sha256=digest(manifest))
    views=[]
    for view in view_names:
        manifest=json.loads((out/'Preview'/view/'PreviewManifest.json').read_text())
        tiles=[]
        for index,record in enumerate(manifest['views']):
            ref=chr(65+index%4)+str(index//4+1)
            image=f'Preview/{view}/{record["image"]}'
            tiles.append(f'<a class="tile" href="{html.escape(image)}" target="_blank"><img src="{html.escape(image)}"><b>{ref} — {html.escape(record["object"].replace("_"," "))}</b><small>{record["triangles"]} triangles · '+
                         ' × '.join(f'{d:.2f}' for d in record['dimensions_m'])+' m</small></a>')
        views.append(f'<section id="{view}" '+('hidden' if view!='iso' else '')+'>'+''.join(tiles)+'</section>')
    gallery='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>D103 Low-density building alternatives</title><style>
body{margin:0;background:#18202a;color:#edf1f5;font:16px Segoe UI,sans-serif;padding:24px}h1{font-size:27px;margin:0 0 10px}p{color:#b9c6d3;max-width:1000px;line-height:1.5}
nav{display:flex;gap:8px;margin:20px 0;position:sticky;top:0;background:#18202a;padding:12px 0}button{background:#314456;color:white;border:1px solid #61788f;border-radius:6px;padding:10px 24px;cursor:pointer}button[aria-pressed=true]{background:#326686}
section{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}section[hidden]{display:none}.tile{display:flex;flex-direction:column;background:#25313e;border-radius:8px;overflow:hidden;text-decoration:none;color:inherit}.tile img{width:100%;aspect-ratio:1;object-fit:contain}.tile b,.tile small{padding:10px 12px}.tile small{padding-top:0;color:#b9c6d3}@media(max-width:800px){section{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style><h1>Low-density building alternatives</h1><p>Column A is the current model; B, C and D are new alternatives. Row 1: homes. Row 2: shops. Row 3: factories. Cameras use the same scale within each category; every model has a 1.432 m citizen ruler. Click any image to see its full-resolution render.</p>
<nav><button aria-pressed="true" data-view="iso">Perspective</button><button aria-pressed="false" data-view="front">Front</button><button aria-pressed="false" data-view="back">Rear</button></nav>'''+''.join(views)+'''<p>Editable source: ArtSource/Blender/Studies/CityLowDensityVariants.blend. This is a Blender appearance study; production selection and native/headset review follow selected designs.</p><script>
document.querySelectorAll('button').forEach(button=>button.addEventListener('click',()=>{document.querySelectorAll('section').forEach(section=>section.hidden=section.id!==button.dataset.view);document.querySelectorAll('button').forEach(other=>other.setAttribute('aria-pressed',other===button?'true':'false'));}));
</script></html>'''
    if study.get('revision',1)>=2:
        gallery=gallery.replace('Column A is the current model;', 'Column A is the current reference, with the proposed A2 sign correction;')
    if study.get('revision',1)>=3:
        gallery=gallery.replace('Column A is the current reference, with the proposed A2 sign correction;', 'Column A is the current reference with the proposed sign and entrance changes;')
        gallery=gallery.replace('</nav>','<button aria-pressed="false" data-view="entrance">Entrance details</button></nav><p>In entrance details: markets row1, industrial row2.</p>')
    gallery=gallery.replace('ArtSource/Blender/Studies/CityLowDensityVariants.blend',study['source'])
    (out/'Review.html').write_text(gallery,encoding='utf-8')
    assert all(digest(ROOT/p)==h for p,h in study['protected'].items())
    render_entries=[entry for view in view_names for entry in json.loads((out/'Preview'/view/'PreviewManifest.json').read_text())['views']]
    reused=sum(bool(entry.get('reused_verified_unchanged_geometry')) for entry in render_entries)
    prior_preserved=None
    earlier_reviews={}
    if out!=ROOT/'Artifacts/D103':
        prior=json.loads((ROOT/'Artifacts/D103/Study.json').read_text())
        assert digest(ROOT/prior['source'])==prior['source_sha256']
        assert all(digest(ROOT/record['export'])==record['sha256'] for record in prior['assets'])
        for view in ('iso','front','back'):
            old_directory=ROOT/'Artifacts/D103/Preview'/view
            old=json.loads((old_directory/'PreviewManifest.json').read_text())
            assert all(digest(old_directory/record['image'])==record['sha256'] for record in old['views'])
        prior_preserved=True
        revision2=ROOT/'Artifacts/D103/Revision2'
        if out!=revision2 and (revision2/'Study.json').is_file():
            prior=json.loads((revision2/'Study.json').read_text())
            assert digest(ROOT/prior['source'])==prior['source_sha256']
            assert all(digest(ROOT/record['export'])==record['sha256'] for record in prior['assets'])
            for view in ('iso','front','back'):
                old=json.loads((revision2/'Preview'/view/'PreviewManifest.json').read_text())
                assert all(digest(revision2/'Preview'/view/record['image'])==record['sha256'] for record in old['views'])
            earlier_reviews['Revision2']=True
    exported=sum(bool(r['export']) for r in study['assets'])
    receipt=dict(task='D103',revision=study.get('revision',1),status='Blender review delivered',variant_count=9,reference_count=3,
                 fbxs_verified=exported,renders_verified=len(render_entries),source=str(source.relative_to(ROOT)),source_sha256=digest(source),
                 last_render_pass=dict(freshly_rendered=len(render_entries)-reused,reused_verified_unchanged=reused),
                 earlier_reviews_preserved=earlier_reviews,exports_authored=study.get('exports_authored',True),
                 first_review_preserved=prior_preserved,adjustment_checks=verification.get('adjustment_checks',{}),
                 source_verification_sha256=digest(out/'SourceVerification.json'),sheets=receipts,
                 gallery=(out/'Review.html').relative_to(ROOT).as_posix(),gallery_sha256=digest(out/'Review.html'),
                 protected_unchanged=True,production_imported=False,manual_blender_selection_pending=True,
                 native_device_appearance_unconfirmed=True)
    (out/'Delivery.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('CITYSIM_D103_REVIEW_PASS 9 alternatives;',exported,'FBXs;',len(render_entries),'renders;',len(view_names),'labeled sheets')


if __name__=='__main__':main()
