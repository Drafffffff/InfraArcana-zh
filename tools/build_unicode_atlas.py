#!/usr/bin/env python3
"""Render Unicode BMP glyphs to a nearest-neighbour SDL atlas; no TTF runtime dependency."""
from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont
from fontTools.ttLib import TTFont
R=Path(__file__).resolve().parents[1]
base=R/'fonts/fusion-pixel-12px-monospaced-zh_hans.ttf'
fallback=R/'fonts/NotoSansSC-Regular.ttf'
f=ImageFont.truetype(str(base),12); fb=ImageFont.truetype(str(fallback),12)
cmap=TTFont(base).getBestCmap(); fcmap=TTFont(fallback).getBestCmap()
img=Image.new('RGBA',(3072,3072));d=ImageDraw.Draw(img)
for cp in sorted(set(cmap)|set(fcmap)):
 if cp<128 or cp>65535:continue
 x=(cp%256)*12;y=(cp//256)*12
 font=f if cp in cmap else fb
 # Use the bitmap font at its native 12 px. Noto fallback is aligned by its bbox.
 glyph=Image.new('L',(12,12));gd=ImageDraw.Draw(glyph)
 if cp in cmap:
  gd.text((0,0),chr(cp),font=font,fill=255)
 else:
  bbox=font.getbbox(chr(cp));gd.text((-bbox[0],-bbox[1]),chr(cp),font=font,fill=255)
  glyph=glyph.point(lambda p:255 if p>=100 else 0)
 white=Image.new('RGBA',(12,12),(255,255,255,255));white.putalpha(glyph)
 img.paste(white,(x,y))
out=R/'source/installed_files/gfx/images/unicode-atlas.png';img.save(out,optimize=True)
(R/'verification/unicode-atlas.json').write_text(json.dumps({'size':[3072,3072],'glyph_cell':[12,12],'bmp_glyph_count':len((set(cmap)|set(fcmap))&set(range(128,65536))),'fonts':[base.name,fallback.name]},indent=2)+'\n')
print(out)
