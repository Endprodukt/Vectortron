# Status

Vectortron, taken from https://github.com/endprodukt/vectortron (branch
`ccr-9894316f-dbxip2`, at `e455e0a`), locally in `F:\Vectorton`.

## 0.1.3: SDK 0.9.0 and the cabinet's volumes (2026-11-10)

- Built against VectorPie SDK 0.9.0 (Goldfinch2/VectorPie `b00d537`, menu
  1.1.13) instead of 0.1.0. The SDK is a clone in `external/VectorPie`
  (ignored by git), which `CMakeLists.txt` picks up by itself; the old
  vendored copy in the Tempest 2000 port is no longer used. PC build and Pi
  build unchanged otherwise (the SDK's new physics engine and Ogg decoder
  compile with both toolchains; the Pi build's 94 warnings are all in the
  SDK's Hershey font table).
- Its new audio option: every SDK game follows the cabinet menu's SOUND
  VOLUME and MUSIC VOLUME (`/usr/local/share/advance/vector_pie_menu.cfg`,
  `[audio]` `effects_volume`, `music_volume`). Vectortron plays every sound
  as an SDK effect, so the cabinet's SOUND VOLUME now applies on top of the
  game's VOLUME without any change to the sound code. The SOUND page shows
  that level (`CABINET SOUND VOLUME nn - VECTORPIE MENU`, read from the same
  file the SDK reads; nothing on a PC). `ROM_GAIN` 0.3 stays: if the cabinet
  is now too quiet, raise VOLUME (up to 10 = full) or the menu's SOUND
  VOLUME.
- Replacement sounds may be Ogg Vorbis as well as WAV (`assets/sounds/
  <name>.ogg`); the game already tried `.ogg`, the old SDK refused it.
- `README.md`, `RESUME.md` and this file in English.
- GitHub release `v0.1.3` (2026-11-10) with `vectortron-0.1.3.vpk`, merged
  into `ccr-9894316f-dbxip2` (PR 1). Not on the cabinet, not in the Drive
  folder `games` yet.

## Done (2026-10-16)

- Sound ROM taken from the MAME set `D:\Mame\roms\robotron.zip`:
  `video_sound_rom_3_std_767.ic12` lies as `assets/video_sound_rom_1.ic12`
  in the project (ignored by git, not part of the project).
- All 31 original sounds written as WAV with `vt-sounds`:
  `sounds/original/*.wav` (5.7 MB, ignored by git).
- PC build (MSYS2) and Pi build (clang, without WSL) run, `ctest` 3/3 passed.
- Package `build/pi/vectortron-0.1.0.vpk` (program, `game.ini`, `leds.cfg`,
  marquee, `assets/` with the sound ROM) on the cabinet:
  `/persistent/games/packages/vectortron-0.1.0.vpk`, unpacked into
  `/persistent/games/vectortron/` (with an empty `save/`), md5 checked.
  The menu shows it without a restart; started from the menu on the
  cabinet, it runs on the USB-DVG with sound (`launch.log`). `pkg/run.sh`
  lies there as `run.sh` (a start from the PC, as with Red Baron).

- Second round (after the first game on the cabinet, `launch.log`: the
  original sounds from the ROM):
  - Volume: the ROM recordings play at `ROM_GAIN` 0.3 (about -10 dB);
    before, the DAC ran at full scale ("brutally loud").
  - High score music as in the original (RRTESTC.ASM, GETHIM): `HSTUNE`
    ($1B, $02, about 10 s) for a place in the table, `ULTUNE` (the air raid,
    18 s) for first place; sent with `SNDLDS`, so regardless of priority.
    New sounds `hiscore` and `hiscore_top`.
  - Flicker: the frame rate on the board became adjustable (DISPLAY →
    REFRESH ON THE BOARD, 30–60 Hz, 40 by default instead of a fixed 30),
    taking effect at the next start (`refresh` in `save/settings.txt`).
  - New package on the cabinet, the program replaced by renaming, `save/`
    untouched. Not yet played on the cabinet.

- Third round (owner: the high score music plays, the picture flickers with
  a full arena):
  - The shot: one line in the wave's laser color instead of 10 lines and a
    spark trail.
  - The HUD fixed to the far wall (score left, WAVE and MEN right), no
    longer moving with the camera; lives as a number (`MEN 2`) instead of
    3D men. With the Robotron X camera SCORE is cut off at the top left.
  - Measured on the PC (wave 9, arena camera, demo player, `VP_STATS=1`),
    per frame: before about 2,100 lines / 700 blank moves, now about 1,300
    / 500 (measured with 9 lives: 7 men fewer). Floor grid: about 48,000 of
    the 110,000 units of line length. Effects FULL: blank travel 90,000 →
    140,000.
  - Copied to the cabinet, not yet played there.
- Fourth round: floor grid 5 x 4 cells instead of 10 x 8 (the tiles stay
  10 x 8); effects: half as many fragments and sparks per kill, shorter
  sparks, the floor ring only for big enemies (size >= 14), rings with
  10/14 instead of 14/20 segments, muzzle flash 2 instead of 3 and wall hit
  4 instead of 8 sparks, at most 300 instead of 700 particles at once.
  Measured (wave 9, grid, FULL): line length 113,000 → 82,000, blank travel
  146,000 → 110,000, lines 1,330 → 1,240. Copied to the cabinet (`floor = 2`
  there, no floor: the grid only shows once it is set).
- Fifth round, the models slimmed down (the full version kept in
  `backup/models-original-2026-10-16.c`): feet and hands are DETAIL parts of
  their own and, like visors and hair, are drawn only within 190 units of
  the camera (the Robotron X camera up close; never with the arena camera).
  The grunt's hips and torso, the hulk's belt and chest, Mikey's trousers
  and shirt, Mommy's dress and bodice one body each; the brain's brain and
  the enforcer six-sided instead of eight-sided, spheroid rings 10 instead
  of 14 segments, the star electrode six-pointed instead of eight-pointed,
  the tank's face DETAIL, the player's side hair DETAIL. Measured (wave 9,
  grid, FULL): lines 1,240 → 950, line length 82,000 → 78,000, blank travel
  110,000 → 107,000. In all today: lines 2,200 → 950. On the cabinet, not
  yet played there.
- Sixth round:
  - The service menu in a game too: SERVICE/F2 pauses the game and opens
    it; BACK/EXIT returns to the paused game (`G.service_from`). The 2 + 3
    chord stays reserved for the attract mode (in a game those are strafe
    and camera).
  - Models: spheroid 2 rings of 8 instead of 3 of 10, the core DETAIL;
    the enforcer's red band DETAIL; the quark's green X lines DETAIL; the
    tank's hatch DETAIL; the hulk's antenna knob DETAIL; the snowflake and
    "2084" on the front only (electrodes always face the camera). Banner
    echoes (WAVE, WAVE CLEARED, EXTRA MAN): 2 at FULL, 1 at LIGHT, 0 at OFF
    (always 3 before).
  - Measured wave 9 (grid, FULL): 950 → 770 lines. **Wave start of wave
    13** (35 grunts, 8 hulks, 25 electrodes, 5 spheroids): about 2,900
    lines and 170,000 units of line length: the case in which the monitor
    flickers; after 10 s about 1,200.
  - On the cabinet, not yet played there.
- Seventh round (owner: 60 Hz fixed, the floor out, save even more):
  - 60 Hz fixed, on the board too (`frame_hz = 60`); the `refresh` setting
    is gone.
  - No floor any more, neither tiles nor grid; the `floor` setting and the
    FLOOR menu line are gone (old entries in `settings.txt` are ignored).
    The waves' floor color (`stage.floor`) is now unused.
  - Electrodes are flat outlines (wireframe) instead of plates with
    thickness: a square 4 instead of about 9 lines, at 25 per wave.
  - The word SCORE is gone (the number only, as in the original);
    materializing stretches to 3.5 times instead of 6 times (the line
    length at a wave start); 2 instead of 3 rings at a wave start.
  - Measured at the start of wave 13 (still with the grid): 2,900 → 2,700
    lines, line length 170,000 → 150,000. On the cabinet, not yet played
    there.
  - Marquee: the owner's picture (Williams style, VECTORTRON 2084,
    2000 x 449) is `pkg/marquee.png`; the old one lies in
    `backup/marquee-old-2026-10-16.png`. On the cabinet in the game folder
    and as `artwork/marquees/vectortron.png` for the menu.

