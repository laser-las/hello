# halcyon

Extern klient för Source-motorn (CS2/CS:GO-layout) med de tre funktioner som beskrivs i specen:

| Område | Vad den gör | Kod |
|---|---|---|
| **Information** | Läser motståndarnas positioner ur minnet och ritar boxar, hälsokurvor, namn, vapen, avstånd, riktlinjer, flaggor och en radar ovanpå spelet | `src/game.cpp`, `src/esp.cpp` |
| **Sikte** | Flyttar muspekaren mot vald hitbox när en motståndare är inom FOV, med mjukning, reaktionstid, humanisering och rekylkompensering | `src/aim.cpp` |
| **Avfyrning** | Klickar automatiskt när siktet ligger över en motståndare, med fördröjning, hålltid och burst | `src/aim.cpp` |

Klienten är **extern**: den öppnar målet read-only, läser minne och ritar i ett eget genomskinligt overlay-fönster. Inget injiceras i spelprocessen.

## Bygg

```
build.bat
```

`build.bat` använder `cl.exe` om du står i en Visual Studio-prompt, annars CMake:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Krav: Windows 10/11 x64, C++17. Länkas mot `user32`, `gdi32`, `gdiplus`, `winmm`. Inga tredjepartsbibliotek.

## Offsets

Allt som är spelspecifikt ligger i `config.ini`. Programmet gissar aldrig: saknas ett offset loggas det och motsvarande funktion står still.

```ini
[offsets]
entity_list  = 0x1F2A3B0
local_player = 0x1F2B4C0
view_matrix  = 0x1A3E4F0
m_ihealth    = 0x344
```

Ett värde kan också vara en signatur:

```ini
entity_list = sig:48 8B 0D ? ? ? ? 48 85 C9
```

Signaturen scannas i `client.dll`, och `mov`/`lea reg,[rip+disp]` räknas om till adressen variabeln pekar på. Antal träffar loggas, så du ser direkt om mönstret är tvetydigt.

### Snabbväg efter en spelpatch

1. Kör en offset-dumper mot samma spelversion.
2. Spara symbolerna som text, en per rad (`dwEntityList = 0x...`, `m_iHealth: 344` fungerar båda).
3. `halcyon.exe --import offsets.txt`
4. `halcyon.exe --dump` — skriver ut modulbas, alla upplösta adresser och fält-offsets.

`--import` accepterar också JSON-liknande rader, så utdata från en dumper kan klistras in nästan oförändrat.

## Körning

```
halcyon.exe                        # config.ini bredvid exe:n
halcyon.exe --preset rage          # legit | semilegit | semirage | rage
halcyon.exe --import offsets.txt
halcyon.exe --import offsets.txt --save config.ini   # skriv ihop och spara
halcyon.exe --dump                 # löser offsets och avslutar
halcyon.exe --no-overlay           # bara sikte/avfyrning
```

Standardtangenter:

| Tangent | Funktion |
|---|---|
| `F1` | ESP av/på |
| `F2` | Aimbot av/på |
| `F3` | Triggerbot av/på |
| `DELETE` | Panic, stänger av allt |
| `END` | Avsluta |

Aimbot ligger på `XBUTTON2` och triggerbot på `ALT` som standard. Båda kan sättas till `toggle` i config om du hellre vill växla med ett tryck.

Överlagringen kräver att spelet körs i **borderless windowed**. I exclusive fullscreen äger spelet swapchainen och ett externt fönster syns inte.

## Konfiguration

`config.ini` är kommenterad och grupperad per funktion: `[aim]`, `[trigger]`, `[esp]`, `[visuals]`, `[radar]`, `[keys]`, `[loop]`.

Några nycklar som gör störst skillnad:

- `aim.smooth` — 1 snappar direkt, 12–20 ser ut som en människa.
- `aim.scale` — musklick per skärmpixel. Utgå från 1.0 och justera mot din känslighet; för högt värde ger översläng.
- `aim.reaction_time` — fördröjning innan siktet börjar röra sig mot ett nytt mål.
- `aim.rcs` — rekylkompensering. Kräver `m_aimpunch`; `visuals.fov` används för att räkna om grader till pixlar.
- `trigger.delay_min_ms` / `delay_max_ms` — slumpad fördröjning innan skottet går.
- `esp.fade_start` / `max_distance` — tonar ut mål på långt håll.

Nycklar som står under "Reserverade nycklar" längst ned i `config.ini` laddas men har ingen effekt i den här builden, eftersom de kräver skrivningar till spelminnet eller hookar (backtrack, resolver, silent aim, no-flash, bhop och liknande).

## Arkitektur

```
main.cpp      attach, offset-upplösning, trådar, hetkeys, livscykel
mem.cpp       ReadProcessMemory, modullista, IDA-mönsterscanning, RIP-dekoder
game.cpp      entity-lista -> World (positioner, ben, hp, vapen, flaggor)
aim.cpp       World -> SendInput (rörelse, klick, scope, rekyl)
esp.cpp       World -> overlay (boxar, radar, crosshair, HUD)
overlay.cpp   lagdrat fönster med per-pixel alpha, GDI+-ritning
config.cpp    INI-parser, presets, --import
```

Två trådar:

- **scan-tråden** läser minnet `aim_hz` gånger per sekund, publicerar en `World`-snapshot och kör sikte/avfyrning mot samma snapshot. En läsning per tick, inte en per funktion.
- **render-tråden** (huvudtråden) tar senaste snapshot, ritar och komponerar overlayen. Den sköter också fönstermeddelanden och hetkeys.

Overlayen är `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE`: klick går rakt igenom, fokus stjäls aldrig, och fönstret döljs automatiskt när spelet inte är i förgrunden.

## Begränsningar

- Extern och read-only: ingen wall check utöver spelets egen `m_bspotted`, ingen påverkan på spelmekanik som flash, rök eller FOV.
- `--dump` kräver att spelet redan kör.
- Overlayen ritar i skärmkoordinater; kör du spelet med skalad DPI kan `entity_stride`/projektion se rätt ut men overlayen hamna fel — använd 100 % skalning eller borderless i inhemsk upplösning.
- Allt i `config.ini` som rör offsets är versionsberoende. Uppdatera efter varje patch och verifiera med `--dump`.
