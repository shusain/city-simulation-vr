"""Indexed renderer sheets and side-by-side full-resolution comparison gallery."""
from pathlib import Path
import argparse
import html
import json
from PIL import Image
from CityUnrealPreview import ROOT, load_batch, digest

from Assets.Preview import CityPreviewSheets as sheet


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    parser.add_argument('--renderer', choices=['Blender', 'Unreal', 'Both'], default='Both')
    args = parser.parse_args(); batch = load_batch(args.batch); out = ROOT/batch['output']
    renderers = ['Blender', 'Unreal'] if args.renderer == 'Both' else [args.renderer]
    pages = []
    for renderer in renderers:
        for group, view in sorted({(e['group'], e['view']) for e in batch['renders']}):
            directory = out/renderer/group/view
            title = f'D104 {renderer} | {group} | {view}'
            if group == 'Materials': title += ' | A brick, B timber, C siding, D metal'
            result = sheet.compose(directory/'PreviewManifest.json', directory, columns=4, rows=3, tile_size=420, title=title)
            assert result['render_count'] == len([e for e in batch['renders'] if e['group'] == group and e['view'] == view])
            pages.append(dict(renderer=renderer, group=group, view=view, sheet=(directory/'PreviewSheet-01.png').relative_to(out).as_posix()))
    if args.renderer == 'Both':
        cards = []
        for group in ('Materials', 'Variants'):
            views = ['iso', 'front'] if group == 'Materials' else ['iso', 'front', 'back', 'entrance']
            for view in views:
                entries = [e for e in batch['renders'] if e['group'] == group and e['view'] == view]
                cards.append(f'<section data-group="{group}" data-view="{view}"'+(' hidden' if (group, view) != ('Materials', 'iso') else '')+'>')
                for i, entry in enumerate(entries):
                    cell = chr(65+i%4)+str(i//4+1)
                    pictures = ''
                    for renderer in ('Blender', 'Unreal'):
                        image = f'{renderer}/{group}/{view}/{entry["key"]}.png'
                        pictures += f'<a href="{image}" target="_blank"><img src="{image}" loading="lazy"><span>{renderer}</span></a>'
                    cards.append(f'<article><h3>{cell} · {html.escape(entry["object"].replace("_", " "))}</h3><div class="pair">{pictures}</div><p>{html.escape(entry.get("label", "Approved geometry"))}</p></article>')
                cards.append('</section>')
        gallery = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>D104 Paired building material review</title>
<style>body{background:#18202a;color:#edf1f5;font:16px Segoe UI,sans-serif;margin:24px}h1{font-size:28px}p{color:#b9c6d3;line-height:1.5}nav{display:flex;gap:12px;position:sticky;top:0;padding:12px;background:#18202a;z-index:1}select{font:inherit;padding:8px;background:#314456;color:white;border:1px solid #61788f}section{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}section[hidden]{display:none}article{background:#25313e;border-radius:8px;padding:12px}h3{margin:0 0 10px;font-size:17px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:8px}.pair img{width:100%}.pair a{color:#edf1f5;text-decoration:none}.pair span{display:block;text-align:center;padding:5px}@media(max-width:900px){section{grid-template-columns:1fr}}</style>
<h1>Building materials · Blender / Unreal</h1><p>Materials: A current brick, B timber, C painted horizontal siding, D corrugated metal (painted for homes/markets; galvanized for industry). Rows: home, market, factory. Each row uses the same approved C geometry. Click a render for full resolution.</p>
<p>Variant grids use the approved D103 A–D building layout; entrance details have markets row 1 and industry row 2. Camera direction, target, orthographic width, dimensions and 1.432 m citizen ruler match across engines. Unreal cameras recede along the same view axis where needed to keep the floor above the near plane. Blender uses Cycles/AgX and studio lights; Unreal uses the existing D3D11 forward path, fixed daylight, a neutral studio reflection cubemap and manual exposure. The existing dynamic-room policy disables lightmap reflection mixing. Lighting and tone mapping differ. This is an isolated asset study.</p>
<nav><select id="group"><option>Materials</option><option>Variants</option></select><select id="view"><option value="iso">Perspective</option><option value="front">Front</option><option value="back">Rear</option><option value="entrance">Entrance details</option></select></nav>'''+''.join(cards)+'''
<script>const group=document.querySelector('#group'),view=document.querySelector('#view');function update(){if(group.value==='Materials'&&['back','entrance'].includes(view.value))view.value='iso';document.querySelectorAll('section').forEach(s=>s.hidden=s.dataset.group!==group.value||s.dataset.view!==view.value);[...view.options].forEach(o=>o.disabled=group.value==='Materials'&&['back','entrance'].includes(o.value));}group.onchange=view.onchange=update;update();</script></html>'''
        (out/'Review.html').write_text(gallery, encoding='utf-8')
        for renderer in ('Blender', 'Unreal'):
            for entry in batch['renders']:
                directory = out/renderer/entry['group']/entry['view']
                doc = json.loads((directory/'PreviewManifest.json').read_text())
                record = next(r for r in doc['views'] if r['key'] == entry['key'])
                assert digest(directory/record['image']) == record['sha256']
                assert record['camera'] == entry['camera']
                with Image.open(directory/record['image']) as im: assert im.size == (800, 800)
        assert digest(ROOT/batch['approved_source']) == batch['approved_source_sha256']
        (out/'Delivery.json').write_text(json.dumps(dict(task='D104', status='Exploratory review delivered',
            sheets=pages, paired_views=len(batch['renders']), material_options=9,
            approved_source_preserved=True, source=batch['source'], source_sha256=batch['source_sha256'],
            production_changed=False, physical_device_verified=False, limitations=batch['limitations']), indent=2)+'\n')
    print('CITYSIM_D104_COMPARISON_PASS', len(pages), 'indexed sheets;', len(batch['renders']), 'views per engine')


if __name__ == '__main__': main()
