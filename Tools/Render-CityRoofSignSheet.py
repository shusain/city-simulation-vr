"""Labeled D79 review sheets, composed from fresh Blender roof/sign renders."""
from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1];out=root/'Artifacts/ArtPreview/D79/Surfaces'
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',26)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
def canvas(width,height,title):
    sheet=Image.new('RGB',(width,height),(24,30,38));draw=ImageDraw.Draw(sheet)
    draw.text((20,14),title,font=font,fill=(232,238,245));return sheet,draw
sheet,draw=canvas(1500,610,'D79 roof surfaces')
for i,(label,name) in enumerate((('Shingles / small homes','ResidentialLow'),('Weathered membrane / flat roofs','CommercialMedium'),('Painted metal panels / factories','IndustrialHigh'))):
    image=Image.open(out/('SM_'+name+'_SurfacePrototype-RoofSurface.png')).convert('RGB')
    sheet.paste(image.resize((480,480),Image.Resampling.LANCZOS),(10+i*500,60))
    draw.text((15+i*500,549),label,font=small,fill=(226,234,242))
draw.text((20,580),'Blender preview | shared tiling base color, normal and roughness maps',font=small,fill=(176,189,204))
sheet.save(out/'Roof-Textures-Preview.png')
metadata=json.loads((root/'ArtSource/Textures/RoofsSigns/roofs_signs.json').read_text())
sheet,draw=canvas(1600,930,'D79 store sign variety — eight fictional businesses')
for i,sign in enumerate(metadata['signs']):
    x=10+(i%4)*400;y=58+(i//4)*421
    image=Image.open(out/('SM_CommercialMedium_Store'+sign['id'].title()+'-Sign.png')).convert('RGB')
    sheet.paste(image.resize((380,380),Image.Resampling.LANCZOS),(x,y))
    draw.text((x+8,y+380),sign['label'].title(),font=small,fill=(226,234,242))
draw.text((20,902),'All eight signs have low, medium and high density variants, plus matching damaged buildings.',font=small,fill=(176,189,204))
sheet.save(out/'Store-Signs-Preview.png')
print('CITYSIM_ROOF_SIGN_SHEETS_PASS')
