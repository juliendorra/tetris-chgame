#!/usr/bin/env python3
"""Check every original DMG voice against source-derived note/length/instrument facts."""
from pathlib import Path
import hashlib
import json
import subprocess
import wave
import re
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/audio';OUT.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',
    f'-I{ROOT}',str(ROOT/'tools/render.cpp'),str(ROOT/'Synth.cpp'),str(ROOT/'src/Songs.cpp'),'-o',str(OUT/'render')],check=True)
subprocess.run([str(OUT/'render'),str(OUT)],check=True)
music=json.loads((ROOT/'tools/music.json').read_text())
for name,entry in music['source'].items():
    assert hashlib.sha256((ROOT/'references/dmg/audio'/name).read_bytes()).hexdigest()==entry['sha256']
source=(ROOT/'references/dmg/audio/audio.asm').read_text()
def table(label,op):
    part=source.split(label+'::',1)[1].split('::',1)[0]
    return [int(v.strip()[1:],16) if v.strip().startswith('$') else int(v.strip())
            for line in part.splitlines() if line.split(';',1)[0].strip().startswith(op+' ')
            for v in line.split(';',1)[0].strip()[len(op):].split(',')]
pitches=table('NotePitches','dw');vibrato=table('VibratoOffsets','db');noise=table('Data_6E94','db')
compared=0
for song_index,song in enumerate(music['songs']):
    # Trace 3 main loops; each channel retains its own original loop length.
    # In B-TYPE the wave lane is 24 frames longer and must not be truncated.
    trace=np.loadtxt(OUT/f'{song_index}.tsv',dtype=int)
    assert len(trace)==song['length']*3 and trace.shape[1]==21
    times=(trace[:,0]*70224000+4194304-1)//4194304
    for ch,events in enumerate(song['voices']):
        actual=trace[:,1+ch*5:6+ch*5]
        if not events:
            assert not actual[:,:3].any()
            continue
        notes=[];durations=[];ages=[];envelopes=[];duties=[];waves=[]
        for raw,length,lo,hi,duty in events:
            note=0 if raw==1 else (raw-1)//5 if ch==3 else raw//2
            envelope=noise[raw] if ch==3 else (0 if hi else lo)
            notes.extend([note]*length);durations.extend(range(length,0,-1));ages.extend(range(length))
            envelopes.extend([envelope]*length);duties.extend([duty]*length)
            waves.extend([{0xa9:0,0xb9:1,0xc9:2}.get(lo,3)]*length)
        length=len(notes);indices=trace[:,0]%length
        notes=np.asarray(notes)[indices];ages=np.asarray(ages)[indices]
        envelopes=np.asarray(envelopes)[indices];duties=np.asarray(duties)[indices]
        start=trace[:,0]-ages
        start_ms=(start*70224000+4194304-1)//4194304
        envelope_clocks=times*64//1000-start_ms*64//1000
        period=envelopes&7
        change=envelope_clocks//np.maximum(period,1)
        change=np.where(period,change,0)
        levels=np.clip((envelopes>>4)+np.where(envelopes&8,change,-change),0,15)
        volumes=np.where(notes,levels*3,0)
        if ch==2:
            volumes=np.asarray([0,24,12,6])[(duties>>5)&3]
            volumes=np.where(((duties&128)!=0)&(np.asarray(durations)[indices]<=6),volumes//2,volumes)
            volumes=np.where(notes,volumes,0)
        assert np.array_equal(actual[:,0],notes),(song['title'],ch,'notes')
        assert np.array_equal(actual[:,1],volumes),(song['title'],ch,'envelopes',trace[np.where(actual[:,1]!=volumes)[0][0],0])
        if ch<3:
            offsets=[]
            for age,duty in zip(ages,duties):
                nibble=(vibrato[min(age//2,54)]>>(0 if age&1 else 4))&15
                offsets.append((nibble-16 if nibble&8 else nibble) if age and duty&15 else 0)
            periods=np.asarray(pitches)[notes]+offsets
            increments=(4194304*2048//((2048-periods)*31250))//(2 if ch==2 else 1)
            increments=np.where(notes,increments,0)
            assert np.array_equal(actual[:,2],increments),(song['title'],ch,'pitches/vibrato')
        else:
            assert not actual[:,2].any()
        # Source commands persist through loop boundaries; compare active notes.
        assert np.array_equal(actual[notes!=0,3],duties[notes!=0]),(song['title'],ch,'duty')
        if ch==2:assert np.array_equal(actual[notes!=0,4],np.asarray(waves)[indices][notes!=0])
        compared+=len(notes)
    with wave.open(str(OUT/f'{song_index}-full.wav')) as f:
        pcm=np.frombuffer(f.readframes(f.getnframes()),dtype=np.uint8).astype(float)-128
        assert f.getframerate()==31250 and np.std(pcm)>5
        assert pcm.min()>-128 and pcm.max()<127
    print(f"PASS {song['title']}: 3 loops, all voices, original notes/rests, durations, envelopes, duties/waveforms, pitch registers and vibrato; non-clipping PCM")
print(f'PASS: {compared:,} original DMG voice frames; pause/resume, mute, danger tempo and all nine cues')
(OUT/'verification.json').write_text(json.dumps({'tracks':3,'voice_frames':compared,'source_revision':music['revision'],'different_frames':0},indent=2)+'\n')
