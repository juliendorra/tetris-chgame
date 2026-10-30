# DMG visual references

These atlases and screen maps are the graphical reference for the CHGame
adaptation. The original artwork is Nintendo's 1989 Game Boy Tetris artwork.
It is not covered by this project's BSD software license.

The source is the published [Tetris disassembly](https://github.com/kaspermeerts/tetris/tree/b95c66859339f5523e80213de5857eefc1c7703f)
at revision `b95c66859339f5523e80213de5857eefc1c7703f`. `sources.json` records
the three atlas URLs and SHA-256 hashes; `tilemaps.json` contains the six
screen maps normalized from that revision's `tetris.asm`.

`tools/make_dmg_assets.py` reconstructs the 160×144 reference images and
generates the device's compact two-bit assets. These are background tilemap
reconstructions, so they omit the original game's dynamic OAM sprites,
counters and cursors. `tools/verify_dmg.py` excludes those changing regions
when comparing the native 128×128 captures.

The reconstruction was visually checked against this
[DMG title capture](https://images.nintendolife.com/00b6d3e81d84b/tetris-2.900x.jpg)
and [NintendoComplete's recorded playthrough](https://www.youtube.com/watch?v=BQwohHgrk2s)
(the title around 0:16 and gameplay around 7:04). The original Game Boy ROM
was not executed or bundled. The separate native-firmware check executes
CHGame RISC-V firmware in the workspace's CHGame emulator.