- GAME OVER like the banners: the main lettering red, echoes in changing
  colors behind it (`render_big_text`, split off from the banners; echoes
  by EFFECTS 2/1/0). On the cabinet.

- Eighth round: the attachments flat instead of boxes (`add_plate`, 4
  instead of 12 edges): the grunt's visor and chest plate, the hulk's
  visor, the player's eyes, Daddy's tie, Mikey's cap peak, the tank's face.
  All DETAIL parts, so drawn only near the camera (Robotron X camera);
  nothing changes with the arena camera. On the cabinet.

- Ninth round: the WAVE line at the top right is gone (the banner names
  the wave at its start); no points pop-up on the field when the family is
  collected (the 1000 for spheroid and quark stay). On the cabinet.

- Electrodes as sprites: the flat outlines now stand turned exactly to the
  eye (`model_pose.billboard`: turned about y and tilted to the eye), not
  only turned about y to the camera; a square is a square in every camera.
  The shrink when shot too. On the cabinet.

- The picture back at 40 Hz (`frame_hz = 40`, logic 60 Hz), at the owner's
  wish, for comparison. On the cabinet.

- The title in the attract mode: the marquee's lettering as vectors
  (`src/logo.c`): VECTORTRON in chunky, rounded letters, a yellow outline
  around a red one (the marquee's red letters with a yellow border), 2084
  below in yellow with the diamond as the 0 and the pointed 8. Each glyph
  is contours on a grid 14 high, rounded when drawn (3 points per corner);
  the inner line is the contour moved into the body. About 900 lines on
  the title page alone. On the cabinet.

- The picture back at 60 Hz.
- A player (Mario) reports: "when the barriers turn white, the picture
  shifts a little" on his CRT. Checked: the wall's flash changes only the
  color, not the geometry; what shifts the picture is the camera shake
  (`fx_shake`) at every kill (1–2.5 units, random, decaying), which
  coincides with the flash. The shake on kills is out; on the player's
  death it stays (6). The wall flash in the laser color stays, as in the
  original (LFLASH).

- Distribution: the package `vectortron-0.1.0.vpk` lies in the Google
  Drive folder `games` (shared with Mario Montminy); on upload Drive
  replaces the old file of the same name with a new version ("Replace
  existing file"). Uploading from the PC: in Chrome on drive.google.com,
  folder `games`, New → File upload (the session caught the file field by
  script, because the Windows file dialog cannot be driven). The cabinet
  was unreachable (off) on the evening of 2026-10-18; the new program
  (without camera shake, with the logo, 60 Hz) is not there yet.

## Fixed bugs

- MAME 0.289 names the sound ROM `video_sound_rom_3_std_767.ic12`; the game
  and `vt-sounds` knew only older names and would not have found it in the
  zip.
- `vt-sounds` could not create a folder on Windows (`mkdir -p` through
  cmd.exe).
- The `vpk` target did not pack `assets/`.
- With the SDK from `F:\Tempest 2000 Port\vendor\VectorPie`, `VECTORPIE_PACK`
  was empty; now the SDK tree's `tools/vp-pack/vp-pack.sh` is taken (SDK
  0.9.0 sets it itself).
- `vp-pack` refused the colon in the description in `game.ini`.

## Known limitations / open

- On Windows the program in the package has no execute permission; it is
  set by hand after packing (see RESUME.md). Belongs in a script.
- With a full arena the picture flickered on the cabinet (owner); not
  checked there after rounds 4 to 7. The wave start (2,700 lines) is the
  measure; a grunt in the distance costs about 55 lines (6 boxes of 9
  visible edges), 35 of them per wave. Levers left, all visible: arms as
  DETAIL (about −650 lines at a wave start), hulks without arms in the
  distance (about −150), legs as one box in the distance (about −300). Not
  measured on the board itself (`VP_STATS=1` through `run.sh`, only with
  permission).
- `vt-sounds` writes the WAVs at full scale; put into `assets/sounds/` as
  replacements they would be as loud as before.

## Controls laid out like every vector game (2026-10-19)

- `src/bind.c` new (derived from Star Wars Enhanced): every action AUTO
  (read directly: AdvanceMAME's defaults – arrows/stick 1 to move,
  R/F/D/G/stick 2 to fire, Left Ctrl/Left Alt/Space + stick buttons + mouse
  buttons, 1/2/5/6), NONE or up to 4 inputs of its own. In the file:
  `auto`, `off`, else `key:..`; an old `none` means AUTO.
- CONTROLS page: one line per action instead of two columns. START 1/Enter,
  then a button; the first since the cursor came to the line replaces,
  every further one adds. While waiting: Enter (for game functions START 1
  too) back, START 2 NONE, Delete AUTO; left/right NONE/AUTO; Delete on the
  line AUTO. No 5 second abort. A key may have several actions.
- The service menu works through fixed inputs only (`src/vpbind.[ch]`, a
  copy from the Tempest 2000 port): arrows, every stick/hat, START
  1/Enter/keypad Enter, Backspace back, F2/the service button. Fire/strafe
  no longer act in the menu. (Opening it with buttons 2 + 3 held in the
  attract mode stays; that is a game binding.)
- Checked on the PC (replace, add, NONE/AUTO); the Pi build installed on
  the cabinet (old file `vectortron.before-vpbind`), not tried there.
- Bug fixed (2026-11-03): arrow keys could not be laid out (FIRE LEFT/RIGHT
  on left/right jumped to NONE or AUTO at once), because the key just laid
  out was still held and the menu took it as its own left/right. Now as in
  vpbind (`let_go`): after laying out, nothing counts for the menu until
  everything is let go (`settle` in `src/bind.c`). Built on the PC, not
  tried in the menu itself.

GitHub release (2026-10-19): `v0.1.1` with the same package as in the
Google Drive folder `games`. `v0.1.2` (2026-11-03): only the arrow key fix
when laying out; package `vectortron-0.1.2.vpk`, not in Drive yet.
Installed on the cabinet (2026-11-03): the package in `packages/`, only
the program and `game.ini` unpacked, the old program as
`vectortron.before-0.1.2`; not started, the fix not tried there. For every
new package in Drive also make a release (`gh release create v<version>
<package>.vpk`).
