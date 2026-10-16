# Vectortron

A 3D vector tribute to **Robotron: 2084** (Williams, 1982) for
[VectorPie](https://github.com/Goldfinch2/VectorPie): a Raspberry Pi driving
a real vector monitor through a USB-DVG or BeamForge, or a window on a PC.

The look and the camera follow **Robotron X**: a tiled arena, robots and
humans as 3D models with hidden lines, a camera that follows the player and
pans as they move, the score and men standing on the far wall. The game
itself – the waves, the enemies, the speeds, the scores, the flow – follows
the **original source code**
([historicalsource/robotron](https://github.com/historicalsource/robotron)),
see [docs/robotron-rules.md](docs/robotron-rules.md).

## The game

* **40 waves** from the original's table (`WVTAB`); past 40, waves 21–40
  repeat. Brain waves (5, 10, 15 …), tank waves (7, 12, 17 …), hulk waves
  (14, 24, 34) and grunt swarms (9, 19, 29, 39) as in the original.
* The original's timers and speeds: grunts hurry with every kill and over
  time (more so when you stall), hulks crush humans and electrodes and can
  only be pushed back, brains turn humans into progs and fire cruise
  missiles, spheroids drop enforcers, quarks drop tanks, tank shells bounce
  off the walls.
* The original's scores; rescue bonus 1000–5000, an extra man every 25,000.
* A man lost: the robots left are placed anew, as in the original (leftover
  enforcers turn back into spheroids).
* Waves "materialize" (brain waves beam in with the transporter), with the
  rushing rectangles between waves.
* Every wave has its own colors (wall, floor, laser), in a cycle of ten like
  the original's wall colors; shots hitting the wall flash it.
* Tempest 2000 style effects: plasma shots with spark trails and muzzle
  flashes, spark showers and floor shock waves on every hit, camera shake,
  zooming banners (WAVE, WAVE CLEARED, EXTRA MAN), fireworks at the end of a
  wave, a sparkling spiral at every rescue.
* The models follow the original sprites: the red-headed player, the nine
  electrode shapes (with their three-step shrink when shot), the blue and
  green quark, the green tank, the grey enforcer and so on.
* 1 or 2 players taking turns, coins and credits with the START lamps, a
  hall of fame with initials (also shown in the VectorPie menu), an attract
  mode with the title, the robots and their points, the high scores and a
  demo game.

## Controls

| | |
|---|---|
| Move | P1 stick (arrow keys), 8 ways |
| Fire, twin stick | a second stick: `p1_doubleright_*` from advmame.rc, or the P2 stick (keys R/F/D/G, joystick 2) |
| Fire, single stick | button 1 (left Ctrl): fires the way you face |
| Strafe (single stick) | hold button 2 (left Alt): keeps the facing |
| Camera | button 3 (Space): Robotron X / arena / classic overhead |
| Start | 1 / 2, coins 5 / 6, back to the menu: Esc |
| Service menu | F2 (or SERVICE as bound), or hold buttons 2 + 3 for two seconds in the attract mode |

As on the original's two 8-way joysticks, moving and firing go 8 ways only
(an analog stick counts past half travel), at most 4 shots at a time. Twin
stick and single stick both work at all times; there is nothing to switch.

Out of the box every action follows the cabinet's `advmame.rc`. Any action
can be bound otherwise in the service menu (CONTROLS): two inputs each, any
key, joystick button, joystick axis direction or hat direction. For a pad's
right stick as the fire stick, either bind FIRE UP/DOWN/LEFT/RIGHT there or
add to `advmame.rc`:

```
input_map[p1_doubleright_up]    joystick_digital[0,1,1,up]
input_map[p1_doubleright_down]  joystick_digital[0,1,1,down]
input_map[p1_doubleright_left]  joystick_digital[0,1,0,left]
input_map[p1_doubleright_right] joystick_digital[0,1,0,right]
```

## Two players

START 2 starts a game for two (when 2 PLAYER GAMES is on), taking turns on
the same controls as on the original cabinet: each player has their own
wave, men and score. Both scores stand on the far wall, 1UP and 2UP, the one
playing blinking; each turn opens with PLAYER 1 / PLAYER 2 and GET READY.

## Service menu

F2, or holding buttons 2 + 3 for two seconds in the attract mode, opens it.
Up / down choose, left / right change, button 1 (or Return) selects, button 2
(or Backspace) goes back. The arrow keys, Return, Backspace and the first
joystick always work in it, whatever the bindings.

* **GAME SETTINGS** – free play, men per game, extra man every (none,
  10,000 … 50,000), difficulty 0–10 (the original's master difficulty, 5
  recommended), 2 player games on/off, attract mode sounds on/off.
* **DISPLAY** – picture width and height (50–120 %), position left/right
  and down/up, over a test pattern (the board's edge, a grid, a circle that
  is round when the picture is right); camera, floor, effects (off, light,
  full).
* **CONTROLS** – every action, its two inputs, CLEAR (back to the
  cabinet's), RESET ALL TO THE CABINET.
* **INPUT TEST** – every action lights up while held.
* **SOUND** – volume, a sound test, and whether the original ROM was found.
* **RESET HIGH SCORES**, **RESET ALL SETTINGS** (each asks for a second press).

Everything takes effect at once and is kept in `save/settings.txt` next to
the executable (it can be edited by hand too). The picture size and
position apply to everything drawn, the game's lines and the SDK's.

The tiled floor costs the beam a lot: FLOOR AUTO picks the grid on a USB-DVG
and the tiles on a BeamForge and in a window. On the board the picture runs
at 30 Hz (`frame_hz_board`); the game logic always runs at 60 Hz, as the
original. `VP_STATS=1` prints what a frame costs.

## Sounds

**No ROM is needed**: out of the box the game plays sounds made in code, in
the spirit of the Williams sound board.

**The original sounds**, optionally: Robotron has its own sound board (a
Williams 6808 with an 8-bit DAC) and the sounds live in its ROM, not in the
game code. Vectortron emulates that board: when it finds the sound ROM in
your own MAME set (`robotron.zip`), it runs the original sound code and
records every sound, so they sound exactly as on the cabinet. The command
sequences (the player's death: `$11` twice, then `$17`) and the priorities
come straight from the game's source, and as on the board there is one
voice: a sound only cuts in on one of the same priority or lower. Brain
waves beam in with the transporter sound; other waves start silently, as in
the original.

The ROM is looked for in:

1. `VT_SOUND_ROM` (a `robotron.zip`, or the ROM file itself)
2. `assets/robotron.zip`, `assets/robotron.snd`, `assets/video_sound_rom_1.ic12` next to the executable
3. AdvanceMAME's ROM folders (`dir_rom` in advmame.rc, `/usr/local/share/advance/rom`, `~/.advance/rom`)

In the zip it is `video_sound_rom_1.ic12` (older sets: `robotron.snd`). The
ROM is not part of this project. `VT_SYNTH_SOUNDS=1` forces the sounds made
in code.

`vt-sounds [robotron.zip] [folder]` writes all the original sounds as WAV
files, to listen to or to edit.

**Your own sounds**: any sound can be replaced by a file
`assets/sounds/<name>.wav` (or `.ogg`) next to the executable, `<name>` one of

`shot grunt_die electrode_die hulk_hit rescue human_die prog_make brain_die
cruise spheroid_die enforcer_birth enforcer_shot enforcer_die quark_die
tank_birth tank_shot shell_bounce tank_die player_die wave_start wave_end
extra_man coin start grunt_step reprogram small_die spark_die shell_die
prog_die transport start2`

(The `assets/` folder is copied next to the executable when building, and
is ignored by git.)

## Building

As any VectorPie game, from WSL2 Debian trixie after the SDK's
`tools/setup-wsl.sh`:

```sh
cmake --preset pc    && cmake --build --preset pc     # Windows: build-pc/vectortron.exe
cmake --preset pi    && cmake --build --preset pi     # the cabinet: build-pi/vectortron
cmake --build --preset pi --target vpk                # build-pi/vectortron-0.1.0.vpk
```

The SDK is taken from `-DVECTORPIE_SDK_DIR=<VectorPie>/sdk` (its source
tree), else the installed SDK (`~/VectorPie/dist/<pc|pi>`), else it is
fetched from GitHub. On Linux directly: `cmake --preset linux && cmake --build --preset linux`.

Copy the `.vpk` to a USB drive's `games/` folder (or to
`/persistent/games/packages/`) and install it from the menu: Settings →
GAMES → INSTALL GAMES.

### Tools

* `vt-gallery --windowed` – every 3D model on one screen, turning.
* `vt-sim [games] [waves]` – the rules without a screen, played by the demo
  player; checks that nothing goes astray (`ctest`).
* `vt-sounds` – the original sounds as WAV files (see above).
* `ctest` also checks the 6800 emulation (`cpu_test`, `wms_test`:
  instructions, flags, the interrupt through the PIA, cycle-exact pitch).
* `VT_WAVE=n` starts a game at wave n at once, `VT_AUTOPLAY=1` lets the demo
  player play it (for testing).

## Legal

Robotron: 2084 is a trademark of its owners; Vectortron is a non-commercial
fan project with none of the original graphics or sounds. The rules were
worked out from the published source code.
