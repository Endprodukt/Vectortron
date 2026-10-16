# Vectortron

Eine 3D-Vektor-Hommage an **Robotron: 2084** (Williams, 1982) für
[VectorPie](https://github.com/Goldfinch2/VectorPie) – Raspberry Pi mit
USB-DVG/BeamForge am echten Vektormonitor, oder im Fenster am PC.

Optik und Kamera orientieren sich an **Robotron X**: eine gekachelte Arena,
Roboter und Menschen als 3D-Modelle mit Verdeckung, eine Kamera, die dem
Spieler folgt und dynamisch schwenkt, Punkte und Leben an der hinteren Wand.
Das Spiel selbst – Wellen, Gegnerverhalten, Tempo, Punkte, Ablauf – folgt dem
**originalen Quellcode** ([historicalsource/robotron](https://github.com/historicalsource/robotron)),
siehe [docs/robotron-rules.md](docs/robotron-rules.md).

## Das Spiel

* **40 Wellen** aus der Original-Tabelle (`WVTAB`), danach wiederholen sich
  die Wellen 21–40. Brain-Wellen (5, 10, 15 …), Tank-Wellen (7, 12, 17 …),
  Hulk-Wellen (14, 24, 34) und Grunt-Schwärme (9, 19, 29, 39) wie im Original.
* Gegner mit den Original-Timern und -Geschwindigkeiten: Grunts werden pro
  Abschuss und über die Zeit schneller (und mehr, wenn man trödelt), Hulks
  zerquetschen Menschen und Elektroden und lassen sich nur zurückstoßen,
  Brains machen Menschen zu Progs und schießen Cruise Missiles, Spheroids
  setzen Enforcer ab, Quarks setzen Tanks ab, Tank-Granaten prallen von den
  Wänden ab.
* Punkte wie im Original; Rettungsbonus 1000–5000, Extra-Mann alle 25 000.
* Pro Leben wird die Welle mit den übrigen Gegnern neu verteilt, wie im
  Original (übrige Enforcer werden wieder zu Spheroids).
* Wellenstart mit „Materialisieren“ (Brain-Wellen: Transporter-Effekt),
  zwischen den Wellen der Rechteck-Tunnel.
* Jede Welle hat ihre eigenen Farben (Wand, Boden, Laser), im Zehnerzyklus
  wie die Wandfarben des Originals; Schüsse an der Wand lassen sie in der
  Laserfarbe aufblitzen.
* Effekte im Stil von Tempest 2000: Plasmaschüsse mit Funkenschweif und
  Mündungsfeuer, Funkenregen und Schockwellen bei jedem Treffer,
  Kamerawackeln, aufzoomende Banner (WAVE, WAVE CLEARED, EXTRA MAN),
  Feuerwerk am Wellenende, Glitzerspirale bei jeder Rettung.
* 1 oder 2 Spieler abwechselnd, Münzen/Credits mit START-Lampen, Hall of
  Fame mit Initialen (erscheint auch im VectorPie-Menü), Attract-Mode mit
  Titel, Gegnervorstellung, Bestenliste und Demo-Spiel.

## Steuerung

| | |
|---|---|
| Bewegen | P1-Stick (Pfeiltasten) |
| Feuern, Twin-Stick | zweiter Stick: `p1_doubleright_*` aus der advmame.rc, oder P2-Stick (Tasten R/F/D/G, Joystick 2, auch analog) |
| Feuern, Single-Stick | Button 1 (Strg links): feuert in Blickrichtung |
| Strafe (Single-Stick) | Button 2 (Alt links) halten: Blickrichtung bleibt fest |
| Kamera wechseln | Button 3 (Leertaste): Robotron X / Arena / klassisch von oben |
| Start | 1 / 2, Münze 5 / 6, Zurück ins Menü: Esc |

Twin- und Single-Stick funktionieren gleichzeitig, eine Umschaltung gibt es
nicht. Gefeuert wird wie im Original in 8 Richtungen, höchstens 4 Schüsse.

Ein Gamepad mit zwei Sticks oder ein Robotron-Panel bindet man in der
`advmame.rc` des Kabinetts, z. B. (rechter Stick eines Pads als Feuer-Stick):

```
input_map[p1_doubleright_up]    joystick_digital[0,1,1,up]
input_map[p1_doubleright_down]  joystick_digital[0,1,1,down]
input_map[p1_doubleright_left]  joystick_digital[0,1,0,left]
input_map[p1_doubleright_right] joystick_digital[0,1,0,right]
```

## Einstellungen

`save/settings.txt` neben dem Programm (wird beim ersten Kamerawechsel
angelegt):

```
free_play = 0     ; 1: START braucht keine Münze
lives     = 3     ; Männer zu Spielbeginn
extra_man = 25000 ; Extra-Mann alle so viele Punkte (0: keiner)
camera    = 0     ; 0 Robotron X, 1 Arena, 2 klassisch von oben
floor     = -1    ; -1 automatisch, 0 Kacheln, 1 Gitter, 2 keiner
difficulty = 5    ; MASTER DIFFICULTY des Originals, 0..10 (5 = Tabelle)
```

Der gekachelte Boden kostet den Strahl viel; `floor = -1` nimmt auf einer
USB-DVG automatisch das Gitter, auf BeamForge und im Fenster die Kacheln. Auf
der Platine läuft das Bild mit 30 Hz (`frame_hz_board`), die Spiellogik immer
mit 60 Hz wie das Original. `VP_STATS=1` zeigt die Kosten pro Bild.

## Sounds

Alle Sounds werden im Programm erzeugt (im Stil des Williams-Soundboards).
Jeder lässt sich durch eine eigene Datei ersetzen:
`assets/sounds/<name>.wav` (oder `.ogg`) neben dem Programm, mit `<name>` aus

`shot grunt_die electrode_die hulk_hit rescue human_die prog_make brain_die
cruise spheroid_die enforcer_birth enforcer_shot enforcer_die quark_die
tank_birth tank_shot shell_bounce tank_die player_die wave_start wave_end
extra_man coin start grunt_step reprogram small_die`

(Der Ordner `assets/` wird beim Bauen mitkopiert und ist in `.gitignore`.)

## Bauen

Wie jedes VectorPie-Spiel, aus WSL2 Debian trixie nach `tools/setup-wsl.sh`
des SDK:

```sh
cmake --preset pc    && cmake --build --preset pc     # Windows: build-pc/vectortron.exe
cmake --preset pi    && cmake --build --preset pi     # Kabinett: build-pi/vectortron
cmake --build --preset pi --target vpk                # build-pi/vectortron-0.1.0.vpk
```

Das SDK wird gesucht: `-DVECTORPIE_SDK_DIR=<VectorPie>/sdk` (Quellbaum), sonst
das installierte SDK (`~/VectorPie/dist/<pc|pi>`), sonst wird es von GitHub
geholt. Unter Linux direkt: `cmake --preset linux && cmake --build --preset linux`.

Die `.vpk` per USB-Stick (`games/`) oder nach `/persistent/games/packages/`
kopieren und im Menü unter Settings → GAMES → INSTALL GAMES installieren.

### Werkzeuge

* `vt-gallery --windowed` – alle 3D-Modelle drehend auf einem Bildschirm.
* `vt-sim [spiele] [wellen]` – die Regeln ohne Bildschirm, gespielt vom
  Demo-Spieler; prüft, dass nichts aus dem Ruder läuft (`ctest`).
* `VT_WAVE=n` startet sofort Welle n, `VT_AUTOPLAY=1` lässt den Demo-Spieler
  spielen (zum Testen).

## Rechtliches

Robotron: 2084 ist eine Marke ihrer Inhaber; Vectortron ist ein nicht
kommerzielles Fan-Projekt ohne Original-Grafiken oder -Sounds. Die Regeln
wurden aus dem veröffentlichten Quellcode nachvollzogen.
