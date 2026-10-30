#!/usr/bin/env python3
"""Record real button-driven gameplay, with a small placement search as a player."""
from pathlib import Path
import re
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'CHGame/tools/chsim'))
from chsim import build
from chdrivelib import Driver,SimTransport,mask_of
from fbimage import to_image
from PIL import Image
from chcart.picture import convert as menu_picture

SHAPES=[]
for row in re.findall(r'\{(0x[^}]+)\}',(ROOT/'Game.cpp').read_text()):
    SHAPES.append([int(n,16) for n in re.findall(r'0x[0-9a-f]+',row)])
frames=[]
durations=[]
elapsed=0
quantized=0
t=SimTransport(build(str(ROOT)));d=Driver(t,'TETRIS')
def state():return list(map(int,d.query('H','TETRISSTATE').split()[1:]))
def run(count,key=''):
    global elapsed,quantized
    d.buttons(mask_of(key) if key else 0)
    for start in range(0,count,4):
        step=min(4,count-start)
        d.frames(step);frames.append(to_image(d.shot(),3))
        elapsed+=step
        target=round(elapsed*100/60)*10
        durations.append(target-quantized);quantized=target
def tap(key):run(1,key);run(1)
def fits(board,mask,x,y):
    return all(0<=x+cx<10 and 0<=y+cy<18 and not board[y+cy][x+cx]
               for cy in range(4) for cx in range(4) if mask&(1<<(cy*4+cx)))
def best(board,piece):
    choices=[]
    for r,mask in enumerate(SHAPES[piece]):
        for x in range(-3,10):
            if not fits(board,mask,x,1):continue
            y=1
            while fits(board,mask,x,y+1):y+=1
            b=[row[:] for row in board]
            for cy in range(4):
                for cx in range(4):
                    if mask&(1<<(cy*4+cx)):b[y+cy][x+cx]=1
            left=[row for row in b if not all(row)]
            cleared=18-len(left);left=[[0]*10 for _ in range(cleared)]+left
            heights=[];holes=0
            for col in range(10):
                top=next((row for row in range(18) if left[row][col]),18)
                heights.append(18-top)
                holes+=sum(not left[row][col] for row in range(top,18))
            cost=sum(heights)*.51+holes*1.7+sum(abs(a-b) for a,b in zip(heights,heights[1:]))*.25-cleared*.9
            choices.append((cost,r,x))
    return min(choices)[1:]
try:
    d.handshake();d.cmd('L1');run(90)
    tap('START');run(40);tap('A');run(40);tap('A');run(30);tap('A')
    for n in range(32):
        # Soft-drop enough to clear the spawn ceiling before rotating.
        run(10,'DOWN');run(1)
        s=state()
        if s[0]!=5:break
        text=d.query('W','TETRISBOARD').split()[1]
        board=[[int(v) for v in text[y*10:y*10+10]] for y in range(18)]
        r,x=best(board,s[2])
        # CHGame A turns left (increments Nintendo orientation), B turns right.
        if r==3:tap('B')
        else:
            for _ in range(r):tap('A')
        for _ in range(abs(x-state()[4])):tap('RIGHT' if x>state()[4] else 'LEFT')
        pieces=state()[10]
        for _ in range(30):
            run(4,'DOWN')
            if state()[10]!=pieces:break
        while state()[1] in (1,2):run(4)
        run(1)
        if n==15:to_image(d.shot(),3).save(ROOT/'docs/gameplay.png')
    tap('START');run(70);tap('START')
    d.cmd('Y7');run(105)
    d.cmd('Y2');run(170)
finally:t.close()
assert not t.bugs,t.bugs
ROOT.joinpath('docs').mkdir(exist_ok=True)
frames[0].resize((128,128),Image.Resampling.NEAREST).save(ROOT/'docs/cart.png')
png,_=menu_picture(ROOT/'docs/cart.png')
(ROOT/'docs/cart.png').write_bytes(png)
frames[0].save(ROOT/'docs/gameplay.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=True)
# A compact contact sheet of the revised DMG title, menu and playfield.
im=Image.new('RGB',(384*3,384))
for i,name in enumerate(('title','game-type','play')):
    im.paste(Image.open(ROOT/f'out/verification/{name}.png'),(384*i,0))
im.save(ROOT/'docs/preview.png')
print(f'{len(frames)} frames -> {ROOT}/docs/gameplay.gif')
