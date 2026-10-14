import json
from pathlib import Path
from fontTools.ttLib import TTFont
from PIL import Image
r=Path(__file__).resolve().parents[1]
rows=json.loads((r/'translations/catalog.json').read_text())
chars={c for x in rows if x['kind']!='technical' for c in x['target'] if ord(c)>=128 and ord(c) not in (0x200b,0xfeff)}
cmap=set(TTFont(r/'fonts/fusion-pixel-12px-monospaced-zh_hans.ttf').getBestCmap())|set(TTFont(r/'fonts/NotoSansSC-Regular.ttf').getBestCmap())|set(TTFont(r/'fonts/NotoSansHebrew.ttf').getBestCmap())
im=Image.open(r/'source/installed_files/gfx/images/unicode-atlas.png')
missing=[c for c in chars if ord(c) not in cmap or ord(c)>65535]
blank=[c for c in chars if not c.isspace() and ord(c)<65536 and not im.crop(((ord(c)%256)*12,(ord(c)//256)*12,(ord(c)%256)*12+12,(ord(c)//256)*12+12)).getchannel('A').getbbox()]
out={'unique_non_ascii_characters':len(chars),'missing':missing,'blank_glyphs':blank}
(r/'verification/glyph-coverage.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print(out)
