#!/usr/bin/env python3
"""Size generated artwork for all Steam slots; retain the original alpha."""
from pathlib import Path
from PIL import Image, ImageOps
import json,hashlib
R=Path(__file__).resolve().parents[1];a=R/'artwork'
sizes={'poster':(600,900),'grid':(920,430),'hero':(1920,620),'logo':(1280,640)}
for name,size in sizes.items():
 im=Image.open(a/f'{name}-original.png').convert('RGBA' if name=='logo' else 'RGB')
 if name=='logo':
  im.thumbnail(size,Image.Resampling.LANCZOS)
  dest=Image.new('RGBA',size);dest.alpha_composite(im,((size[0]-im.width)//2,(size[1]-im.height)//2));im=dest
 else: im=ImageOps.fit(im,size,method=Image.Resampling.LANCZOS)
 im.save(a/f'{name}.png')
# Use the game's own icon for the app identity.
im=Image.open(R/'source/icon/icon.ico').convert('RGBA');im.resize((256,256),Image.Resampling.NEAREST).save(a/'icon.png')
report={'generator':'built-in image_gen','fan_art':True,'prompt_file':'prompts.json','icon':'official v23.0.0 icon/icon.ico','slots':{name:{'size':list(Image.open(a/f'{name}.png').size),'sha256':hashlib.sha256((a/f'{name}.png').read_bytes()).hexdigest()} for name in [*sizes,'icon']}}
(a/'sources.json').write_text(json.dumps(report,indent=2)+'\n')
