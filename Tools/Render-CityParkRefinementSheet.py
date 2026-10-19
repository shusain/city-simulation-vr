"""Label/combine D83 Blender renders; bundled Python/Pillow, no Unreal."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/ArtPreview/D83'
manifest=json.loads((ROOT/'ArtSource/Manifests/city_park_refinement.json').read_text())
font_path='C:/Windows/Fonts/segoeui.ttf'
font=ImageFont.truetype(font_path,22)
small=ImageFont.truetype(font_path,17)
labels={'building.watertower.a':'Water tower','building.service.a':'Water treatment',
        'building.service.medium.a':'Water treatment - medium','building.service.high.a':'Water treatment - high',
        'building.park.a':'Basic / corner park','building.park.neighborhood':'Neighborhood park',
        'building.park.city':'City park','building.park.skate':'Skate park'}
for category,ids in (
        ('Water',('building.watertower.a','building.service.a','building.service.medium.a','building.service.high.a')),
        ('Parks',('building.park.a','building.park.neighborhood','building.park.city','building.park.skate'))):
    sheet=Image.new('RGB',(1000,1870),'#18202a');draw=ImageDraw.Draw(sheet)
    draw.text((20,12),f'D83 {category} - Blender source preview',font=font,fill='white')
    draw.text((20,45),'Before                                        Refined | identical camera and bounds',font=small,fill='#ccd8e4')
    for row,asset_id in enumerate(ids):
        entry=next(e for e in manifest['assets'] if e['asset_id']==asset_id)
        original=entry['name']
        y=80+row*445
        draw.text((20,y),f"{labels[asset_id]} | triangles {entry['triangles_before']} -> {entry['triangles_after']}",font=font,fill='white')
        for col,state in enumerate(('Before','After')):
            image=Image.open(OUT/(original+'-'+state+'.png')).convert('RGB').resize((410,410),Image.Resampling.LANCZOS)
            sheet.paste(image,(45+col*500,y+30))
    sheet.save(OUT/(category+'-Comparison.png'))

clips=('Idle','Walk','SkateBalance','Cheer','SeatedIdle')
sheet=Image.new('RGB',(1600,380),'#18202a');draw=ImageDraw.Draw(sheet)
draw.text((15,10),'D83 citizens | 20 bones | Blender animation pose tests',font=font,fill='white')
for i,clip in enumerate(clips):
    image=Image.open(OUT/('SK_Citizen_Rig_Casual-'+clip+'.png')).convert('RGB').resize((300,300),Image.Resampling.LANCZOS)
    sheet.paste(image,(i*320+10,65));draw.text((i*320+12,40),clip,font=small,fill='white')
sheet.save(OUT/'Citizen-Poses.png')
overview=Image.new('RGB',(1000,990),'#18202a');draw=ImageDraw.Draw(overview)
draw.text((20,12),'D83 Blender prototypes - curved surfaces and park detail',font=font,fill='white')
for i,(file,label) in enumerate((
        ('SM_Building_WaterTower_A-After.png','Smooth water tower'),
        ('SM_Building_Service_A-After.png','Smooth water treatment cylinders'),
        ('SM_Building_Park_A-After.png','Symmetrical flat park crowns'),
        ('Skate-Ramp-Detail.png','Smooth skate ramp riding curve'))):
    x=20+(i%2)*500;y=60+(i//2)*460
    draw.text((x,y),label,font=small,fill='white')
    image=Image.open(OUT/file).convert('RGB').resize((430,430),Image.Resampling.LANCZOS)
    overview.paste(image,(x+15,y+25))
overview.save(OUT/'Overview.png')
frames=[Image.open(OUT/f'Walk-{i:02}.png').convert('RGB') for i in range(1,25)]
frames[0].save(OUT/'Citizen-Walk.gif',save_all=True,append_images=frames[1:],duration=1000/24,loop=0)
assert Image.open(OUT/'Citizen-Walk.gif').n_frames==24
sports=json.loads((ROOT/'Artifacts/D83/sports-preview.json').read_text())
def replay_cut(size,label='Replay - forward pass'):
    frame=Image.new('RGB',size,'#18202a');draw=ImageDraw.Draw(frame)
    draw.text((20,size[1]//2-12),label,font=font,fill='white')
    return frame

for record in sports['scenes']:
    clip=record['clip']
    frames=[Image.open(OUT/f'{clip}-{i:03}.png').convert('RGB') for i in range(1,record['period']+1,2)]
    durations=[1000/12]*len(frames)
    if record.get('playback')=='one_shot':
        frames.append(replay_cut(frames[0].size));durations.append(600)
    frames[0].save(OUT/(clip+'.gif'),save_all=True,append_images=frames[1:],duration=durations,loop=0)
    assert Image.open(OUT/(clip+'.gif')).n_frames==len(frames)
clips=('SkateCarve','BaseballSwing','SoccerKick')
montage=[]
for i in range(48):
    frame=Image.new('RGB',(1080,410),'#18202a');draw=ImageDraw.Draw(frame)
    draw.text((15,10),'D83 park activity tests - Blender only',font=font,fill='white')
    for col,clip in enumerate(clips):
        period=next(r['period'] for r in sports['scenes'] if r['clip']==clip)
        image=Image.open(OUT/f'{clip}-{(i*2)%period+1:03}.png').convert('RGB').resize((350,350),Image.Resampling.LANCZOS)
        frame.paste(image,(col*360+5,55));draw.text((col*360+10,35),clip,font=small,fill='white')
    montage.append(frame)
montage[0].save(OUT/'Park-Activities.gif',save_all=True,append_images=montage[1:],duration=1000/12,loop=0)
filmstrip=Image.new('RGB',(1280,1120),'#18202a');draw=ImageDraw.Draw(filmstrip)
draw.text((15,8),'D83 sports keyframes - board, bat and ball contact prototypes',font=font,fill='white')
for row,clip in enumerate(clips):
    period=next(r['period'] for r in sports['scenes'] if r['clip']==clip)
    y=42+row*358
    draw.text((15,y),clip,font=small,fill='white')
    for col in range(4):
        frame=1+(period//4)*col
        image=Image.open(OUT/f'{clip}-{frame:03}.png').convert('RGB').resize((315,315),Image.Resampling.LANCZOS)
        filmstrip.paste(image,(col*320+2,y+25))
filmstrip.save(OUT/'Sports-Keyframes.png')
tree=Image.open(OUT/'Tree-Design-Comparison.png').convert('RGB')
sheet=Image.new('RGB',(1000,710),'#18202a');sheet.paste(tree,(0,60));draw=ImageDraw.Draw(sheet)
draw.text((20,8),'Shared broadleaf design - matched height',font=font,fill='white')
draw.text((80,36),'Existing background tree',font=small,fill='white')
draw.text((580,36),'Pruned simple park tree',font=small,fill='white')
sheet.save(OUT/'Tree-Design-Labeled.png')
tricks=('SkateKickflip','Skate180','SkateLipStall')
sequence=[]
filmstrip=Image.new('RGB',(1280,1120),'#18202a');draw=ImageDraw.Draw(filmstrip)
draw.text((15,8),'D83 skate keyframes - forward kickflip, rolling 180 and rear-truck lip stall',font=font,fill='white')
sequence_durations=[]
for row,clip in enumerate(tricks):
    period=next(r['period'] for r in sports['scenes'] if r['clip']==clip)
    for frame_number in range(1,period+1,2):
        frame=Image.new('RGB',(480,540),'#18202a');label=ImageDraw.Draw(frame)
        label.text((15,10),clip,font=font,fill='white')
        caption='Forward travel 1.6m/s | keyframes' if clip!='SkateLipStall' else 'Rear-truck lip stall | keyframes'
        label.text((15,37),caption,font=small,fill='#ccd8e4')
        frame.paste(Image.open(OUT/f'{clip}-{frame_number:03}.png').convert('RGB'),(0,60))
        sequence.append(frame);sequence_durations.append(1000/12)
    sequence.append(replay_cut((480,540),'Next demonstration'));sequence_durations.append(500)
    y=42+row*358;draw.text((15,y),clip,font=small,fill='white')
    for col in range(4):
        t=(.25,.43,.65,.90)[col] if clip!='SkateLipStall' else col/4
        frame_number=1+2*int(t*period/2)
        image=Image.open(OUT/f'{clip}-{frame_number:03}.png').convert('RGB').resize((315,315),Image.Resampling.LANCZOS)
        filmstrip.paste(image,(col*320+2,y+25))
sequence[0].save(OUT/'Skate-Tricks.gif',save_all=True,append_images=sequence[1:],duration=sequence_durations,loop=0)
filmstrip.save(OUT/'Skate-Tricks-Keyframes.png')
sheet=Image.new('RGB',(1000,560),'#18202a');draw=ImageDraw.Draw(sheet)
draw.text((20,12),'Skateboard proportion - identical citizen and camera',font=font,fill='white')
draw.text((30,42),'Before: 1.265m',font=small,fill='white')
draw.text((530,42),'Now: 0.715m',font=small,fill='white')
for col,path in enumerate((ROOT/'Artifacts/D83/BeforeTreeAndTricks/SkateCarve-025.png',OUT/'SkateCarve-025.png')):
    sheet.paste(Image.open(path).convert('RGB'),(col*500+10,70))
sheet.save(OUT/'Skateboard-Size-Comparison.png')
print('CITYSIM_D83_PREVIEW_SHEETS_PASS 8 sheets; walk and 14 sports GIFs')
