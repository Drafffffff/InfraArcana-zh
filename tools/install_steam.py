#!/usr/bin/env python3
"""Add/update only Infra Arcana Chinese's non-Steam shortcut and all five artwork slots.
Run on the target user after Steam has exited; preserve all other entries.
"""
from pathlib import Path
import struct,zlib,json,subprocess,shutil,datetime
h=Path.home();game=h/'Games/infra-arcana-zh';config=h/'.local/share/Steam/userdata/146975941/config';path=config/'shortcuts.vdf'
if subprocess.run(['pgrep','-x','steam'],stdout=subprocess.DEVNULL).returncode==0:
 raise SystemExit('Steam is running. Exit it normally before changing shortcuts.vdf.')
def parse(data,pos=0):
 out={}
 while pos<len(data):
  kind=data[pos];pos+=1
  if kind==8:return out,pos
  end=data.index(0,pos);key=data[pos:end].decode();pos=end+1
  if kind==0:value,pos=parse(data,pos)
  elif kind==1:
   end=data.index(0,pos);value=data[pos:end].decode();pos=end+1
  elif kind==2:value=struct.unpack_from('<I',data,pos)[0];pos+=4
  else:raise ValueError((kind,key,pos))
  out[key]=value
 return out,pos
def encode(obj):
 b=bytearray()
 for key,value in obj.items():
  kind=0 if isinstance(value,dict) else 1 if isinstance(value,str) else 2
  b.append(kind);b.extend(key.encode()+b'\0')
  if kind==0:b.extend(encode(value))
  elif kind==1:b.extend(value.encode()+b'\0')
  else:b.extend(struct.pack('<I',value & 0xffffffff))
 b.append(8);return bytes(b)
old=path.read_bytes() if path.exists() else b'\x00shortcuts\x00\x08\x08'
root,_=parse(old);shortcuts=root.setdefault('shortcuts',{})
exe=f'"{game}/game/launch.sh"';name='Infra Arcana 中文版';appid=zlib.crc32((exe+name).encode())|0x80000000
entry={'appid':appid,'AppName':name,'Exe':exe,'StartDir':f'"{game}/game"','icon':str(game/'artwork/icon.png'),'ShortcutPath':'','LaunchOptions':'','IsHidden':0,'AllowDesktopConfig':1,'AllowOverlay':1,'OpenVR':0,'Devkit':0,'DevkitGameID':'','DevkitOverrideAppID':0,'LastPlayTime':0,'FlatpakAppID':'','tags':{'0':'中文'}}
key=next((k for k,v in shortcuts.items() if v.get('Exe')==exe),None)
if key is None:key=str(max([int(k) for k in shortcuts]+[-1])+1)
else:
 existing=shortcuts[key];entry={**existing,**entry};entry['LastPlayTime']=existing.get('LastPlayTime',0)
others={k:v for k,v in shortcuts.items() if k!=key};shortcuts[key]=entry
new=encode(root);decoded,_=parse(new)
assert {k:v for k,v in decoded['shortcuts'].items() if k!=key}==others
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S');backup=game/'installation-records'/stamp;backup.mkdir(parents=True)
(backup/'shortcuts.vdf.before').write_bytes(old)
config.mkdir(parents=True,exist_ok=True);temp=path.with_suffix('.vdf.tmp');temp.write_bytes(new);temp.replace(path)
grid=config/'grid';grid.mkdir(exist_ok=True)
slots={'grid':f'{appid}.png','poster':f'{appid}p.png','hero':f'{appid}_hero.png','logo':f'{appid}_logo.png','icon':f'{appid}_icon.png'}
for source,target in slots.items():shutil.copy2(game/'artwork'/f'{source}.png',grid/target)
(grid/f'{appid}.json').write_text(json.dumps({'nVersion':1,'logoPosition':{'pinnedPosition':'CenterCenter','nWidthPct':60,'nHeightPct':70}}))
result={'entry':name,'appid':appid,'steam_game_id':(appid<<32)|0x02000000,'installed':str(game),'config':str(path),'artwork':slots,'existing_shortcuts_preserved':len(others),'backup':str(backup)}
(game/'steam-install.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
