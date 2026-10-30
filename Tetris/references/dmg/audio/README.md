# Original DMG music references

`music.asm`, `music_macros.asm` and `audio.asm` come from the pinned
[published disassembly](https://github.com/kaspermeerts/tetris/tree/b95c66859339f5523e80213de5857eefc1c7703f).
They supply original A/B/C score and instrument facts for the native CHGame
player. The generator emits only the three scores and their tables, with no
Game Boy executable code or ROM image. The software license does not relicense
Nintendo's music or arrangements; see the project's NOTICE.

`tools/make_music.py` verifies label addresses, resolves score pointers and
records channel events and independent loop lengths in `tools/music.json`.
That file also records source URLs and SHA-256 hashes. The native audio test
compares all four channels over three main loops, including B-TYPE's original
longer wave-channel loop.
