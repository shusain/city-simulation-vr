"""Compose labeled review sheets from the project's Blender renders."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/ArtPreview/D79'
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',26)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
sheet=Image.new('RGB',(1500,1120),(27,33,42))
draw=ImageDraw.Draw(sheet)
columns=(('High-density residential','SM_Building_Residential_High_A'),
         ('Basic power','SM_Building_Power_A'),('Water treatment','SM_Building_Service_A'))
for col,(label,name) in enumerate(columns):
    draw.text((col*500+20,14),label,font=font,fill=(235,241,248))
    for row,state in enumerate(('Before','After')):
        tile=Image.open(out/(name+'-'+state+'.png')).convert('RGB').resize((480,480),Image.Resampling.LANCZOS)
        y=55+row*530
        sheet.paste(tile,(col*500+10,y))
        draw.text((col*500+20,y+485),state,font=small,fill=(235,241,248))
sheet.save(out/'Building-Comparison.png')

citizen=root/'Artifacts/ArtPreview/D71/Citizen-Comparison.png'
sheet=Image.open(citizen).convert('RGB')
draw=ImageDraw.Draw(sheet)
draw.rectangle((0,0,1600,62),fill=(27,33,42))
for x,label in ((90,'Current (height normalized)'),(510,'Casual prototype'),(865,'Office prototype'),(1220,'Worker prototype')):
    draw.text((x,18),label,font=small,fill=(235,241,248))
sheet.save(citizen.with_name('Citizen-Comparison-Labeled.png'))
surfaces=out/'Surfaces'
if (surfaces/'SM_ResidentialHigh_SurfacePrototype-Whole.png').exists():
    sheet=Image.new('RGB',(1500,580),(24,30,38));draw=ImageDraw.Draw(sheet)
    columns=(('Green-painted brick walls','Wall'),('Mechanical unit: paint + metal','Roof'),('Whole building','Whole'))
    for i,(title,suffix) in enumerate(columns):
        image=Image.open(surfaces/('SM_ResidentialHigh_SurfacePrototype-'+suffix+'.png'))
        sheet.paste(image.convert('RGB').resize((480,480),Image.Resampling.LANCZOS),(10+i*500,60))
        draw.text((12+i*500,16),title,fill=(226,234,242),font=small)
    draw.text((12,547),'D79 Blender surface prototype | enlarged brick | separate roof and equipment surfaces',fill=(176,189,204),font=small)
    sheet.save(surfaces/'Building-Surfaces-Preview.png')
    manifest=json.loads((root/'ArtSource/Manifests/city_building_surface_prototype.json').read_text(encoding='utf-8'))
    for damaged in (False,True):
        sheet=Image.new('RGB',(1500,1670),(24,30,38)); draw=ImageDraw.Draw(sheet)
        draw.text((20,12),'D79 brick building family'+(' — damaged variants' if damaged else ''),font=font,fill=(226,234,242))
        for col,density in enumerate(('Low','Medium','High')):
            draw.text((20+col*500,58),density+' density',font=small,fill=(190,206,222))
            for row,category in enumerate(('Residential','Commercial','Industrial')):
                e=next(e for e in manifest['assets'] if e.get('category')==category and e.get('density')==density and e.get('damaged')==damaged)
                y=95+row*515
                image=Image.open(surfaces/(Path(e['export']).stem+'-Whole.png')).convert('RGB')
                sheet.paste(image.resize((480,480),Image.Resampling.LANCZOS),(10+col*500,y))
                draw.text((20+col*500,y+480),category,font=small,fill=(226,234,242))
        draw.text((20,1640),'Blender previews | shared enlarged brick maps | original geometry and window masks',font=small,fill=(176,189,204))
        sheet.save(surfaces/('Building-Family'+('-Damaged' if damaged else '')+'.png'))
    focus=(('Medium commercial','SM_CommercialMedium_SurfacePrototype','Sign seated on podium roof'),
           ('Medium industrial','SM_IndustrialMedium_SurfacePrototype','Separate equipment; open upper vents'),
           ('High industrial','SM_IndustrialHigh_SurfacePrototype','More windows; open upper vents'))
    if all((surfaces/(name+'-Detail.png')).exists() for _,name,_ in focus):
        sheet=Image.new('RGB',(1500,1130),(24,30,38)); draw=ImageDraw.Draw(sheet)
        for col,(label,name,detail) in enumerate(focus):
            draw.text((20+col*500,14),label,font=font,fill=(226,234,242))
            for row,suffix in enumerate(('Whole','Detail')):
                image=Image.open(surfaces/(name+'-'+suffix+'.png')).convert('RGB')
                sheet.paste(image.resize((480,480),Image.Resampling.LANCZOS),(10+col*500,55+row*525))
            draw.text((20+col*500,1065),detail,font=small,fill=(226,234,242))
        draw.text((20,1100),'D79 Blender refinement previews | existing brick and metal materials',font=small,fill=(176,189,204))
        sheet.save(surfaces/'Roof-Windows-Preview.png')
print('CITYSIM_REFINEMENT_SHEETS_READY')
