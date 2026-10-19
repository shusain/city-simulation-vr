"""Verify reflected cue pixels and compose the isolated D104 comparison."""
from pathlib import Path
import ast
import hashlib
import html
import json
from html.parser import HTMLParser
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Artifacts/D104/Reflectivity'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    reports={}; rows=[]
    for directory, count in [('',24),('Built',20),('Perspective',20),('PerspectiveControls',24),('Polished',4)]:
        folder=OUT/directory
        report=json.loads((folder/'Report.json').read_text())
        assert len(report['views'])==count and not report['cooked'] and not report['production_changed']
        assert report['objects_only_hidden_from_scene_capture']
        reports[directory]=report
        for view in report['views']:
            path=folder/view['image']; assert digest(path)==view['sha256']
            pixels=np.asarray(Image.open(path).convert('RGB'),dtype=np.int16)
            assert pixels.shape==(1000,1000,3)
            # Hidden-cue captures contain no direct red/green objects. Strong
            # chroma then provides image-content evidence, not brightness alone.
            red=(pixels[:,:,0]>pixels[:,:,1]+40)&(pixels[:,:,0]>pixels[:,:,2]+40)&(pixels[:,:,0]>100)
            green=(pixels[:,:,1]>pixels[:,:,0]+40)&(pixels[:,:,1]>pixels[:,:,2]+40)&(pixels[:,:,1]>100)
            row=dict(view, folder=directory, relative_image=path.relative_to(OUT).as_posix(),
                red_pixels=int(red.sum()),green_pixels=int(green.sum()))
            rows.append(row)

    def get(folder, condition, material, hidden=True):
        return next(r for r in rows if r['folder']==folder and r['condition']==condition and r['material']==material and r['cue_hidden_from_camera']==hidden)

    ortho=get('Built','E_Automatic','Smooth_002_HQ')
    perspective=get('Perspective','E_Automatic','Smooth_002_HQ')
    built=get('Perspective','F_Built','Smooth_002_HQ')
    no_probe=get('PerspectiveControls','G_NoProbe','Smooth_002_HQ')
    uniform=get('PerspectiveControls','H_UniformCube','Smooth_002_HQ')
    scene=get('PerspectiveControls','I_CapturedScene','Smooth_002_HQ')
    polished=get('Polished','K_CapturedScene','D104_Corrugated_Polished_HQ')
    ready=get('','A_NoProbe','D104_Corrugated')
    assert float(np.asarray(Image.open(OUT/ready['relative_image']).convert('RGB'))[350:690,160:840,0].std())>8, 'Corrugated shader fallback'
    assert all(r['red_pixels']==r['green_pixels']==0 for r in [ortho,no_probe,uniform])
    assert all(r['red_pixels']>1000 and r['green_pixels']>1000 for r in [perspective,built,scene,polished])
    assert not reports['PerspectiveControls']['lightmass_built'] and not reports['Polished']['lightmass_built']
    assert not perspective['baked'] and built['baked']
    difference=np.abs(np.asarray(Image.open(OUT/perspective['relative_image']).convert('RGB'),dtype=float)-
        np.asarray(Image.open(OUT/built['relative_image']).convert('RGB'),dtype=float))
    source=json.loads((ROOT/'Artifacts/D104/Batch.json').read_text())
    assert digest(ROOT/source['source'])==source['source_sha256']
    assert digest(ROOT/source['approved_source'])==source['approved_source_sha256']
    protected=json.loads((ROOT/'Artifacts/D104/ProtectedInputs.json').read_text(encoding='utf-8-sig'))
    assert all(digest(p).upper()==h for p,h in protected.items())
    gallery_views=0
    for renderer in ('Blender','Unreal'):
        report=json.loads((ROOT/'Artifacts/D104'/renderer/'Report.json').read_text())
        for r in report['views']:
            assert digest(ROOT/'Artifacts/D104'/renderer/r['group']/r['view']/r['image'])==r['sha256']
            gallery_views+=1
    native=json.loads((OUT/'MeatSaberNative.json').read_text()) if (OUT/'MeatSaberNative.json').is_file() else None
    meat_verified=False
    if native:
        assert native['scalar_parameters']['Metallic']==1
        assert abs(native['scalar_parameters']['Roughness']-.05)<1e-6
        meat_names=json.loads((OUT/'MeatSaberNames.json').read_text())
        meat_root=Path(native['source_repository'])
        if meat_root.is_dir():
            assert all(digest(meat_root/r['path'])==r['sha256'] for r in meat_names)
            assert digest(meat_root/'Content/Saber/Materials/BaseHandle.uasset')==digest(ROOT/'Artifacts/D104/UnrealProject/Content/Saber/Materials/BaseHandle.uasset')
            meat_verified=True
    for name in ('Capture-CityReflectiveMaterials.py','Compose-CityReflectionStudy.py'):
        ast.parse((ROOT/'Tools'/name).read_text())

    font_path=Path('C:/Windows/Fonts/segoeui.ttf')
    font=ImageFont.truetype(str(font_path),24); small=ImageFont.truetype(str(font_path),19)
    references={}

    def sheet(filename, title, cells, columns):
        cw=680; ch=495; margin=20; top=85
        image=Image.new('RGB',(columns*cw+margin*2,top+((len(cells)+columns-1)//columns)*ch+margin),'#17212b')
        draw=ImageDraw.Draw(image); draw.text((margin,20),title,font=font,fill='white')
        draw.text((margin,53),'Direct objects hidden from camera; all colored shapes below are reflected content.',font=small,fill='#b9c9d8')
        for index,(label,row) in enumerate(cells):
            x=margin+(index%columns)*cw; y=top+(index//columns)*ch
            draw.text((x,y),label,font=font,fill='white')
            source_image=Image.open(OUT/row['relative_image']).convert('RGB').crop((80,210,920,750))
            source_image=source_image.resize((660,424),Image.Resampling.LANCZOS)
            image.paste(source_image,(x,y+38))
            draw.text((x,y+465),f"Red: {row['red_pixels']:,} px  |  Green: {row['green_pixels']:,} px",font=small,fill='#b9c9d8')
            references[f'{filename}:{index+1}']=dict(label=label,image=row['relative_image'],sha256=row['sha256'],
                source_crop=[80,210,920,750],direct_objects_hidden=True)
        image.save(OUT/filename)

    sheet('ReflectionProof.png','D104 | Camera and reflection-source proof',[
        ('Orthographic + captured scene + HQ',ortho),('Perspective + captured scene + HQ',perspective),
        ('Perspective + no local probe',no_probe),('Perspective + uniform studio cubemap',uniform),
        ('Perspective before lighting build',perspective),('Perspective after lighting build',built)],2)
    material_keys=['Smooth_002','Smooth_002_HQ','Rough_036_HQ','D104_Corrugated','D104_Corrugated_HQ']
    labels=['Smooth / default','Smooth / HQ','Rough 0.36 / HQ','Approved corrugated','Corrugated / HQ copy']
    cells=[]
    for folder,projection in [('Built','Ortho'),('Perspective','Perspective')]:
        cells.extend((projection+' | '+label,get(folder,'E_Automatic',key)) for key,label in zip(material_keys,labels))
    sheet('ForwardMaterials.png','D104 | Forward reflection and material controls',cells,5)
    sheet('Roughness.png','D104 | Surface roughness (study copies only)',[
        ('Smooth metal / roughness 0.05',get('Polished','K_CapturedScene','Smooth_005_HQ')),
        ('Smooth metal / roughness 0.36',get('Perspective','E_Automatic','Rough_036_HQ')),
        ('Corrugated / roughness 0.05 copy',polished),
        ('Corrugated / approved roughness ~0.36',get('Perspective','E_Automatic','D104_Corrugated_HQ'))],2)
    sheet('LightmapMixing.png','D104 | Rough-metal mixing control (no bake)',[
        ('Rough metal / lightmap mixing OFF',get('PerspectiveControls','I_CapturedScene','Rough_036_HQ')),
        ('Rough metal / lightmap mixing ON',get('PerspectiveControls','J_CapturedScene_Mixing','Rough_036_HQ')),
        ('Corrugated / lightmap mixing OFF',get('PerspectiveControls','I_CapturedScene','D104_Corrugated_HQ')),
        ('Corrugated / lightmap mixing ON',get('PerspectiveControls','J_CapturedScene_Mixing','D104_Corrugated_HQ'))],2)

    data={f"{r['folder']}|{r['condition']}|{r['material']}|{int(r['cue_hidden_from_camera'])}":r['relative_image'] for r in rows}
    page='''<!doctype html><html lang="en"><meta charset="utf-8"><title>D104 isolated reflection review</title>
<meta name="viewport" content="width=device-width,initial-scale=1"><style>
body{background:#15212b;color:#e9f1f7;font:16px/1.55 system-ui;margin:0;padding:28px;max-width:1500px;margin:auto}h1,h2{line-height:1.2}p{max-width:1050px}a{color:#88d5ff}select,button{font:inherit;background:#294052;color:white;border:1px solid #7093a7;padding:8px;border-radius:6px;margin:4px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}figure{margin:0;background:#202f3b;padding:12px;border-radius:8px}img{width:100%;display:block}figcaption{font-weight:600;margin-bottom:8px}.frame{height:clamp(240px,36vw,510px);object-fit:cover;object-position:center}details{margin:24px 0}summary{cursor:pointer;font-size:20px}table{border-collapse:collapse}td,th{padding:9px;border-bottom:1px solid #547082;text-align:left}@media(max-width:700px){.grid{grid-template-columns:1fr}.frame{height:340px}}
</style><h1>D104 · Isolated reflections</h1>
<p><strong>Reflections work in the perspective control before lighting baking or cooking.</strong> The same flat metal, scene probe and HQ material remain gray in this orthographic SceneCapture2D setup. Red cube and green sphere are hidden only from the camera; colored shapes on the surface prove reflection image content.</p>
<label>Surface <select id="material"><option value="Smooth_002_HQ">Smooth metal · HQ reflections</option><option value="Smooth_002">Smooth metal · default reflections</option><option value="Rough_036_HQ">Rough metal · 0.36 · HQ</option><option value="D104_Corrugated">Approved corrugated material</option><option value="D104_Corrugated_HQ">Corrugated · HQ study copy</option></select></label>
<label>View <select id="visibility"><option value="1">Reflection only</option><option value="0">Objects visible for reference</option></select></label>
<div class="grid"><figure><figcaption>Orthographic · captured scene · no bake</figcaption><a id="orthoLink"><img class="frame" id="ortho" alt="Orthographic reflection control"></a></figure><figure><figcaption>Perspective · captured scene · no bake</figcaption><a id="perspectiveLink"><img class="frame" id="perspective" alt="Perspective reflection control"></a></figure></div>
<p>The constant studio cubemap contains no objects or lighting features. Default forward reflections also place these nearby objects poorly on a flat plane; HQ captures improve placement, while roughness blurs them. Capture projections are approximate, so reflected positions differ from a true mirror.</p>
<h2>Does a lighting build change the result?</h2><div class="grid"><figure><figcaption>Before preview lighting build</figcaption><a id="beforeLink"><img class="frame" id="before" alt="Before lighting build"></a></figure><figure><figcaption>After preview lighting build</figcaption><a id="afterLink"><img class="frame" id="after" alt="After lighting build"></a></figure></div>
<p>No Lightmass build or cooking is needed to obtain colored scene reflections in this Editor test. Automatic Editor probe updates supplied the capture. Persisted reflection build data is still needed for a packaged capture-based scene; packaged/mobile/headset behavior was deferred.</p>
<h2>MeatSaber precedent</h2><p>Read-only inspection of the cloned project found sphere reflection captures in both maps and an engine metal parent for BaseHandle: metallic 1.0, roughness 0.05. HQ forward reflections are off on that parent. Its config also selects Lumen, but Lumen is incompatible with forward shading; that setting is not adopted here. The roughness controls below use separate copies and leave the approved ~0.36 corrugated surface unchanged.</p>
<div class="grid"><figure><figcaption>Corrugated study copy · roughness 0.05 · HQ</figcaption><a href="POLISHED"><img class="frame" src="POLISHED" alt="Polished corrugated reflection proof"></a></figure><figure><figcaption>Approved roughness · HQ copy</figcaption><a href="ROUGH"><img class="frame" src="ROUGH" alt="Rough corrugated comparison"></a></figure></div>
<details><summary>Reflection source and mixing controls</summary><div class="grid">CONTROLS</div><p>All four controls use perspective, smooth HQ metal and no lighting build. Lightmap mixing primarily suppresses rough surfaces, shown in the downloadable sheet.</p></details>
<p>Download sheets: <a href="ReflectionProof.png">camera/source proof</a> · <a href="ForwardMaterials.png">forward/material matrix</a> · <a href="Roughness.png">roughness</a> · <a href="LightmapMixing.png">lightmap mixing</a>. <a href="Verification.json">verification</a> · <a href="PreviewSheets.json">full-resolution reference index</a> · <a href="../Review.html">approved building/material review</a></p>
<p>Sources: <a href="https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine">Epic forward shading</a> · <a href="https://dev.epicgames.com/documentation/unreal-engine/reflections-captures-in-unreal-engine">reflection captures</a> · <a href="https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine">Lumen limitations</a>.</p>
<script>const images=DATA;function set(id,folder,condition,material,hidden){const path=images[[folder,condition,material,hidden].join('|')];document.getElementById(id).src=path;document.getElementById(id+'Link').href=path}function update(){const m=document.getElementById('material').value,h=document.getElementById('visibility').value;set('ortho','Built','E_Automatic',m,h);set('perspective','Perspective','E_Automatic',m,h);set('before','Perspective','E_Automatic',m,h);set('after','Perspective','F_Built',m,h)}document.getElementById('material').onchange=update;document.getElementById('visibility').onchange=update;update();</script></html>'''
    controls=[]
    for condition,title in [('G_NoProbe','No local probe'),('H_UniformCube','Uniform studio cubemap'),('I_CapturedScene','Captured scene · mixing off'),('J_CapturedScene_Mixing','Captured scene · mixing on')]:
        path=get('PerspectiveControls',condition,'Smooth_002_HQ')['relative_image']
        controls.append(f'<figure><figcaption>{html.escape(title)}</figcaption><a href="{path}"><img class="frame" src="{path}" alt="{html.escape(title)}"></a></figure>')
    page=page.replace('POLISHED',polished['relative_image']).replace('ROUGH',get('Perspective','E_Automatic','D104_Corrugated_HQ')['relative_image']).replace('CONTROLS',''.join(controls)).replace('DATA',json.dumps(data))
    class References(HTMLParser):
        def handle_starttag(self,tag,attrs):
            for key,value in attrs:
                if key in ('src','href') and value and not value.startswith(('https:','http:')):
                    # Index/verification files are written just below.
                    if value not in ('PreviewSheets.json','Verification.json'): assert (OUT/value).is_file(),value
    References().feed(page)
    (OUT/'Review.html').write_text(page,encoding='utf-8')
    (OUT/'PreviewSheets.json').write_text(json.dumps(dict(task='D104',references=references),indent=2)+'\n')
    result=dict(task='D104',status='Controlled reflection image content verified',captures=len(rows),
        strong_chroma_threshold='R or G > other two channels +40; channel >100',metrics=rows,
        perspective_before_after_build_mean_absolute_rgb_difference=float(difference.mean()),
        perspective_before_after_identical=perspective['sha256']==built['sha256'],
        editor_reflections_without_lighting_build_or_cooking=True,
        orthographic_hq_missing_cue_content=True,lightmass_does_not_fix_orthographic_result=True,
        scene_capture_is_approximate_not_planar_mirror=True,
        automatic_corrugated_texture_pattern_verified=True,
        protected_native_inputs=len(protected),approved_gallery_views_unchanged=gallery_views,
        approved_sources_unchanged=True,meat_saber_inspected_read_only=bool(native),meat_saber_current_source_hashes_verified=meat_verified,production_changed=False,
        limitation='Cooked/package/mobile/headset reflection behavior not tested; persistent map capture build data required for capture-based game scenes.',
        next_step='Add separate perspective reflection-aware building material previews; preserve scale and approved orthographic geometry sheets.',
        sheets=[dict(path=n,sha256=digest(OUT/n)) for n in ['ReflectionProof.png','ForwardMaterials.png','Roughness.png','LightmapMixing.png']],
        review_sha256=digest(OUT/'Review.html'))
    (OUT/'Verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('CITYSIM_D104_REFLECTION_PROOF_PASS '+str(len(rows))+' native captures; '+str(gallery_views)+' approved gallery views preserved')


if __name__=='__main__': main()
