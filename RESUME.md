# Continuing

Answer the owner in German (du). On the cabinet only copy files; start,
stop or test nothing without asking.

## The SDK

`external/VectorPie` is a clone of https://github.com/Goldfinch2/VectorPie
(ignored by git); the build takes `external/VectorPie/sdk` by itself when
no `-DVECTORPIE_SDK_DIR` is given. 0.1.3 was built at `b00d537` (SDK 0.9.0,
menu 1.1.13). If the folder is missing:

```sh
git clone https://github.com/Goldfinch2/VectorPie.git external/VectorPie
```

(The older SDK 0.1.0 in `F:\Tempest 2000 Port\vendor\VectorPie` is no
longer used.)

## Building (Git Bash)

PC (MSYS2 MinGW-w64):

```sh
export PATH=/c/msys64/mingw64/bin:$PATH
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc && (cd build/pc && ctest)
```

Pi (clang + lld, against the copy of the cabinet's sysroot from the
Tempest 2000 port):

```sh
export PATH=/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:/c/msys64/mingw64/bin:/usr/bin:$PATH
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release "-DVT_PI_SYSROOT=F:/Tempest 2000 Port/build/pi-sysroot"
cmake --build build/pi --target vpk
```

Then set the execute permission inside the package (Windows has none):
`vectortron/vectortron` in the `.vpk` to mode 0755 (Python `tarfile`, as in
`F:\Tempest 2000 Port\tools\make_vpk.sh`).

## Sound ROM and WAVs

```sh
unzip -o -j /d/Mame/roms/robotron.zip video_sound_rom_3_std_767.ic12 -d assets
mv assets/video_sound_rom_3_std_767.ic12 assets/video_sound_rom_1.ic12
cp /d/Mame/roms/robotron.zip build/pc/ && (cd build/pc && ./vt-sounds.exe robotron.zip ../../sounds/original; rm robotron.zip)
```

## The cabinet

`pi@192.168.0.122`, key `~/.ssh/id_ed25519_vectorpie`. The package goes to
`/persistent/games/packages/`, unpacked into `/persistent/games/vectortron/`
(`save/` there stays through updates).

## Releasing

For every new package: a GitHub release
(`gh release create v<version> build/pi/vectortron-<version>.vpk --title
"Vectortron <version>" --notes ...`, notes in English) and the same file in
the shared Google Drive folder `games` (uploaded through the owner's
Chrome; Drive replaces a file of the same name as a new version).

## Models

`backup/models-original-2026-10-16.c` is the full version of the models
before they were slimmed down (round 5 in STATUS.md); `vt-gallery
--windowed` shows them all.

## Next steps

- The package's execute permission into a script (`tools/make_vpk.sh` or
  the `vpk` target).
- Measure the frame rate on the board (with permission: `run.sh` sets
  `VP_STATS=1` and writes `run.log`).
- Play 0.1.3 on the cabinet: the cabinet's SOUND VOLUME now applies on top
  of the game's VOLUME; raise one of them if the sounds are too quiet.
