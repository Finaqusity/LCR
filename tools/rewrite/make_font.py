"""Rasterize the local monospaced font into the firmware's tiny ASCII atlas."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2]
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 9)
def raster(ch):
    canvas = Image.new('1', (6, 10))
    ImageDraw.Draw(canvas).text((0, -1), ch, font=font, fill=1)
    return [sum(int(canvas.getpixel((x, y))) << y for y in range(10)) for x in range(6)]

rows = []
for code in range(32, 127):
    columns = raster(chr(code))
    rows.append('    {' + ','.join(f'0x{v:03x}u' for v in columns) + '},')
# The ASCII atlas has no U+03A9; rasterize the same Consolas face so resistance
# units read kΩ/MΩ on the panel instead of spelling "OHM".
omega = raster('\u03a9')
target = root / 'firmware_next/include/lcr_font_data.h'
target.write_text('/* Generated bitmap glyphs; run tools/rewrite/make_font.py. */\n'
                  'static const uint16_t lcr_font[95][6] = {\n' + '\n'.join(rows) + '\n};\n'
                  '/* U+03A9 OHM SIGN, same 6x10 rasterizer as the ASCII glyphs. */\n'
                  'static const uint16_t lcr_font_omega[6] = {' +
                  ','.join(f'0x{v:03x}u' for v in omega) + '};\n', encoding='utf-8')
