"""Label the matched Blender views with measured source-space dimensions."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale/Preview'
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',n)
canvas=Image.new('RGB',(1600,650),(20,30,44));draw=ImageDraw.Draw(canvas)
for i,(name,title) in enumerate((('Goal-Before','Original goal: 6.0m wide / 2.4m post height'),('Goal-After','Adjusted goal: 4.8m wide / 2.0m post height'))):
    frame=Image.open(OUT/(name+'.png')).convert('RGB').resize((800,567));canvas.paste(frame,(i*800,48))
    draw.text((i*800+16,12),title,font=font(22),fill=(235,243,252))
draw.text((16,621),'Matched Blender views | same approved keeper pose | citizen neutral height 1.43m | shared game transform preserves ratios',font=font(18),fill=(174,196,216))
canvas.save(OUT/'Goal-Scale-Comparison.png')
print('CITYSIM_D88_SCALE_COMPARISON_PASS')
