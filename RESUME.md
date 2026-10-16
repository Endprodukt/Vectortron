# Weitermachen

Antworten an den Besitzer auf Deutsch (du). Auf dem Automaten nur Dateien
kopieren; nichts starten, stoppen oder testen, ohne zu fragen.

## Bauen (Git Bash)

PC (MSYS2 MinGW-w64), SDK aus dem Tempest-2000-Port:

```sh
export PATH=/c/msys64/mingw64/bin:$PATH
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 "-DVECTORPIE_SDK_DIR=F:/Tempest 2000 Port/vendor/VectorPie/sdk"
cmake --build build/pc && (cd build/pc && ctest)
```

Pi (clang + lld, gegen die Sysroot-Kopie des Automaten aus dem Tempest-2000-Port):

```sh
export PATH=/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:/c/msys64/mingw64/bin:/usr/bin:$PATH
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release "-DVT_PI_SYSROOT=F:/Tempest 2000 Port/build/pi-sysroot" "-DVECTORPIE_SDK_DIR=F:/Tempest 2000 Port/vendor/VectorPie/sdk"
cmake --build build/pi --target vpk
```

Danach das Ausführungsrecht im Paket setzen (Windows kennt es nicht):
`vectortron/vectortron` im `.vpk` auf Modus 0755 (Python `tarfile`, wie in
`F:\Tempest 2000 Port\tools\make_vpk.sh`).

## Sound-ROM und WAVs

```sh
unzip -o -j /d/Mame/roms/robotron.zip video_sound_rom_3_std_767.ic12 -d assets
mv assets/video_sound_rom_3_std_767.ic12 assets/video_sound_rom_1.ic12
cp /d/Mame/roms/robotron.zip build/pc/ && (cd build/pc && ./vt-sounds.exe robotron.zip ../../sounds/original; rm robotron.zip)
```

## Automat

`pi@192.168.0.122`, Schlüssel `~/.ssh/id_ed25519_vectorpie`. Paket nach
`/persistent/games/packages/`, ausgepackt in `/persistent/games/vectortron/`
(`save/` dort bleibt bei Updates stehen).

## Modelle

`backup/models-original-2026-10-16.c` ist die volle Fassung der Modelle vor
dem Abspecken (Runde 5 in STATUS.md); `vt-gallery --windowed` zeigt alle.

## Nächste Schritte

- Paket-Rechte in ein Skript (`tools/make_vpk.sh` oder ins `vpk`-Ziel).
- Bildrate auf dem Board messen (mit Erlaubnis: `run.sh` setzt `VP_STATS=1`,
  schreibt `run.log`) und ab Werk 40 Hz bestätigen oder anpassen.
