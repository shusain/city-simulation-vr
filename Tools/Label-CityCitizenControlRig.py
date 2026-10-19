"""Compose D87 source render annotations and a sampled24fps demo GIF."""
from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D87/Preview'
views=json.loads((OUT/'Views.json').read_text())
def font(size):return ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',size)
WHITE=(239,245,250);MUTED=(174,194,213);BODY=(97,207,255);LEFT=(71,237,175);RIGHT=(255,195,102)
def labeled(case,detail=True):
    raw=Image.open(OUT/(case+'-raw.png')).convert('RGB')
    im=Image.new('RGB',(1480,1150),(19,29,43));im.paste(raw,(190,100));d=ImageDraw.Draw(im)
    d.text((35,22),'Citizen animation controls - '+('planted crouch' if case.startswith('Demo') else case.lower()),font=font(32),fill=WHITE)
    d.text((35,65),'Blender source render with actual control positions overlaid. Green: left; gold: right; blue: body.',font=font(20),fill=MUTED)
    positions={}
    for b in views[case]:
        name=b['name'];h=tuple(v+s for v,s in zip(b['head'],(190,100)));t=tuple(v+s for v,s in zip(b['tail'],(190,100)))
        if name.startswith('CTRL_'):
            color=LEFT if name.endswith('.L') else RIGHT
            x,y=h;radius=12 if '_pole.' in name else 18
            if '_pole.' in name:
                d.line(((x,y-radius),(x+radius,y),(x,y+radius),(x-radius,y),(x,y-radius)),fill=color,width=4)
            else:d.ellipse((x-radius,y-radius,x+radius,y+radius),outline=color,width=4)
            positions[name]=(h,color)
        elif name in ('root','pelvis','spine','chest','neck','head'):
            d.line((h,t),fill=BODY,width=3);positions[name]=(((h[0]+t[0])/2,(h[1]+t[1])/2),BODY)
    if detail:
        labels={'CTRL_hand.L':(1110,480),'CTRL_hand.R':(40,500),
                'CTRL_foot.L':(1110,920),'CTRL_foot.R':(40,970),
                'CTRL_elbow_pole.L':(1080,320),'CTRL_elbow_pole.R':(40,350),
                'CTRL_knee_pole.L':(1080,740),'CTRL_knee_pole.R':(40,760),'pelvis':(40,645),'head':(1080,210)}
        for name,(x,y) in labels.items():
            point,color=positions[name]
            text=name.replace('CTRL_','').replace('_pole',' pole')
            width=d.textlength(text,font=font(23));end=(x+width+10,y+15) if x<point[0] else (x-10,y+15)
            d.line((point,end),fill=color,width=2)
            d.rounded_rectangle((x-5,y-2,x+width+5,y+31),radius=4,fill=(19,29,43))
            d.text((x,y),text,font=font(23),fill=color)
    d.text((35,1090),'Move hands/feet to pose limbs; move diamonds to steer elbows/knees; lower pelvis to crouch.',font=font(23),fill=WHITE)
    d.text((35,1123),'Elbows: 145deg flex   |   Knees: 150deg flex   |   IK stretch off   |   Clean export: original20 bones',font=font(20),fill=MUTED)
    return im
for case in ('Neutral','Crouch','Reach','Step'):labeled(case).save(OUT/(case+'-Controls.png'))
frames=[labeled('Demo-'+str(f).zfill(2),False).resize((888,690)) for f in range(1,49,4)]
frames[0].save(OUT/'Planted-Crouch.gif',save_all=True,append_images=frames[1:],duration=[170,160,170]*4,loop=0)
print('CITYSIM_D87_LABELS_PASS 4 views;12-frame sampled demo GIF')
