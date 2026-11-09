# halcyon

Extern klient för Source-motorn (CS2/CS:GO-layout), skriven helt i **C# / .NET 8 / WinForms**.
Klienten öppnar målet read-only, läser minne med `ReadProcessMemory`, ritar ESP i ett eget
genomskinligt overlay-fönster och skickar aim/trigger/övriga inputs med `SendInput`. Inget
injiceras i spelprocessen.

| Område | Vad den gör | Kod |
|---|---|---|
| **Information** | Läser motståndarnas positioner ur minnet och ritar boxar, hälsa, pansar, namn, vapen, avstånd, riktlinjer, skelett, flaggor, glow, offscreen-pilar och radar ovanpå spelet | `src/Game.cs`, `src/Esp.cs` |
| **Sikte** | Flyttar muspekaren mot vald hitbox när en motståndare är inom FOV, med mjukning, reaktionstid, humanisering, rekylkompensering och valbar prioritet | `src/Aim.cs` |
| **Avfyrning** | Klickar automatiskt när siktet ligger över en motståndare, med fördröjning, hålltid, burst och auto-pistol | `src/Aim.cs` |
| **Rörelse & chat** | Bunny hop, auto strafe, auto reload, knife bot, chat spam och kill say | `src/Misc.cs` |
| **Inställningar** | Vanligt WinForms-fönster med flikar för varje funktion, keybinds, färger och offsets | `src/SettingsForm.cs` |

Den gamla C++-versionen ligger kvar i `legacy-cpp/` som referens. Den byggs inte av
lösningen och behövs inte för att köra C#-klienten.

## Krav

- Windows 10/11 x64.
- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) (8.0.x).
- Visual Studio Community 2022 (17.8 eller senare) med arbetsbelastningen
  **.NET desktop development**, eller VS Code med **C# Dev Kit**.
- Klienten begär administratörsrättigheter via `app.manifest` (UAC vid start), eftersom
  `ReadProcessMemory` mot ett spel som körs som admin annars nekas.

## Bygg och kör

### VS Code

Öppna mappen i VS Code och kör i terminalen:

```powershell
dotnet build -c Release
.\bin\Release\net8.0-windows\halcyon.exe
```

`F5` fungerar direkt: `.vscode/launch.json` och `tasks.json` bygger Release och startar
`bin\Release\net8.0-windows\halcyon.exe`. Eftersom manifestet kräver
administratörsrättigheter måste både VS Code och terminalen köras som administratör, annars
startar debuggen utan att kunna läsa spelprocessen. Vill du köra utan att lämna terminalen:

```powershell
dotnet run -c Release
```

### Visual Studio Community

1. Öppna `Halcyon.sln`.
2. Välj **Release** och **Any CPU** (binären är alltid x64 eftersom `PlatformTarget` är satt).
3. **Build → Build Solution** (eller `Ctrl+Shift+B`).
4. Starta `bin\Release\net8.0-windows\halcyon.exe`, eller tryck `F5`. VS startar då klienten
   och UAC-frågan dyker upp; kör Visual Studio som administratör om du vill felsöka direkt.

Efter varje build kopieras `config.ini` till utdatamappen. Redigerar du rotfilen måste du
bygga om, eller starta med `--config` mot en annan fil.

## Körning

```
halcyon.exe                        # config.ini bredvid exe:n
halcyon.exe --preset rage          # legit | semilegit | semirage | rage
halcyon.exe --import offsets.txt
halcyon.exe --import offsets.txt --save config.ini   # skriv ihop och spara
halcyon.exe --dump                 # löser offsets, skriver ut dem och avslutar
halcyon.exe --no-overlay           # bara sikte/avfyrning, inget ESP-fönster
halcyon.exe --config min.ini       # annan konfigurationsfil
```

`--dump` och loggen skrivs både till konsolen (om klienten startas från en terminal) och till
`halcyon.log` bredvid exe:n.

### Inställningsfönstret

Tryck `INSERT` för att öppna inställningarna. Det är ett vanligt WinForms-fönster med flikar
för General, Aimbot, Trigger, ESP, Visuals, Radar, Misc, Offsets, Keys och Loop. Overlayen
förblir klick-genomträdande och döljs automatiskt medan inställningsfönstret är i förgrunden.

