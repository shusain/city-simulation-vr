"""Original deterministic D79 roof PBR maps and readable fictional shop atlases."""
from pathlib import Path
import hashlib,json,math
import numpy as np
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
out=root/'ArtSource/Textures/RoofsSigns'; out.mkdir(parents=True,exist_ok=True)
rng=np.random.default_rng(7911); records=[]
def save(name,pixels,role,wrap=True):
    path=out/(name+'.png')
    image=pixels if isinstance(pixels,Image.Image) else Image.fromarray(np.uint8(np.clip(pixels,0,1)*255+.5))
    image.convert('RGB').save(path)
    records.append(dict(file=path.relative_to(root).as_posix(),role=role,width=image.width,height=image.height,
                        wrap=wrap,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
def normal(height,tile):
    dx=(np.roll(height,-1,1)-np.roll(height,1,1))/(2*tile/height.shape[1])
    dy=(np.roll(height,-1,0)-np.roll(height,1,0))/(2*tile/height.shape[0])
    n=np.stack((-dx,-dy,np.ones_like(dx)),-1); n/=np.linalg.norm(n,axis=-1,keepdims=True)
    return n*.5+.5
roof_types=(('RoofShingles',4.8,1024),('RoofMembrane',8,512),('RoofMetal',4.8,512))
for name,tile,size in roof_types:
    y,x=np.mgrid[:size,:size]/size
    grain=rng.normal(0,.018,(size,size))
    if name=='RoofShingles':
        row=np.floor(y*12).astype(int); u=(x*8+(row%2)*.5)%1; v=(y*12)%1
        shingle=np.floor(x*8+(row%2)*.5).astype(int)%8
        shade=rng.uniform(-.09,.09,(12,8))[row,shingle]
        seam=np.maximum(np.exp(-np.minimum(u,1-u)**2/.0007),np.exp(-np.minimum(v,1-v)**2/.0005))
        gray=(.66+shade+grain)*(1-seam)+.45*seam; height=.002*(1-seam)+.00012*grain
        rough=.91+.02*grain; ao=1-.18*seam
    elif name=='RoofMembrane':
        u=(x*4)%1;v=(y*4)%1
        seam=np.maximum(np.exp(-np.minimum(u,1-u)**2/.00025),np.exp(-np.minimum(v,1-v)**2/.00025))
        broad=.04*np.sin(2*math.pi*x)*np.cos(4*math.pi*y)+.03*np.sin(6*math.pi*x+4*math.pi*y)
        gray=.66+grain+broad-.12*seam; height=.0008*seam+grain*.0004
        rough=.87+broad;ao=1-.10*seam
    else:
        u=(x*6)%1
        seam=np.exp(-np.minimum(u,1-u)**2/.0005)
        grain*=.4; gray=.72+grain-.08*seam+.025*np.cos(12*math.pi*x)
        height=.005*seam+.0002*np.cos(12*math.pi*x);rough=.57+grain;ao=1-.08*seam
    save(name+'_BaseColor',np.repeat(gray[...,None],3,-1),'sRGB neutral roof surface')
    save(name+'_Normal',normal(height,tile),'linear DirectX tangent normal')
    save(name+'_ORM',np.stack((ao,rough,np.zeros_like(gray)),-1),'linear R=AO G=roughness B=metallic(0); painted roof')

businesses=[('market','MARKET','fruit',(30,104,73)),('bakery','BAKERY','bread',(157,66,35)),
            ('cafe','CAFE','cup',(103,66,48)),('books','BOOKS','book',(42,73,139)),
            ('pharmacy','PHARMACY','cross',(22,111,119)),('repairs','REPAIRS','wrench',(175,106,30)),
            ('outfitters','OUTFITTERS','shirt',(98,66,136)),('tech','TECH','chip',(36,80,104))]
font_path='C:/Windows/Fonts/segoeuib.ttf'
def icon(draw,kind,cx,cy,s,color):
    def box(a,b,c,d):return (cx+a*s,cy+b*s,cx+c*s,cy+d*s)
    if kind=='fruit':
        draw.ellipse(box(-.7,-.4,.7,.8),fill=color);draw.line((cx,cy-.35*s,cx+.12*s,cy-.8*s),fill=color,width=int(s*.15))
        draw.ellipse(box(.1,-.85,.65,-.5),fill=color)
    elif kind=='bread':
        draw.rounded_rectangle(box(-.85,-.55,.85,.55),radius=int(s*.35),fill=color)
        for a in (-.4,0,.4):draw.line((cx+(a-.1)*s,cy-.25*s,cx+(a+.1)*s,cy+.12*s),fill=(90,47,31),width=int(s*.08))
    elif kind=='cup':
        draw.rounded_rectangle(box(-.65,-.45,.5,.55),radius=int(s*.13),fill=color)
        draw.arc(box(.25,-.4,.9,.5),270,90,fill=color,width=int(s*.15))
        draw.line((cx-.8*s,cy+.75*s,cx+.7*s,cy+.75*s),fill=color,width=int(s*.1))
        draw.line((cx-.1*s,cy-.65*s,cx+.05*s,cy-1*s),fill=color,width=int(s*.08))
    elif kind=='book':
        draw.polygon([(cx-.9*s,cy-.7*s),(cx,cy-.5*s),(cx+.9*s,cy-.7*s),(cx+.9*s,cy+.7*s),(cx,cy+.85*s),(cx-.9*s,cy+.7*s)],fill=color)
        draw.line((cx,cy-.4*s,cx,cy+.7*s),fill=(42,73,139),width=int(s*.08))
    elif kind=='cross':
        draw.rectangle(box(-.25,-.85,.25,.85),fill=color);draw.rectangle(box(-.85,-.25,.85,.25),fill=color)
    elif kind=='wrench':
        draw.line((cx-.6*s,cy+.65*s,cx+.4*s,cy-.4*s),fill=color,width=int(s*.3))
        draw.arc(box(-.1,-.95,.85,0),40,310,fill=color,width=int(s*.22));draw.ellipse(box(-.85,.4,-.4,.85),fill=color)
    elif kind=='shirt':
        draw.polygon([(cx-.3*s,cy-.8*s),(cx-.85*s,cy-.5*s),(cx-.6*s,cy),(cx-.4*s,cy-.2*s),(cx-.4*s,cy+.8*s),(cx+.4*s,cy+.8*s),(cx+.4*s,cy-.2*s),(cx+.6*s,cy),(cx+.85*s,cy-.5*s),(cx+.3*s,cy-.8*s),(cx,cy-.5*s)],fill=color)
    else:
        draw.rectangle(box(-.5,-.5,.5,.5),outline=color,width=int(s*.14))
        for t in (-.3,0,.3):
            draw.line((cx+t*s,cy-.8*s,cx+t*s,cy-.5*s),fill=color,width=int(s*.09));draw.line((cx+t*s,cy+.5*s,cx+t*s,cy+.8*s),fill=color,width=int(s*.09))
            draw.line((cx-.8*s,cy+t*s,cx-.5*s,cy+t*s),fill=color,width=int(s*.09));draw.line((cx+.5*s,cy+t*s,cx+.8*s,cy+t*s),fill=color,width=int(s*.09))
for tall in (False,True):
    height=512 if tall else 256;atlas=Image.new('RGB',(2048,height*2))
    for i,(key,label,kind,bg) in enumerate(businesses):
        tile=Image.new('RGB',(512,410 if tall else 256),bg); draw=ImageDraw.Draw(tile)
        draw.rounded_rectangle((12,12,500,tile.height-12),radius=15,outline=(241,230,203),width=7)
        icon(draw,kind,256 if tall else 90,130 if tall else 126,65 if tall else 54,(251,240,213))
        text_box=(35,252,477,365) if tall else (171,48,474,208)
        size=72 if tall else 64
        while True:
            font=ImageFont.truetype(font_path,size); bounds=draw.textbbox((0,0),label,font=font)
            if bounds[2]-bounds[0]<=text_box[2]-text_box[0] or size<25:break
            size-=1
        draw.text(((text_box[0]+text_box[2]-(bounds[2]-bounds[0]))/2,
                   (text_box[1]+text_box[3]-(bounds[3]-bounds[1]))/2-bounds[1]),label,font=font,fill=(251,240,213))
        if tall:tile=tile.resize((512,512),Image.Resampling.LANCZOS)
        atlas.paste(tile,((i%4)*512,(i//4)*height))
    save('StoreSigns_'+('Tall' if tall else 'Wide')+'_BaseColor',atlas,'sRGB opaque shop sign atlas',False)
for record in records:
    data=np.asarray(Image.open(root/record['file'])).astype(float)/255
    if 'Normal' in record['file']:assert np.max(abs(np.linalg.norm(data*2-1,axis=-1)-1))<.008
    if record['wrap']:
        interior=max(np.mean(abs(np.diff(data,axis=a))) for a in (0,1))
        seam=max(np.mean(abs(data[0]-data[-1])),np.mean(abs(data[:,0]-data[:,-1])))
        assert seam<max(.06,interior*4),record['file']
metadata=dict(task='D79',provenance='Original procedural roof maps and fictional shop graphics',
              normal_convention='DirectX; Blender flips green',roof_tiles_m={name:tile for name,tile,_ in roof_types},
              sign_grid=[4,2],signs=[dict(id=k,label=l,icon=i,color=c,atlas_index=n) for n,(k,l,i,c) in enumerate(businesses)],textures=records)
(out/'roofs_signs.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
print('CITYSIM_ROOF_SIGN_TEXTURES_PASS',len(records),'maps;',len(businesses),'signs')
