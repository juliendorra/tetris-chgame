# Audio update 1.1.6

The original A/B/C score and instrument definitions are retained. Both pulse parts receive a static output lift, playback uses the full master level, and a wider linear output range preserves normal note decays. Routine movement/rotation/landing cues no longer duck the soundtrack and are quieter. Major event cues use only a small music reduction.

Validation uses the actual C++ audio backend and native Rev0 builds. Digital
levels and source checks do not establish measured acoustic loudness or
physical timer margin. These releases retain each public repository's
existing build and save setup; workspace console builds also included a
separate SD-save overlay that is not part of this audio change.

## Release build validation

Built against the clean upstream SDK at
`e1f8d05940ddb7bbef89c13326da75c003f02904` with GCC 16.2.

```text
Tetris: build
flash (sections): 28411 B
image: 28712 B of 50944 (save pages free: 2; two need <= 50432)
static RAM: 15140 B of 18416 (stack 2048 B separate)
```

9 rendered audio files match the tested workspace PCM byte for byte.

All 66,816 source voice frames match over three score loops. The complete
host/simulator check passes, including all nine cues and unchanged DMG
controls, settings migration and independently reconstructed line clears.
Routine control-cue disturbance is 5.6% of music RMS at ten inputs/second.