- **Apply** skriver in värdena i den körande konfigurationen.
- **Save** skriver även `config.ini`.
- **Reload file** läser om filen från disk.
- Stänger du fönstret (kryss, `ESC` eller `INSERT`) appliceras ändringarna automatiskt och
  fokus går tillbaka till spelet.
- **Exit client** avslutar klienten.

### Standardtangenter

| Tangent | Funktion |
|---|---|
| `INSERT` | Öppna/stäng inställningarna |
| `F1` | ESP av/på |
| `F2` | Aimbot av/på |
| `F3` | Triggerbot av/på |
| `DELETE` | Panic, stänger av ESP, aim och trigger |
| `END` | Avsluta klienten |

Aimbot ligger på `XBUTTON2` och triggerbot på `ALT` som standard. Båda kan sättas till
`toggle = true` om du hellre växlar med ett tryck. De globala tangetarna ändras under fliken
**Keys**, och aim/trigger-tangenterna på respektive flik: klicka på knappen och tryck sedan
den nya tangenten (Esc avbryter).

Överlagringen kräver att spelet körs i **borderless windowed**. I exclusive fullscreen äger
spelet swapchainen och ett externt fönster syns inte.

## Offsets

Allt som är spelspecifikt ligger i `config.ini`. Programmet gissar aldrig: saknas ett offset
loggas det och motsvarande funktion står still.

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

Signaturen scannas i `client.dll`, och `mov`/`lea reg,[rip+disp]` räknas om till adressen
variabeln pekar på. Antal träffar loggas, så du ser direkt om mönstret är tvetydigt.

### Snabbväg efter en spelpatch

1. Kör en offset-dumper mot samma spelversion.
2. Spara symbolerna som text, en per rad (`dwEntityList = 0x...`, `m_iHealth: 344` fungerar båda).
3. `halcyon.exe --import offsets.txt`
4. `halcyon.exe --dump` — skriver ut modulbas, alla upplösta adresser och fält-offsets.
5. `halcyon.exe --import offsets.txt --save config.ini` om du vill behålla resultatet.

`--import` accepterar också JSON-liknande rader, så utdata från en dumper kan klistras in
nästan oförändrat. Fält-offsets som inte känns igen lämnas orörda.

## Konfiguration

`config.ini` är kommenterad och grupperad per funktion:

`[general]`, `[game]`, `[offsets]`, `[aim]`, `[trigger]`, `[esp]`, `[visuals]`, `[radar]`,
`[misc]`, `[keys]`, `[loop]`.

Presets skriver över aim-, trigger- och ESP-grupperna i en sammanhängande profil:

| Preset | Karaktär |
|---|---|
| `legit` | Låg FOV, hög smooth, lång reaktionstid, bara synliga mål, enkel ESP |
| `semilegit` | Mjukt sikte med auto scope/pistol, fullständigare ESP |
| `semirage` | Snabbare sikte, auto fire, multi-hitbox, fyllda boxar, skelett och glow |
| `rage` | Snapp, prediction, backtrack-approximation, resolver-jitter, allt ESP på |

Ett preset appliceras vid start med `--preset`, eller från fliken **General**. Sparar du
efteråt blir profilen bestående.

Nycklar som gör störst skillnad:

- `aim.smooth` — 1 snappar direkt, 12–20 ser ut som en människa.
- `aim.scale` — musklick per skärmpixel. Utgå från 1.0 och justera mot din känslighet; för
  högt värde ger översläng.
- `aim.reaction_time` / `reaction_jitter` — fördröjning innan siktet börjar röra sig mot ett
  nytt mål.
- `aim.rcs` — rekylkompensering. Kräver `m_aimpunch`; `visuals.fov` används för att räkna om
  grader till pixlar.
- `trigger.delay_min_ms` / `delay_max_ms` — slumpad fördröjning innan skottet går.
- `esp.fade_start` / `max_distance` — tonar ut mål på långt håll.
- `esp.visible_check` — ritar bara mål som spelet själv markerar som synliga (`m_bspotted`).
- `esp.oof_check` — `true` ritar bara spelare vars box är på skärmen. Sätt den till `false`
  om du vill använda `esp.offscreen_arrows`.
