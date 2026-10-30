#!/usr/bin/env python3
"""Compare native captures against the independently reconstructed DMG screens."""
from pathlib import Path
import json
import re
import argparse
import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[1]
REF=ROOT/'references/dmg'
parser=argparse.ArgumentParser()
parser.add_argument('--native',action='store_true',help='Also compare the latest actual-firmware emulator capture')
args=parser.parse_args()
OUT=ROOT/'out/dmg-comparison';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'View.cpp').read_text().split('COLORS[3][4][3]={',1)[1].split('};',1)[0]
base=np.array(list(map(int,re.findall(r'\d+',source))),dtype=float).reshape(3,4,3)[0]
ramp=np.rint(np.stack([np.interp(np.arange(16),[0,5,10,15],base[:,c]) for c in range(3)],axis=1)).astype(int)
ramp=(ramp//16*17).astype(np.uint8)

def reference(name,score=None):
    image=Image.open(REF/f'{name}.png').convert('L')
    if score is not None:
        ImageDraw.Draw(image).rectangle((104,24,151,31),fill=255)
        atlas=Image.open(REF/'font.png').convert('L')
        digits=str(score)
        for n,ch in enumerate(digits):
            index=int(ch)
            tile=atlas.crop((index*8,0,index*8+8,8))
            image.paste(tile,((19-len(digits)+n)*8,24))
    src=np.asarray(image)
    shade=(255-src.astype(float))/85
    # An independent area-filter oracle: integer supersampling, then 5x5 means.
    high=shade.repeat(4,axis=0).repeat(4,axis=1)
    high=np.pad(high,((0,4),(0,0)),constant_values=3)
    index=np.rint(high.reshape(116,5,128,5).mean(axis=(1,3))*5).astype(int)
    target=np.tile(ramp[15],(128,128,1));target[6:122]=ramp[index]
    return Image.fromarray(target)

cases=[
    ('title','TitleScreenTilemap',[(8,112,16,120),(96,112,152,120)]),
    ('game-type','ConfigScreenTilemap',[(24,40,72,48),(24,96,136,120)]),
    ('level','TypeADifficultyTilemap',[(40,48,48,56)]),
    ('play','TypeAGameplayTilemap',[(16,0,96,144),(104,24,152,32),(112,56,144,64),
                                (112,80,152,88),(120,104,152,128)]),
]
results=[]
for screen,name,rects in cases:
    actual=Image.open(ROOT/f'out/verification/{screen}.png').convert('RGB').resize((128,128),Image.Resampling.NEAREST)
    expected=reference(name)
    mask=np.ones((128,128),dtype=bool)
    for x0,y0,x1,y1 in rects:
        x0=max(0,int(x0*.8)-1);x1=min(128,int(np.ceil(x1*.8))+1)
        y0=max(0,6+int(y0*.8)-1);y1=min(128,6+int(np.ceil(y1*.8))+1)
        mask[y0:y1,x0:x1]=False
    same=np.all(np.asarray(actual)==np.asarray(expected),axis=2)
    wrong=np.count_nonzero(mask & ~same);count=np.count_nonzero(mask)
    difference=np.asarray(actual).copy();difference[mask & ~same]=(255,0,255)
    Image.fromarray(difference).resize((384,384),Image.Resampling.NEAREST).save(OUT/f'{screen}-diff.png')
    expected.save(OUT/f'{screen}-reference.png')
    results.append({'screen':screen,'compared_pixels':int(count),'different_pixels':int(wrong),
                    'match_percent':round(100*(1-wrong/count),3)})
    assert not wrong,(screen,wrong,count)
    print(f'PASS {screen}: {count} static pixels match the DMG design exactly after area filtering')
(OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')

# The actual firmware emulator must preserve the same static DMG pixels.
native=ROOT/'out/emulator/native-play.png'
if args.native:
    assert native.exists(),'Run verify_firmware_emulator.py first'
    _,name,rects=cases[-1]
    actual=np.asarray(Image.open(native).convert('RGB'))
    # This emulator expands RGB565 for display; chsim expands RGB444.
    # Compare palette identities rather than their 1-3 unit RGB differences.
    distance=((actual[:,:,None,:].astype(int)-ramp[None,None,:,:].astype(int))**2).sum(axis=3)
    actual=ramp[distance.argmin(axis=2)]
    result=json.loads((ROOT/'out/emulator/results.json').read_text())
    expected=np.asarray(reference(name,result.get('score')))
    mask=np.ones((128,128),dtype=bool)
    for x0,y0,x1,y1 in rects:
        if (x0,y0,x1,y1)==(104,24,152,32) and 'score' in result:continue
        mask[max(0,6+int(y0*.8)-1):min(128,6+int(np.ceil(y1*.8))+1),
             max(0,int(x0*.8)-1):min(128,int(np.ceil(x1*.8))+1)]=False
    assert np.all(actual[mask]==expected[mask]),'Native firmware differs from the reference HUD'
    print('PASS native RISC-V firmware: DMG HUD, numeric score and borders match the reference')

# Tracked pre-revision captures make this comparison reproducible on a fresh checkout.
font=ImageFont.load_default(size=17)
sheet=Image.new('RGB',(1184,1660),(26,34,30));draw=ImageDraw.Draw(sheet)
for col,label in enumerate(('DMG reference tilemaps','Previous CHGame design','Revised CHGame design')):
    draw.text((12+col*394,10),label,font=font,fill=(225,235,218))
for row,(screen,name,_) in enumerate(cases):
    y=44+row*400
    images=[reference(name),Image.open(ROOT/f'references/previous/{screen}.png'),
            Image.open(ROOT/f'out/verification/{screen}.png')]
    for col,im in enumerate(images):
        sheet.paste(im.resize((384,384),Image.Resampling.NEAREST),(10+col*394,y))
    draw.text((12,y+384),screen.upper(),font=font,fill=(225,235,218))
sheet.save(ROOT/'docs/dmg-comparison.png')
print(ROOT/'docs/dmg-comparison.png')
