#!/usr/bin/env python3
"""Check the actual firmware, sound and steady gameplay rate in a CHGame emulator."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--emulator',type=Path,required=True)
args=parser.parse_args()
out=ROOT/'out/emulator';out.mkdir(parents=True,exist_ok=True)
binary=ROOT/'build/release/Tetris.ino.bin'
presses=['start@0.8:0.1','a@1.4:0.1','a@2.0:0.1','a@2.6:0.1']
for index,at in enumerate((3.0,5.6,8.2,10.8,13.4,16.0,18.6,21.2,23.8,26.4)):
    presses += [f'{"left" if index%2==0 else "right"}@{at}:0.9',f'down@{at+1.0}:1.0']
counts=[]
nm=ROOT.parent/'.toolchains/current/native/bin/riscv-none-elf-nm'
symbols=subprocess.check_output([str(nm),'-C',str(ROOT/'build/release/Tetris.ino.elf')],text=True)
game_address=int(re.search(r'^([0-9a-f]+) \w game$',symbols,re.M)[1],16)
# Fixed-width Game layout: 180 board bytes, uint32 random, uint32 score.
score_address=game_address+184
native_score=0
for seconds in (8,28):
    command=[str(args.emulator.resolve()),str(binary),str(seconds),str(out/f'{seconds}.ppm')]
    command+=['--mem',f'{score_address:x}','1']
    for press in presses:command+=['--press',press]
    if seconds==28:command+=['--wav',str(out/'native.wav')]
    r=subprocess.run(command,capture_output=True,text=True,check=True)
    (out/f'{seconds}.log').write_text(r.stdout+r.stderr)
    assert 'FAULT' not in r.stdout,r.stdout
    assert 'display on' in r.stdout,r.stdout
    counts.append(int(re.search(r'frames written (\d+)',r.stdout)[1]))
    if seconds==28:native_score=int(re.search(r'mem [0-9a-f]+ (\d+)',r.stdout)[1])
    im=Image.open(out/f'{seconds}.ppm')
    im.resize((384,384),Image.Resampling.NEAREST).save(out/f'firmware-{seconds}.png')
    if seconds==28:im.save(out/'native-play.png')
    print(r.stdout.strip())
fps=(counts[1]-counts[0])/20
assert 59<=fps<=60.5,('steady gameplay frame rate',fps)
result={'firmware_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
        'emulator':str(args.emulator),'measurement_seconds':[8,28],
        'frames_written':counts,'steady_gameplay_fps':fps,'cpu_faults':0,'score':native_score,
        'note':'Emulated 48 MHz CHGame, not a physical-device measurement.'}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'PASS: actual RISC-V firmware, {fps:.2f} fps steady gameplay, audio active, no CPU faults')
