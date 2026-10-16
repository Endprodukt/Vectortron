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
| Move | P1 stick (arrow keys) |
| Fire, twin stick | a second stick: `p1_doubleright_*` from advmame.rc, or the P2 stick (keys R/F/D/G, joystick 2, analog too) |
| Fire, single stick | button 1 (left Ctrl): fires the way you face |
| Strafe (single stick) | hold button 2 (left Alt): keeps the facing |
| Camera | button 3 (Space): Robotron X / arena / classic overhead |
| Start | 1 / 2, coins 5 / 6, back to the menu: Esc |

Twin stick and single stick both work at all times; there is nothing to
switch. As in the original, shots go 8 ways, at most 4 at a time.

A gamepad with two sticks or a Robotron panel is bound in the cabinet's
`advmame.rc`, for example a pad's right stick as the fire stick:

```
input_map[p1_doubleright_up]    joystick_digital[0,1,1,up]
input_map[p1_doubleright_down]  joystick_digital[0,1,1,down]
input_map[p1_doubleright_left]  joystick_digital[0,1,0,left]
input_map[p1_doubleright_right] joystick_digital[0,1,0,right]
```

## Settings

`save/settings.txt` next to the executable (written the first time the
camera is changed):

```
free_play  = 0     ; 1: START needs no coin
lives      = 3     ; men at the start of a game
extra_man  = 25000 ; an extra man every this many points (0: none)
camera     = 0     ; 0 Robotron X, 1 arena, 2 classic overhead
floor      = -1    ; -1 automatic, 0 tiles, 1 grid, 2 none
difficulty = 5     ; the original's MASTER DIFFICULTY, 0..10 (5: as the wave table)
```

The tiled floor costs the beam a lot; `floor = -1` picks the grid on a
USB-DVG and the tiles on a BeamForge and in a window. On the board the
picture runs at 30 Hz (`frame_hz_board`); the game logic always runs at
60 Hz, as the original. `VP_STATS=1` prints what a frame costs.

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
