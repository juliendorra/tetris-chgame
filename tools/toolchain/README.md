# Native RISC-V compiler for Apple silicon

The Apple silicon setup builds GCC 16.2 and binutils 2.47 locally, with the
WCH compressed instructions and fast-interrupt convention needed by CHGame.
This uses the same build recipe and patches as the tested workspace build.
It is a community compiler port, not an official GNU or WCH release.

Install Xcode Command Line Tools, Git, Python 3.12+, uv and Arduino CLI.
The compiler build also uses make, patch, curl and tar from PATH. Allow
several gigabytes of disk space and a substantially longer first setup.

From the repository root, `python3 tetris.py setup` installs CHGame board
package 0.3.0, then builds the compiler and selects it using checkout-local
Arduino overrides. To repeat just the compiler step:

```sh
python3 tetris.py native-toolchain
```

The recipe records pinned source URLs and SHA-256 hashes in
[build_current_gcc.py](../build_current_gcc.py). It builds the compiler,
binutils, libgcc, libstdc++ and a native Arduino ctags helper from source.
The existing board package supplies its RISC-V C/printf runtime. The script
keeps downloads and builds under `.toolchains/current/`; no compiler binary
or vendor runtime is redistributed in this repository.

The build script uses the [MIT license](LICENSE-BUILD-SCRIPTS). The three
GNU-derived compiler/binutils patches retain [GPLv3](patches/COPYING3).
Downloaded upstream sources retain their own licenses.

Host tests need only the Python dependencies and clang++; they do not need
this compiler. Builds on other hosts use the board package's compiler and
have not been validated here. This repository does not trigger hosted CI.