- `loop.aim_hz` — hur ofta minnet läses och aim/trigger utvärderas (30–2000).
- `loop.render_fps` — 0 = obegränsat.

### Reserverade funktioner

Följande nycklar finns i configen och i inställningsfönstret för kompatibilitet, men har
medvetet ingen effekt i en extern, read-only klient. De kräver skrivningar till spelminnet
eller hookar:

- silent aim, anti-aim, fake latency
- no-flash, no-smoke, no-decals, remove hands
- FOV changer, third-person, no-scope
- auto accept, auto defuse, auto buy
- spectator list, sound ESP, ping counter
- bomb-, bomb timer-, dropped weapon- och grenade-ESP
- jump bug, edge jump

`backtrack` och `resolver` är approximationer: backtrack väljer den sparade position som
ligger närmast önskad tid i klientens egen historik, och resolver lägger på en liten fast
eller slumpad lateral offset. Ingen av dem rör spelets egna nätverkshistorik.

## Arkitektur

```
Program.cs       attach, CLI, offset-upplösning, trådar, hotkeys, livscykel
Native.cs        P/Invoke: process, moduler, DIB, layered window, SendInput
Vk.cs            virtuella tangentkoder och namn
Core.cs          Vec2/Vec3/Mat4, WorldToScreen, klocka, logg
Memory.cs        ReadProcessMemory, modullista, IDA-mönsterscanning, RIP-dekoder
Game.cs          entity-lista -> World (positioner, ben, hp, vapen, flaggor)
Input.cs         SendInput-omslutning (tangenter, musklick, text)
Config.cs        INI-parser, presets, --import, save
Aim.cs           World -> SendInput (rörelse, klick, scope, rekyl, backtrack)
Esp.cs           World -> overlay (boxar, radar, crosshair, HUD)
Misc.cs          bhop, strafe, reload, knife bot, chat
Overlay.cs       lagdrat WinForms-fönster med per-pixel alpha och GDI+-ritning
SettingsForm.cs  tabbad konfigurationsyta
```

Två trådar:

- **scan-tråden** läser minnet `aim_hz` gånger per sekund, publicerar en färsk `World`-snapshot
  och kör sikte/avfyrning mot samma snapshot. En läsning per tick, inte en per funktion.
- **render-tråden** (huvudtråden, STA) tar senaste snapshot, ritar och komponerar overlayen.
  Den sköter också fönstermeddelanden, inställningsfönstret och hotkeys via
  `Application.DoEvents()`.

Overlayen är `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`:
klick går rakt igenom, fokus stjäls aldrig, och fönstret döljs automatiskt när spelet inte är
i förgrunden. Ritningen går via `CreateDIBSection` + GDI+ + `UpdateLayeredWindow` med manuell
alpha-premultiplikation.

## C++-versionen

`legacy-cpp/` innehåller den tidigare implementationen med CMake och `build.bat`. Den är
oförändrad i sak och kan fortfarande byggas separat:

```powershell
cmake -S legacy-cpp -B legacy-cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build legacy-cpp/build --config Release
```

Kräver C++17 och länkas mot `user32`, `gdi32`, `gdiplus`, `winmm`. `config.ini` kopieras
bredvid den byggda exe:n. C#-lösningen refererar inte till den.

## Begränsningar

- Extern och read-only: ingen wall check utöver spelets egen `m_bspotted`, ingen påverkan på
  spelmekanik som flash, rök eller FOV.
- `--dump` kräver att spelet redan kör.
- Overlayen ritar i spelets klientkoordinater (fysiska pixlar, PerMonitorV2). Kör spelet i
  borderless windowed; vid ovanliga DPI-upplösningar kan `entity_stride`/projektion se rätt ut
  men overlayen hamna fel — använd 100 % skalning eller inhemsk upplösning.
- Aim/trigger går via `SendInput` och påverkas av din musacceleration, polling rate och
  känslighet. Justera `aim.scale` och `aim.smooth` mot din egen setup.
- Allt i `config.ini` som rör offsets är versionsberoende. Uppdatera efter varje patch och
  verifiera med `--dump`.
