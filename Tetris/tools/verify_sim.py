#!/usr/bin/env python3
"""Exercise actual buttons, menus, palettes, pause, line clears, ending and saves."""
from pathlib import Path
import sys
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'CHGame/tools/chsim'))
from chsim import build
from chdrivelib import Driver,SimTransport,mask_of
from fbimage import to_image

OUT=ROOT/'out/verification';OUT.mkdir(parents=True,exist_ok=True)
t=SimTransport(build(str(ROOT)));d=Driver(t,'TETRIS')
try:
    d.handshake();d.cmd('L1');d.frames(2)
    def state():return list(map(int,d.query('H','TETRISSTATE').split()[1:]))
    def tap(key,settle=1):
        d.buttons(mask_of(key));d.frames(1);d.buttons(0);d.frames(settle)
    def shot(name):
        im=to_image(d.shot(),3);im.save(OUT/f'{name}.png');return im
    assert state()[0]==0
    enhanced=shot('title');tap('SELECT');classic=shot('classic');assert state()[15]==1
    assert len(enhanced.getcolors(1000))>4 and len(classic.getcolors(1000))<=4
    tap('SELECT');tap('SELECT');assert state()[15]==0
    tap('RIGHT');tap('RIGHT');tap('A');assert state()[0]==9;tap('B')
    tap('LEFT');tap('B');assert state()[0]==9;tap('B')
    tap('START');assert state()[0]==1;shot('game-type')
    tap('RIGHT');tap('RIGHT');assert state()[14]==1
    tap('LEFT');tap('LEFT');assert state()[14]==0
    tap('A');assert state()[0]==2;shot('music')
    tap('LEFT');assert state()[13]==0
    tap('RIGHT');assert state()[13]==1
    tap('RIGHT');assert state()[13]==1
    tap('DOWN');assert state()[13]==3;shot('music-off')
    tap('LEFT');assert state()[13]==2;shot('music-c')
    tap('DOWN');assert state()[13]==2
    tap('UP');assert state()[13]==0;shot('music-a')
    tap('RIGHT');assert state()[13]==1;shot('music-b')
    tap('DOWN');assert state()[13]==3
    tap('UP');assert state()[13]==1
    tap('A');assert state()[0]==3;shot('level')
    tap('A');assert state()[0]==5
    # Let every rotation box (including the I's full height) clear the ceiling.
    d.frames(120);before=state();shot('play')
    tap('A');assert state()[3]==(before[3]+1)%4 and state()[4]==before[4]
    tap('B');assert state()[3]==before[3] and state()[4]==before[4]
    tap('LEFT');assert state()[4]==before[4]-1
    tap('RIGHT');assert state()[4]==before[4]
    tap('SELECT');assert state()[11]==0
    tap('SELECT');assert state()[11]==1
    tap('START');assert state()[0]==6
    pause=state();image=d.shot();d.frames(180)
    assert state()==pause and d.shot()==image
    tap('START');assert state()[0]==5;d.frames(60);assert state()[12]>pause[12]
    # A vertical bar lands between two other I pieces. Its visible pixels must
    # remain identical when it becomes locked background, rather than joining
    # the adjacent bars and switching to horizontal patterns.
    d.cmd('Y9');d.frames(1);assert state()[1]==0 and state()[5]==14
    falling=shot('bar-falling')
    d.frames(52);assert state()[1]==0
    d.frames(1);assert state()[1]==2
    landed=shot('bar-landed')
    assert np.array_equal(np.asarray(falling)[18:366,33:237],np.asarray(landed)[18:366,33:237])
    # A clean four-line well, with the real gravity / lock / clear animation.
    d.cmd('Y0');d.frames(2);assert state()[1]==1
    shot('tetris');d.frames(92);assert state()[8]==1200 and state()[9]==4
    d.cmd('Y1');d.frames(94);assert state()[7]==1 and state()[9]==13
    # B-Type win and high-score initials, using the same engine and save path.
    d.cmd('Y2');d.frames(94);assert state()[0]==7 and state()[1]==4
    shot('victory');d.frames(65);tap('A');assert state()[0]==10
    shot('name');tap('UP');tap('START');assert state()[0]==8
    shot('records');records_image=d.shot();tap('B');assert state()[0]==3
    for selected in range(4):
        d.cmd(f'Z{selected}');d.cmd('V');d.frames(2)
        assert state()[0]==0 and state()[13]==selected
        tap('START')
        if state()[14]!=1:tap('RIGHT')
        assert state()[14]==1
        tap('START');tap('SELECT');assert state()[0]==8
        shot('saved-records');assert d.shot()==records_image
    tap('B');tap('A');assert state()[0]==4
    tap('DOWN');shot('height');tap('UP')
    tap('RIGHT');tap('RIGHT');tap('A');assert state()[0]==5
    shot('b-type')
    d.cmd('Y3');d.frames(2);assert state()[0]==7 and state()[1]==3;shot('game-over')
finally:t.close()
assert not t.bugs,t.bugs
print('PASS: original A/B/C/OFF menu, palettes, controls, frozen pause/audio, stable I-piece landing patterns, Tetris, level-up, B-Type victory, initials, namespaced settings persistence with scores retained; no torn frames')
