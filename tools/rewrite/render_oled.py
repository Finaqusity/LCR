"""Render the actual C-generated OLED framebuffers, without redrawing the UI."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

root=Path(__file__).resolve().parents[2]
source=root/'firmware_next/build-host'
screens=[('oled_series','SERIES / RUN'),('oled_parallel','PARALLEL / HOLD'),
         ('oled_calibration','CALIBRATION CONFIRM'),('oled_error','ACQUISITION ERROR'),
         ('oled_wire_180','WIRE DATA (ROTATED 180 DEG)')]
image=Image.new('RGB',(1100,1020),'#121821')
draw=ImageDraw.Draw(image)
font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',18)
for i,(name,label) in enumerate(screens):
    x=24+(i%2)*550;y=20+(i//2)*335
    draw.text((x,y),label,font=font,fill='#b6c9dc')
    frame=ImageOps.invert(Image.open(source/f'{name}.pbm').convert('L')).convert('RGB').resize((512,256),Image.Resampling.NEAREST)
    image.paste(frame,(x,y+34))
image.save(root/'docs/rewrite/oled_preview.png')
image.crop((0,0,1100,335)).save(root/'docs/rewrite/oled_models.png')
