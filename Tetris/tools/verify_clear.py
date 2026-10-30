#!/usr/bin/env python3
"""Check every clear frame against the DMG flash / bottom-up VRAM sequence."""
from pathlib import Path
import json
import re
import sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'CHGame/tools/chsim'))
from chsim import build
from chdrivelib import Driver, SimTransport
from fbimage import to_image

REF = ROOT / 'references/dmg'
OUT = ROOT / 'out/line-clear'
OUT.mkdir(parents=True, exist_ok=True)

def tiles(name):
    im = Image.open(REF / name).convert('L')
    return [np.asarray(im.crop((x, y, x+8, y+8))) for y in range(0, im.height, 8)
            for x in range(0, im.width, 8)]

atlas = tiles('font.png')[:39] + tiles('copyrightandtitlescreen.png')[:9] + tiles('configandgameplay.png')
background = np.array(json.loads((REF / 'tilemaps.json').read_text())['TypeAGameplayTilemap']).reshape(18, 20)
source = (ROOT / 'View.cpp').read_text().split('COLORS[3][4][3]={', 1)[1].split('};', 1)[0]
colors = np.array(list(map(int, re.findall(r'\d+', source)))).reshape(3, 4, 3)[0]
ramp = np.rint(np.stack([np.interp(np.arange(16), [0, 5, 10, 15], colors[:, c]) for c in range(3)], axis=1))
ramp = (ramp.astype(int)//16*17).astype(np.uint8)

def block_tiles(board):
    result = np.full((18, 10), 0x2f)
    for y, row in enumerate(board):
        for x, value in enumerate(row):
            if not value:
                continue
            tile = [0x2f, 0x84, 0x81, 0x80, 0x83, 0x82, 0x86, 0x85][value]
            result[y, x] = tile
    return result

def expected_image(field):
    tilemap = background.copy()
    tilemap[:, 2:12] = field
    pixels = np.empty((144, 160))
    for y, row in enumerate(tilemap):
        for x, index in enumerate(row):
            pixels[y*8:y*8+8, x*8:x*8+8] = (255-atlas[index].astype(float))/85
    high = pixels.repeat(4, axis=0).repeat(4, axis=1)
    high = np.pad(high, ((0, 4), (0, 0)), constant_values=3)
    coverage = np.rint(high.reshape(116, 5, 128, 5).mean(axis=(1, 3))*5).astype(int)
    image = np.tile(ramp[15], (128, 128, 1))
    image[6:122] = ramp[coverage]
    return image

t = SimTransport(build(str(ROOT)))
d = Driver(t, 'TETRIS')
compared = 0
recording = []
checkpoints = {}
try:
    d.handshake()
    d.cmd('L1')
    for fixture in range(4, 9):
        d.cmd(f'Y{fixture}')
        d.frames(1)  # Lock: frame zero of the animation.
        state = list(map(int, d.query('H', 'TETRISSTATE').split()[1:]))
        assert state[1] == 1, state
        board = np.array(list(map(int, d.query('W', 'TETRISBOARD').split()[1]))).reshape(18, 10)
        full = np.all(board != 0, axis=1)
        assert full.sum() == (fixture-3 if fixture < 8 else 2)
        initial = block_tiles(board)
        initial[14:18, 4] = [0x80, 0x88, 0x88, 0x89]
        collapsed = np.concatenate((np.full((int(full.sum()), 10), 0x2f), initial[~full]))
        final_board = np.concatenate((np.zeros((int(full.sum()), 10), dtype=int), board[~full]))
        for elapsed in range(93):
            # Independent transcription of AnimateLineClear / hTimer1 and the
            # reverse-ordered PlayingFieldWipe19..02 VBlank calls. Frame 2 is
            # grey; 12 restores blocks; 22/32 and 42/52 repeat; 62 blanks;
            # 75..92 copy exactly one collapsed row, bottom first.
            field = initial.copy()
            if 2 <= elapsed < 62 and ((elapsed-2)//10) % 2 == 0:
                field[full] = 0x8c
            elif elapsed >= 62:
                field[full] = 0x2f
            if elapsed >= 75:
                first = 92-elapsed
                field[first:] = collapsed[first:]
            if elapsed == 92:
                field = collapsed.copy()
                state = list(map(int, d.query('H', 'TETRISSTATE').split()[1:]))
                # The last redraw also spawns the preview piece, as on DMG.
                masks = [0x1700, 0x4700, 0x0f00, 0x6600, 0x6300, 0x3600, 0x2700]
                kind, rotation, px, py = state[2:6]
                assert rotation == 0
                for cy in range(4):
                    for cx in range(4):
                        if masks[kind] & (1 << (cy*4+cx)) and 0 <= py+cy < 18:
                            tile = [0x84, 0x81, 0x8b, 0x83, 0x82, 0x86, 0x85][kind]
                            if kind == 2:
                                tile = [0x8a, 0x8b, 0x8b, 0x8f][cx]
                            field[py+cy, px+cx] = tile
            actual = np.asarray(to_image(d.shot(), 1).convert('RGB'))
            expected = expected_image(field)
            # The whole well and both borders, including fractional pixels at
            # the edges; exclude the independently changing sidebar/preview.
            wrong = np.any(actual[6:122, 11:79] != expected[6:122, 11:79], axis=2)
            if wrong.any():
                Image.fromarray(actual).save(OUT / f'actual-{fixture}-{elapsed}.png')
                Image.fromarray(expected).save(OUT / f'expected-{fixture}-{elapsed}.png')
            assert not wrong.any(), (fixture, elapsed, int(wrong.sum()))
            compared += wrong.size
            if fixture == 7:
                im = Image.fromarray(actual).resize((384, 384), Image.Resampling.NEAREST)
                recording.append(im)
                if elapsed in (0, 2, 12, 42, 62, 75, 84, 92):
                    checkpoints[elapsed] = im
            if elapsed < 92:
                d.frames(1)
        state = list(map(int, d.query('H', 'TETRISSTATE').split()[1:]))
        assert state[1] == 0 and state[9] == int(full.sum()), state
        actual_board = np.array(list(map(int, d.query('W', 'TETRISBOARD').split()[1]))).reshape(18, 10)
        assert np.array_equal(actual_board, final_board)
        print(f'PASS clear fixture {fixture}: all 93 frames, {full.sum()} lines, stable top-row/stack compaction')
finally:
    t.close()
assert not t.bugs, t.bugs
durations = [round((i+1)*100/60)*10-round(i*100/60)*10 for i in range(len(recording))]
durations[0] += 500
durations[-1] += 700
recording[0].save(ROOT / 'docs/line-clear.gif', save_all=True, append_images=recording[1:], duration=durations, loop=0)
sheet = Image.new('RGB', (4*384, 2*410), (26, 34, 30))
draw = ImageDraw.Draw(sheet)
for i, (elapsed, im) in enumerate(checkpoints.items()):
    x, y = (i % 4)*384, (i//4)*410
    sheet.paste(im, (x, y))
    draw.text((x+8, y+388), f'Clear frame {elapsed} ({elapsed/60:.3f}s)', fill='white')
sheet.save(OUT / 'sequence.png')
(OUT / 'results.json').write_text(json.dumps({'fixtures': 5, 'frames': 465, 'compared_pixels': compared, 'different_pixels': 0}, indent=2)+'\n')
print(f'PASS: {compared:,} animated well pixels match the independent DMG sequence; single/double/triple/Tetris and separated rows')
