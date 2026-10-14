#!/usr/bin/env python3
"""Apply Luna's catalog to player-facing literal groups and data, before dynamic composition."""
from pathlib import Path
import argparse,ast,json,re,collections,subprocess,xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[1];S=R/'source'
TOKEN=re.compile(r'''//[^\n]*|/\*.*?\*/|'(?:\\(?:x[0-9A-Fa-f]+|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8}|[0-7]{1,3}|.)|[^'\\\r\n])'|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|R"([^ (\\\t\r\n]*)\((.*?)\)\1"''',re.S)
STRING=re.compile(r'''(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|R"([^ (\\\t\r\n]*)\((.*?)\)\1"''',re.S)
def decode(token):
 if token.startswith('R"'):
  return token[token.index('(')+1:token.rindex(')')]
 token=token[token.index('"'):]
 try:return ast.literal_eval(token)
 except (ValueError,SyntaxError):return token[1:-1]
def literals(text):
 tokens=list(TOKEN.finditer(text));i=0
 while i<len(tokens):
  m=tokens[i];i+=1
  if not STRING.fullmatch(m.group()):continue
  start,end=m.span();value=decode(m.group())
  while i<len(tokens):
   n=tokens[i];gap=text[end:n.start()]
   if gap.strip():break
   # Adjacent comments between literals are legal C++.
   if n.group().startswith(('//','/*')):
    j=i+1
    while j<len(tokens) and tokens[j].group().startswith(('//','/*')):j+=1
    if j<len(tokens) and not re.sub(r'//[^\n]*|/\*.*?\*/','',text[end:tokens[j].start()],flags=re.S).strip() and STRING.fullmatch(tokens[j].group()):
     i=j;n=tokens[i]
    else:break
   if not STRING.fullmatch(n.group()):break
   value+=decode(n.group());end=n.end();i+=1
  yield start,end,value

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--catalog',type=Path,default=R/'translations/catalog.json');args=ap.parse_args()
 rows=json.loads(args.catalog.read_text());by_path=collections.defaultdict(dict);invalid=[];allowed=collections.defaultdict(set)
 for row in rows:
  for loc in row['locations']:by_path.setdefault(loc.split(':',1)[0],{})
  for loc in row.get('translatable_locations',row['locations']):
   parts=loc.split(':',2)
   if len(parts)>1 and parts[1].isdigit():allowed[(parts[0],row['source'])].add(int(parts[1]))
 for row in rows:
  if row['kind']=='technical' or not row.get('target') or row['source']==row['target']:continue
  src,tgt=row['source'],row['target']
  # Preserve boundary whitespace automatically. Chinese punctuation may replace English punctuation.
  lead=re.match(r'^\s*',src).group();trail=re.search(r'\s*$',src).group()
  tgt=lead+tgt.strip()+trail
  if collections.Counter(re.findall(r'\{[^{}]+\}',src))!=collections.Counter(re.findall(r'\{[^{}]+\}',tgt)):
   invalid.append({'source':src,'reason':'format tags changed'});continue
  for loc in row['locations']:by_path[loc.split(':',1)[0]][src]=tgt
 has_locations=any('translatable_locations' in row for row in rows)
 counts={};applied=set()
 for rel,mapping in by_path.items():
  p=S/rel
  if not p.exists():continue
  if not mapping or rel in {'src/actor_data.cpp','src/xml.cpp'}:
   # These modules contain XML schema vocabulary and diagnostic strings only.
   original_bytes=subprocess.check_output(['git','-C',str(R),'show',f'unicode-baseline:source/{rel}'])
   if p.read_bytes()!=original_bytes:p.write_bytes(original_bytes)
   continue
  text=subprocess.check_output(["git","-C",str(R),"show",f"unicode-baseline:source/{rel}"]).decode("utf-8",errors="replace");n=0
  if p.suffix in {'.cpp','.hpp','.h','.cc','.c'}:
   changes=[];ordinal=collections.Counter();original_lines=collections.defaultdict(list)
   original=subprocess.check_output(['git','-C',str(R),'show',f'upstream-v23.0.0:source/{rel}']).decode('utf-8',errors='replace')
   for a0,b0,v0 in literals(original):original_lines[v0].append(original.count('\n',0,a0)+1)
   for a,b,src in literals(text):
    index=ordinal[src];ordinal[src]+=1
    if src in mapping and has_locations:
     lines=original_lines[src]
     if index>=len(lines) or lines[index] not in allowed[(rel,src)]:continue
    if src in mapping:
     # Never rewrite preprocessor include names even if they collide with a UI string.
     line=text[text.rfind('\n',0,a)+1:a]
     if re.match(r'\s*#\s*include\b',line):continue
     if rel=='src/config.cpp' and re.search(r'(?:ini|config)\s*\[\s*$',line):continue
     if rel=='src/paths.cpp' and re.fullmatch(r'[a-z_./]+',src):continue
     changes.append((a,b,json.dumps(mapping[src],ensure_ascii=False)));applied.add(src)
   for a,b,target in reversed(changes):text=text[:a]+target+text[b:]
   n=len(changes)
  elif rel=='installed_files/data/monsters.xml':
   root=ET.fromstring(text)
   for node in root.findall('./monster/text/*'):
    if node.tag.startswith('allow_'):continue
    if node.text in mapping:
     src=node.text;node.text=mapping[src];applied.add(src);n+=1
   text='<!-- Derived from official Infra Arcana v23.0.0; AGPL-3.0-or-later. -->\n'+ET.tostring(root,encoding='unicode')+'\n'
   text=re.sub(r'(?m)^[ \t]+$', '', text)
  elif p.suffix=='.txt' and not p.name.startswith('LICENSE') and '/data/map/' not in rel:
   out=[]
   for line in text.splitlines(keepends=True):
    src=line.strip()
    if src in mapping:
     prefix=line[:len(line)-len(line.lstrip())];suffix=line[len(line.rstrip()):]
     line=prefix+mapping[src].strip()+suffix;applied.add(src);n+=1
    out.append(line)
   text=''.join(out)
  else:continue
  if p.read_text()!=text:p.write_text(text)
  counts[rel]=n
 from patch_runtime import apply as patch_runtime
 patch_runtime()
 expected={row['source'] for row in rows if row['kind']!='technical' and row.get('target') and row['source']!=row['target']}
 report={'unapplied_sources':sorted(expected-applied),'applied_unique_sources':len(applied),'replacements':sum(counts.values()),'files':counts,'rejected':invalid}
 (R/'verification/apply-catalog.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='files'},ensure_ascii=False))
 if invalid:raise SystemExit(1)
if __name__=='__main__':main()
