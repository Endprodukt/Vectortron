# Stand

Vectortron, geholt von https://github.com/endprodukt/vectortron (Branch
`ccr-9894316f-dbxip2`, Stand `e455e0a`), lokal in `F:\Vectorton`.

## Erledigt (2026-10-16)

- Sound-ROM aus dem MAME-Set `D:\Mame\roms\robotron.zip` geholt:
  `video_sound_rom_3_std_767.ic12` liegt als `assets/video_sound_rom_1.ic12`
  im Projekt (von git ignoriert, nicht Teil des Projekts).
- Alle 31 Originalgeräusche mit `vt-sounds` als WAV geschrieben:
  `sounds/original/*.wav` (5,7 MB, von git ignoriert).
- PC-Build (MSYS2) und Pi-Build (clang, ohne WSL) laufen, `ctest` 3/3 bestanden.
- Paket `build/pi/vectortron-0.1.0.vpk` (Programm, `game.ini`, `leds.cfg`,
  Marquee, `assets/` mit der Sound-ROM) auf dem Automaten:
  `/persistent/games/packages/vectortron-0.1.0.vpk` und ausgepackt nach
  `/persistent/games/vectortron/` (mit leerem `save/`), md5 geprüft.
  Das Menü zeigt es ohne Neustart; am Automaten aus dem Menü gestartet,
  läuft auf dem USB-DVG mit Ton (`launch.log`). `pkg/run.sh` liegt dort als
  `run.sh` (Start vom PC aus, wie bei Red Baron).

- Zweite Runde (nach dem ersten Spiel am Automaten, `launch.log`: Originalgeräusche
  aus der ROM):
  - Lautstärke: die ROM-Aufnahmen werden mit `ROM_GAIN` 0.3 (rund -10 dB)
    abgespielt; vorher lief der DAC in voller Aussteuerung („brutal laut“).
  - Highscore-Musik wie im Original (RRTESTC.ASM, GETHIM): `HSTUNE` ($1B, $02,
    rund 10 s) bei einem Platz in der Liste, `ULTUNE` (Fliegeralarm, 18 s) für
    Platz 1; mit `SNDLDS` gesendet, also ohne Rücksicht auf die Priorität.
    Neue Klänge `hiscore` und `hiscore_top`.
  - Flimmern: Bildrate auf dem Board jetzt einstellbar (DISPLAY → REFRESH ON
    THE BOARD, 30–60 Hz, ab Werk 40 statt bisher fest 30), wirkt beim
    nächsten Start (`refresh` in `save/settings.txt`).
  - Neues Paket auf dem Automaten, Programm per Umbenennen ersetzt, `save/`
    unberührt. Noch nicht auf dem Automaten gespielt.

- Dritte Runde (Besitzer: Highscore-Musik läuft, Bild flimmert bei voller Arena):
  - Schuss: eine Linie in der Laserfarbe der Welle statt 10 Linien und
    Funkenspur.
  - HUD fest an der hinteren Wand (Punkte links, WAVE und MEN rechts), läuft
    nicht mehr mit der Kamera mit; Leben als Zahl (`MEN 2`) statt
    3D-Männchen. Mit der Robotron-X-Kamera ist SCORE links oben angeschnitten.
  - Messung am PC (Welle 9, Kamera Arena, Demo-Spieler, `VP_STATS=1`), je
    Bild: vorher rund 2.100 Linien / 700 Leerwege, jetzt rund 1.300 / 500
    (mit 9 Leben gemessen: 7 Männchen weniger). Boden-Gitter: rund 48.000
    der 110.000 Einheiten Linienlänge. Effekte FULL: Leerweg 90.000 → 140.000.
  - Auf den Automaten kopiert, dort noch nicht gespielt.
- Vierte Runde: Boden-Gitter 5 x 4 Felder statt 10 x 8 (die Kacheln bleiben
  10 x 8); Effekte: halb so viele Trümmer und Funken je Abschuss, Funken
  kürzer, Bodenring nur bei großen Gegnern (size >= 14), Ringe mit 10/14
  statt 14/20 Segmenten, Mündungsfeuer 2 statt 3 und Wandtreffer 4 statt 8
  Funken, höchstens 300 statt 700 Teilchen zugleich. Messung (Welle 9, Gitter,
  FULL): Linienlänge 113.000 → 82.000, Leerweg 146.000 → 110.000, Linien
  1.330 → 1.240. Auf den Automaten kopiert (dort steht `floor = 2`, kein
  Boden: das Gitter wirkt dort erst, wenn es eingestellt wird).
- Fünfte Runde, die Modelle abgespeckt (Sicherung der schönen Fassung:
  `backup/models-original-2026-10-16.c`): Füße und Hände sind eigene
  DETAIL-Teile und werden wie Visiere und Haare nur näher als 190 Einheiten
  an der Kamera gezeichnet (Robotron-X-Kamera nah; mit der Arena-Kamera nie).
  Hüfte und Rumpf des Grunts, Gürtel und Brust des Hulks, Hose und Hemd von
  Mikey, Kleid und Mieder von Mommy je ein Körper; Gehirn des Brains und
  Enforcer sechs- statt achtseitig, Spheroid-Ringe 10 statt 14 Segmente,
  Stern-Elektrode sechs- statt achtzackig, Gesicht des Tanks DETAIL, seitliche
  Haare des Spielers DETAIL. Messung (Welle 9, Gitter, FULL): Linien 1.240 →
  950, Linienlänge 82.000 → 78.000, Leerweg 110.000 → 107.000. Insgesamt
  heute: Linien 2.200 → 950. Auf dem Automaten, dort noch nicht gespielt.

## Behobene Fehler

- MAME 0.289 nennt die Sound-ROM `video_sound_rom_3_std_767.ic12`; das Spiel
  und `vt-sounds` kannten nur ältere Namen und hätten sie in der Zip nicht
  gefunden.
- `vt-sounds` konnte unter Windows keinen Ordner anlegen (`mkdir -p` über cmd.exe).
- Das `vpk`-Ziel packte `assets/` nicht mit ein.
- Mit dem SDK-Stand aus `F:\Tempest 2000 Port\vendor\VectorPie` war
  `VECTORPIE_PACK` leer; jetzt wird `tools/vp-pack/vp-pack.sh` des SDK-Baums genommen.
- `vp-pack` lehnte den Doppelpunkt in der Beschreibung in `game.ini` ab.

## Bekannte Einschränkungen / offen

- Unter Windows hat das Programm im Paket kein Ausführungsrecht; es wird nach
  dem Packen von Hand gesetzt (siehe RESUME.md). Gehört in ein Skript.
- Bei voller Arena flimmerte es auf dem Automaten (Besitzer); nach den
  Runden 4 und 5 dort noch nicht geprüft. Weitere Hebel, falls nötig: die
  LOD-Entfernung (190) senken, Arme ganz weglassen in der Ferne, die Zahl
  der Ringe/Teilchen weiter drücken. Auf dem Board selbst nicht gemessen
  (`VP_STATS=1` über `run.sh`, nur mit Erlaubnis).
- `vt-sounds` schreibt die WAVs in voller Aussteuerung; als Ersatz nach
  `assets/sounds/` gelegt wären sie wieder so laut wie vorher.
