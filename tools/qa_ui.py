#!/usr/bin/env python3
"""Run only on Bazzite, in the private Xvfb :99 QA display."""
from pathlib import Path
import os,subprocess,time,argparse
R=Path.home()/'Games/infra-arcana-zh';E={**os.environ,'DISPLAY':':99'}
def run(*args):return subprocess.check_output(args,env=E,text=True)
def key(k):subprocess.run(['xdotool','key','--clearmodifiers',k],env=E,check=True);time.sleep(.7)
def shot(name):subprocess.run(['import','-window','root',str(R/'verification'/f'{name}.png')],env=E,check=True)
def keys(seq):
 for k in seq.split():key(k)
ap=argparse.ArgumentParser();ap.add_argument('phase');a=ap.parse_args()
time.sleep(1.5)
wid=run('xdotool','search','--class','^ia$').splitlines()[0]
subprocess.run(['xdotool','windowfocus',wid],env=E,check=True)
if a.phase=='menus':
 shot('zh-main-4k');key('o');shot('zh-options-4k')
 for k,label in [('v','video'),('a','audio'),('i','input'),('g','gameplay')]:
  key(k);shot('zh-'+label+'-4k');keys('Down Down Down Down Down Down Down Down Down Down Down Down Down Down');key('Escape')
 key('Escape');key('t');shot('zh-manual-4k');keys('Down Down Down Down Down Down Down Down Down Down Down Down');key('Return');shot('zh-manual-page-4k');key('Escape');key('Escape')
 key('n');shot('zh-background-4k')
 for _ in range(5):key('Down')
 shot('zh-background-other-4k');keys('Up Up Up Up Up Return');shot('zh-name-or-traits-4k')
elif a.phase=='capture':shot('zh-current-4k')
