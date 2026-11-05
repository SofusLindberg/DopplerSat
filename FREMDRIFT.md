# Fremdrift

Logbog over arbejdet på Doppler Saturator. Nyeste øverst. Claude skriver en ny post **hver gang** noget er bygget, så en ny session altid kan fortsætte præcis herfra. (Multiband Saturator har sin egen logbog i `../MultibandSat/FREMDRIFT.md`.)

---

## 2026-11-05 — Wobble 2 flyttet til VST2, Steinberg-søgestien fjernet i FL Studio

**Status:** Ingen ændring i pluginnet. Seneste version er stadig **Doppler Saturator 1.1** (installeret, ikke testet i en DAW).

**Ønsket:**
- Ved opstart fik brugeren status fra posten nedenfor.
- Brugeren svarede ja til forslaget under "Åbent" med "Ja, flyt Wobble to til VST2" (se `KRAV.md`).

**Fundet:**
- Wobble 2 (SONiVOX 2.4.0, VST2-synth) består af tre dele:
  - plugin-filen `C:\Program Files\Steinberg\Wobble 2_x64.dll`;
  - presets og samples i `C:\Program Files\Steinberg\SONiVOX\Wobble 2` (433 filer, 277 MB, ingen DLL'er);
  - indstillinger i `C:\ProgramData\SONiVOX\Wobble 2`.
- **Wobble 2 har stierne til sit indhold gemt:**
  - `Wobble.txt` peger på `.../Steinberg/SONiVOX/Wobble 2/Samples`;
  - preset-databasen `Wobble.svxdb` (SQLite) har 226 faste stier ind i Steinberg-mappen.
  - Derfor er kun plugin-filen flyttet. Indholdet skulle ellers rettes i SQLite-databasen, og der er intet SQLite-værktøj på maskinen.
- **Ableton** (Live 11.3.43) bruger allerede `C:/Program Files/Common Files/VST2` som VST2-mappe. Før så den altså ikke Wobble 2. Det gør den nu efter en ny scanning.
- I FL's database var det kun `Wobble 2.nfo`, der pegede ind i `C:\Program Files\Steinberg\`.

**Gjort:**
1. **Backup** af FL's søgestier, SONiVOX-registreringen og `Wobble 2.nfo` i `Dokumenter\Image-Line\Backup Wobble 2-flytning 2026-11-05\`. En `.reg`-fil kan dobbeltklikkes for at lægge den tilbage.
2. **Flyttet** (med administrator-adgang) `Wobble 2_x64.dll` til `C:\Program Files\Common Files\VST2\`.
   - Hashen er den samme før og efter (`F3DCA15F…`), og filens dato er bevaret.
   - Registreringsnøglen `HKLM\SOFTWARE\WOW6432Node\SONiVOX\Wobble 2` har nu `VSTDir` = VST2-mappen. Så lægger en senere opdatering filen samme sted. `ContentDir` er uændret.
3. **FL Studio** (var lukket):
   - `ps_file_filename_0` i `Wobble 2.nfo` peger nu på den nye placering. Filen er stadig skjult og har stadig CRLF.
   - Søgestien `C:\Program Files\Steinberg\` (List\6) er fjernet fra `HKCU\Software\Image-Line\Shared\Plugin search paths`, og Count er sat fra 7 til 6.
   - Tilbage står FL's egne mapper samt `Common Files\VST3`, `Common Files\VST2` og `C:\Program Files\VstPlugins`.

**Åbent:**
- Wobble 2's afinstallationsprogram (`C:\Program Files (x86)\Wobble 2\unins000.exe`) kender kun den gamle placering. Afinstalleres Wobble 2, bliver `VST2\Wobble 2_x64.dll` liggende og skal slettes i hånden.
- Det er ikke testet, at Wobble 2 åbner med presets i FL Studio og Ableton.
- Latensen (se posten nedenfor).

**NÆSTE GANG (fortæl brugeren dette ved opstart):**
- Seneste version er **Doppler Saturator 1.1**. Den er installeret ved siden af 1.0, men ikke testet i en DAW.
- Wobble 2 ligger nu i `Common Files\VST2`, og Steinberg-stien er væk fra FL Studio. Spørg:
  - Virker Wobble 2 med presets i FL Studio?
  - Er Steinberg-filerne væk efter "Find more plugins"?
  - Er Wobble 2 kommet med i Ableton efter en ny scanning?
- Scan plugins igen i Ableton og FL Studio, og prøv 1.1. Se punkterne under "NÆSTE GANG" i posten nedenfor.

---

## 2026-11-04 — Version 1.1: ingen drive, kun output, højreklik-automation, "?" ved 0 ms, 2–3 gange lettere (samme lyd), FL Studio ryddet op. Gem og luk

**Status ved lukning:**
- Bygget som **"Doppler Saturator 1.1"** (plugin-kode D101, **12 parametre**). Ingen fejl eller advarsler.
- **`HostSmoke --compare` mod 1.0 (Drive 0 %): bit-identisk** i alle 14 situationer: former, offset, Sync, fartgrænsen, Dry/Wet, Output, Bypass til/fra, skift 24/48, højt og lavt bånd.
- **Røgtesten er OK:**
  - alle 12 parametre kan automatiseres;
  - "Bypass" er DAW'ens bypass;
  - peak højst 0,74 for en indgang på 0,6;
  - ingen NaN.
- **EditorShot:** `doppler1..3.png`, `doppler4_help.png` og `doppler5_menu.png` er set. Testen viser også, at højreklik åbner menuen og *ikke* ændrer Bypass eller Frequency.
- **Installeret og verificeret med filhash** (`C1FEA715…`, moduleinfo 1.1.0). VST3'en er bygget 23:23:15, efter sidste kildeændring kl. 23:22:50. 1.0 ligger uændret ved siden af (`624E93D9…`).
- **Ikke testet i Ableton eller FL Studio.** Committed og pushet.

**Ønsket:** se `KRAV.md` ("Version 1.1", brugerens ord). Kort fortalt:
1. fjern Drive (mindst mulig drive) og Distortion, hvis den ikke ændrer pitch-effektens lyd;
2. kun output i spektret og forklaringen;
3. alle parametre skal kunne automatiseres, fx med højreklik;
4. "?" ved de to 0 ms med en kort, ikke-teknisk forklaring og hvad 24 / 48 dB betyder;
5. gør pluginnet lettere uden at ændre lyden;
6. ryd op i plugin-listen (skærmbillede fra FL Studio).

Undervejs skrev brugeren: "gem og luk, gør det til en regel, at du altid skal fortælle mig næste gang, hvor vi kom til" og "den nye version [...] skal i navnet have versionnummer og lægges i VST3-mappen [...] så jeg stadig har de ældre versioner".

**Fundet:**
- **Ved 0 % drive saturerer motoren intet.** `BandSat` er slukket, så formeren køres slet ikke. Lyden er kun den tørre sum T = tch(gch0(h23(x))), dvs. allpassene fra alle delefiltre. Kun bånd 1's fire delefiltre flytter sig, og det er Doppler-effekten.
  - Distortion gjorde altså *ingen* forskel. Den er fjernet, ligesom Drive.
  - 1.0 regnede desuden hele bånd 1's opdeling (8 kæder + fades), gch[1] og 16 delefilter-designs + punkter + fades hver 32. sample og smed resultatet væk.
- **Fejl i testværten siden 1.0:** `setReal` sendte rå værdier som 0..1 til VST3-værten. Frequency 300 blev 20 kHz, Width 1 blev 6 okt, Rate 1 blev 20 Hz, og Output 0 blev −24 dB. **1.0's pitch-tal i posten nedenfor (89 / 127 cent) er altså målt ved forkerte indstillinger.** Rettet (`normFor`: diskrete = nr. / (antal − 1), ellers binær søgning på pluginnets egen tekst), og testen tjekker selv omregningen.
- **Rigtige pitch-tal** (Amount 100 %, Width 1 okt, båndet om tonen, gennemsnit over 4 perioder):

| tone, Rate | 24 dB | 48 dB | 48/24 |
|---|---|---|---|
| 300 Hz, 0,25 Hz | 16 | 49 | 3,1 |
| 300 Hz, 1 Hz | 64 | 202 | 3,2 |
| 300 Hz, 3 Hz | 162 | 228 | 1,4 |
| 300 Hz, 6 Hz | 154 | 192 | 1,25 |
| 300 Hz, 12 Hz | 110 | 152 | 1,4 |
| 100 Hz, 1 Hz | 196 | 615 | 3,1 |
| 1 kHz, 1 Hz | 20 | 59 | 3,0 |
| 3 kHz, 1 Hz | 7 | 21 | 2,8 |
| 8 kHz, 1 Hz | 5 | 13 | 2,6 |

  - 48 bøjer ca. 3 gange så meget ved langsom og middel Rate.
  - Fra ca. 3 Hz rammer kanterne fartgrænsen (40 / 20 okt/s), og så er forskellen kun ca. 1,3 gange.
  - Effekten er størst på dybe toner.
- **Plugin-listen i FL Studio:** de 364 "plugins" (Qt6…, qwindows, medialib, asiobtin, REX Shared Library osv.) er DLL'er fra Steinbergs programmer. Activation Manager, ASIO og MediaBay blev installeret 3/11 kl. 17:10.
  - De ligger i `C:\Program Files\Steinberg\`, som er en *egen* søgesti i FL Studio (brugeren tilføjede den, formentlig til Wobble 2).
  - FL Studio scannede dem 3/11 kl. 22:09. Det har intet med vores plugins at gøre.

**Bygget:**
1. **Drive og Distortion fjernet.** `buildEngineParams` sætter drive 0. Knappen, vælgeren, drive-aksen (0–400 %), drive-stregen og træk op/ned i displayet er væk. Modulet hedder nu **"Edges"**.
2. **Kun output:** inputspektret er fjernet fra tegningen *og* fra lyden (`inAnalyzer` er væk, så der er én FFT mindre). Forklaringen viser kun "Output".
3. **Automation:**
   - `ParamControl<>` om alle drejeknapper, vælgere og knapper. Højreklik viser `showParamMenu`:
     - DAW'ens egne punkter øverst, hvis DAW'en har dem (VST3 `IComponentHandler3` via `getHostContext()->getContextMenuForParameter`);
     - "Select ... for automation (last touched)" (begin, samme værdi, end);
     - "Reset ... to default";
     - uden DAW-menu: en kort vejledning til Ableton og FL Studio.
   - Højreklik på displayet giver Frequency's menu.
   - Menuen har pluginnets mørke look.
   - `getBypassParameter()` gør vores Bypass til DAW'ens bypass (før havde JUCE sin egen ekstra, altså 15 parametre).
4. **"?" ved Edges:** `HelpButton` / `HelpOverlay` er kopieret fra Multiband Saturator (samme layout). Teksten er "How they sound" og "What 24 and 48 dB/oct mean" (tallene ovenfor). Desuden viser `SlopeView` ("Edge steepness") de to kanters fald (−24 / −48 dB én oktav ude), og det valgte lyser.
5. **Lettere:** nyt `Source/DopplerCore.h` gør præcis det samme som `Engine` ved 0 % drive: samme filtre, samme rækkefølge, samme opvarmning og samme toning ved Bypass og 24/48-skift.
   - Kun allpass-delen af bånd 1's delefiltre designes (`designXoverAp`), og kun når kanterne har flyttet sig. De faste designes én gang.
   - CPU (skiftevis 3 gange, `HostSmoke --bench`):

| blokstørrelse | 1.0 | 1.1 |
|---|---|---|
| 512 | 15–18 % | 7–8 % |
| 64 | 28–31 % | 10–11 % |
| 1024 | 28–31 % | 12 % |

   - `DspBench` (nyt): oversamplingen er ca. halvdelen af det, der er tilbage, filtrene resten, og `setParams` 0,1–0,3 %.
   - **Latensen er stadig 60 samples (1,4 ms)** fra 4x-oversamplingen. Den kan ikke fjernes uden at ændre lyden: ved 1x kan de øverste delefiltre ikke ligge over 20,7 kHz og holder op med at flytte sig, når båndet ligger højt. DAW'en udligner de 60 samples selv.
6. **FL Studio ryddet op:** 364 falske poster (728 filer, `.nfo` + `.fst`) i `Dokumenter\Image-Line\FL Studio\Presets\Plugin database\Installed\{Effects,Generators}\{VST,New}` er slettet. Backup ligger i `Dokumenter\Image-Line\Backup FL plugin-database (Steinberg-filer) 2026-11-04.zip`. Wobble 2 og alle rigtige plugins er urørt. FL Studios søgesti er *ikke* ændret (se Åbent).
7. **Tests:**
   - HostSmoke har fået `--compare`, `--bench`, pitch-tabellen og parameter-/omregningstjekket.
   - EditorShot har fået hjælpe- og menubilledet plus højreklik-tjekket.
   - `DspBench` er nyt.

**Faldgruber:**
- PowerShell's `Get-ChildItem` (og `Compress-Archive`) ser ikke skjulte filer, og FL's `.nfo`-filer er skjulte. Den første zip manglede derfor halvdelen. Brug `-Force` eller `[IO.Compression.ZipFile]::CreateFromDirectory`.
- En JUCE-popupmenu lukker selv igen, når programmet ikke er i forgrunden. I testen skal den derfor tælles straks efter klikket.
- En VST3-værts parametre er ikke `RangedAudioParameter` (se fejlen ovenfor).

**Åbent / til brugeren:**
- **FL Studio vil finde Steinberg-DLL'erne igen** ved næste "Find more plugins", fordi `C:\Program Files\Steinberg\` er en søgesti. Wobble 2 (`Wobble 2_x64.dll`) ligger direkte i den mappe. Mulighed: flyt Wobble 2 til `C:\Program Files\Common Files\VST2` og fjern Steinberg-stien i FL's Plugin Manager. Spørg først.
- Skal latensen fjernes, selvom diskanten så ændrer sig lidt, når båndet ligger højt? (Kun hvis brugeren mærker de 1,4 ms.)

**NÆSTE GANG (fortæl brugeren dette ved opstart):**
- Seneste version er **Doppler Saturator 1.1**. Den er installeret ved siden af 1.0, men ikke testet i en DAW. Scan plugins igen i Ableton og FL Studio.
- Prøv:
  - Højreklik på en knap: i Ableton og FL Studio kommer vores menu; i fx Reaper/Bitwig også DAW'ens egne punkter.
  - I FL Studio: "Select ... for automation" → Tools > Last tweaked > Create automation clip.
  - "?" ved Edges.
  - At lyden er som 1.0 med Drive 0 %.
- Spørg om Steinberg-søgestien i FL Studio (se Åbent).

---

## 2026-11-03 (2) — Pushet til GitHub. Gem og luk

- Brugeren oprettede det private repo og skrev: "DopplerSat er oprettet på Git. Push, gem, luk, og fortæl mig næste gang, hvor vi er kommet til. :-)"
- Pushet til `https://github.com/SofusLindberg/DopplerSat` (`master` sporer `origin/master`).
- Installationen er kontrolleret igen: den installerede "Doppler Saturator 1.0" er identisk med buildet (filhash `624E93D9…`, moduleinfo 1.0.0).

**NÆSTE GANG (fortæl brugeren dette ved opstart):**
- Seneste version er **1.0**. Den er installeret og verificeret, men **ikke testet i Ableton**.
- Genstart Live og scan plugins igen, og indsæt "Doppler Saturator 1.0". Prøv:
  - Sine ved 0,5–2 Hz med Amount 100 % på en bas eller et pad.
  - Square med Sync.
  - Dry/Wet 50 % (phaser).
- Spørg:
  - Er pitch-effekten stærk nok? Fartgrænsen er 40 / 20 okt/s.
  - Skal Edge Q være fra (som nu) eller et blødt fade?
  - Er knapperne de rigtige?
  - Skal leverandøren "DitNavn" ændres?

---

## 2026-11-03 — Version 1.0: projektet oprettet ud fra Multiband Saturators 0 ms-tilstande

**Status:**
- Bygget som **"Doppler Saturator 1.0"** (plugin-kode D100, 14 parametre + JUCE's bypass = 15). Ingen fejl eller advarsler.
- **Røgtesten er OK** (`tests/host`, se tallene nedenfor).
- `EditorShot` har lavet `doppler1..3.png`, og billederne er set.
- Installationen kører; se posten i Multiband Saturators log og "Status ved lukning", når den er færdig.
- **Ikke testet i Ableton.**

**Ønsket:** se `KRAV.md` (brugerens ord, 2026-11-03). Brugeren valgte navnet "Doppler Saturator" med privat repo, og at Amount er "hvor langt båndet flytter sig". Undervejs skrev brugeren: "du må gerne genbruge designet af de to 0 ms til doppler-pluginnet for at spare tid og energi".

**Bygget:**
- `Source/SatDsp.h` er en kopi af Multiband Saturator 3.18's. Den har fået to flag i `Engine`:
  - `forceTree` er brugt, så Doppler-effekten er der også ved 0 % drive.
  - `onlyOnBands` (nyt) springer de tre slukkede bånds opdeling og allpass-kæder over. Det giver halv CPU og samme lyd: pitch-tallene er bit for bit de samme.
- **Ét bånd** (motorens bånd 1). De andre er slukket og ligger ved 10–12 Hz og over 70 kHz.
- **Edge Q er låst til fra**, så saturationen er et tydeligt bånd med 0 ms-kanterne. Med Edge Q 50 % bredte fadet sig over det meste af spektret.
- **LFO** (hver 32. sample):
  - Former: Sine, Triangle, Square (bløde overgange på ca. 8 ms) og Random (glider fra én tilfældig værdi til den næste).
  - Offset klipper kurven ved top og bund.
  - Rate går fra 0,01 til 20 Hz eller Sync (8 takter til 1/32, punkteret og trioler). Sync følger værtens position, når sangen spiller.
- **Band:** Frequency, Amount (100 % = ±3 oktaver) og Width (0,1–6 oktaver).
- **Saturation:** Drive, Distortion og de to 0 ms-knapper (standard 48). Skiftes der mellem 24 og 48, startes motoren forfra (dry, mens filtrene varmes op).
- **Output:** Dry/Wet (dry er forsinket præcis som oversamplingen), Output og Bypass.
- **Brugerfladen:** fast 900 × 600 i Multiband Saturators design 2A.
  - Displayet viser spektrum (input og output), det skraverede område LFO'en flytter båndet hen over, og båndet lige nu (samme kurvemodel som lyden).
  - Træk til venstre / højre = Frequency, op / ned = Drive, dobbeltklik = nulstil.
  - LFO-visningen viser kurven med en prik. Random viser et rullende spor af de faktiske værdier.
  - LCD-felterne viser "Sweep range" og "Band now".
- `installer-vst3.ps1` og `ableton-automation.ps1` er som i Multiband Saturator 3.18.

**Fundet og rettet undervejs:**
- **Niveauhop ved hurtig flytning.** Med Square og 20 Hz gav en tone med peak 0,6 op til 8,8. Målt med én bånd: niveauet stiger med kanternes fart, ikke med hvor tit filtrene opdateres.
  - 24 dB: 80 okt/s gav 0,78.
  - 48 dB: 60 okt/s gav 1,25 og 100 okt/s 1,7.
  - Nu er der en **fartgrænse på kanterne**: 40 okt/s ved 24 dB og 20 okt/s ved 48 dB. Det giver højst ca. +1,4 dB i det værste tilfælde (bredde 0,2 oktav).
  - Ved hurtig Rate og stor Amount bliver bevægelsen derfor mindre. Tooltippen på Rate siger det.
- **Skift mellem 24 og 48 midt i lyden** gav også et hop (filtrene skifter opbygning). Nu startes motoren forfra.
- **CPU:** 26 % → 11,6 % af én kerne i røgtesten, efter `onlyOnBands`. `setParams` hver 32. sample koster kun ca. 2 %.

**Målt (røgtesten):** tone 300 Hz, båndet om 300 Hz (1 oktav), LFO 1 Hz sinus.

| Tilstand | Amount 0 % | Amount 100 %, 0 % drive | Amount 100 %, 100 % drive |
|---|---|---|---|
| 0 ms 24 | 0,00 cent | 89 cent | 88 cent |
| 0 ms 48 | 0,07 cent | 127 cent | 124 cent |

Tallene er et gennemsnit over 8 perioder. Alle 16 trin (former, offset, Sync, hurtig / langsom, Dry/Wet 50 %, Bypass) har peak højst 0,70 for en indgang med 0,6. Ingen NaN. Latens 60 samples (4x oversampling).

**Åbent / til brugeren:**
- Edge Q er låst til **fra**. Sig til, hvis du hellere vil have det bløde fade.
- Fartgrænsen (40 / 20 okt/s) sætter et loft for, hvor meget pitchen kan bøje.
- Leverandøren hedder stadig "DitNavn" (som i Multiband Saturator).

**NÆSTE GANG:** Rescan i Ableton og indsæt "Doppler Saturator 1.0". Prøv Sine ved 0,5–2 Hz med Amount 100 % på en bas eller et pad, Square med Sync og Dry/Wet 50 % (phaser). Fortæl, om pitch-effekten er stærk nok, og om Edge Q fra er rigtigt.
