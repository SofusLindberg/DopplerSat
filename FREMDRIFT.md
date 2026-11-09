# Fremdrift

Logbog over arbejdet på Doppler Saturator. Nyeste øverst. Claude skriver en ny post **hver gang** noget er bygget, så en ny session altid kan fortsætte præcis herfra. (Multiband Saturator har sin egen logbog i `../MultibandSat/FREMDRIFT.md`.)

---

## 2026-11-09 (4) — Gem og luk

**Status ved lukning:**
- Brugeren skrev: "gem og luk og fortæl mig næste gang hvor vi kom til".
- Seneste version er **Pitch Bend Texture 1.5**. Den er installeret og verificeret igen ved lukning: den installerede fil er identisk med buildet (`933573D3…`), og buildet (18:11:04) er nyere end sidste kildeændring (18:06:38). Doppler Saturator 1.0–1.2 og Pitch Bend Texture 1.3–1.4 ligger ved siden af.
- Alt er committed og pushet. Arbejdsmappen er ren.
- **Ikke testet i en DAW. Mac-buildet er ikke kørt.**

**NÆSTE GANG (fortæl brugeren dette ved opstart):** se punkterne under "NÆSTE GANG" i posten 2026-11-09 (3) nedenfor. Kort fortalt:
1. Test 1.5 i Ableton / FL Studio (Sweep, Band Size, 96 dB/oct, automation, mono-spor).
2. Se hjemmesiden (`website/index.html`): billede i 900 × 600 og købsboks til $10.
3. Skal Mac-buildet startes på GitHub (Actions > "Mac build")? Har brugeren en Mac med Logic?
4. Brugeren skal selv oprette en konto hos en betalingsudbyder og evt. et Apple Developer ID.
5. Åbne spørgsmål: COMPANY_NAME "DitNavn" → "SoundSo"?, 192 dB/oct, latens, AAX.

---

## 2026-11-09 (3) — Version 1.5: displays hedder Sweep / Band Size, klar til Mac (AU), webshop på hjemmesiden

**NÆSTE GANG (fortæl brugeren dette ved opstart; brugeren bad selv om at blive mindet):**
- **Test pluginnet:** scan plugins igen og prøv **Pitch Bend Texture 1.5** i Ableton og FL Studio: Sweep, Band Size, 96 dB/oct med Sine 0,1–0,5 Hz på bas / pad, Bypass, automation (højreklik), gerne et mono-spor.
- **Se hjemmesiden:** `website/index.html` (eller `website/serve.ps1` → http://localhost:8765/). Tjek billedet (nu 900 × 600, ikke strakt) og købsboksen ($10).
- **Mac / Logic:** Mac-buildet er ikke kørt. Spørg, om det skal startes (GitHub > Actions > "Mac build" > Run workflow), og om brugeren har en Mac med Logic til den endelige test.
- **Brugeren skal selv oprette:** en konto hos en betalingsudbyder (Lemon Squeezy / Paddle) til webshoppen og et Apple Developer ID ($99/år) til at signere Mac-versionen.
- Stadig åbent: 192 dB/oct, latensen, AAX til Pro Tools, COMPANY_NAME er stadig "DitNavn" (vises som producent i DAW'erne; skal det være "SoundSo"?).

**Status:**
- Bygget som **"Pitch Bend Texture 1.5"** (D105). Ingen fejl eller advarsler.
- **`HostSmoke --compare` mod 1.4:** bit-identisk ("SAMME LYD").
- **Uden SSE2** (`build-shim`, `DS_FORCE_SIMD_SHIM=ON`, som på Apple Silicon) mod den almindelige 1.5: bit-identisk i alle 14 situationer.
- **Røgtest + DAW-test:** alt OK. CPU målt skiftevis med `--bench`: 1.4 1,95 / 1,96 %, 1.5 1,93 / 1,91 % (en røgtest viste 9,3 %, men det var anden belastning på maskinen).
- **EditorShot:** displayene hedder "SWEEP" og "BAND SIZE".
- **Installeret og verificeret med filhash** (`933573D3…`, moduleinfo 1.5.0). 1.0–1.4 ligger uændret ved siden af.
- **Ikke testet i en DAW. Mac-buildet er ikke kørt.**

**Ønsket:** se `KRAV.md` ("Version 1.5, webshop og Mac", brugerens ord): webshop til 10 USD, billedet er strakt, virker det i Logic på Mac, og (undervejs) displayene skal hedde som knapperne.

**Lavet:**
1. **Billedet strakt:** CSS gjorde det smallere, men `height="600"` holdt højden fast. Nu `height: auto`, og billedet vises i sin rigtige størrelse (900 × 600, målt i browseren) under købsboksen. På mobil skaleres det i forholdet 3:2 (342 × 228). Billedet er nyt fra 1.5.
2. **Webshop (mockup):** købsboks med **$10 USD, one-time**, hvad man får, og knapper ("Buy · $10" øverst, "Buy it for $10" i toppen, "Buy Pitch Bend Texture · $10"). Knapperne åbner `CHECKOUT_URL`; den er tom, så en boks forklarer, at betalingsudbyderens checkout åbner her. Der er ingen falsk kortformular.
   - Forslag til udbyder: en "merchant of record" (Lemon Squeezy eller Paddle). De sælger på SoundSo's vegne og klarer moms i EU og sales tax i USA, levering og licensnøgler. Stripe er billigere, men så skal SoundSo selv klare momsen.
   - Licens: JUCE 9 har en gratis licens op til en vis omsætning. Det skal læses i JUCE 9-licensen, før der sælges.
3. **Displays:** "Sweep range" → **"Sweep"**, "Band now" → **"Band Size"**, tooltips rettet.
4. **Mac / Logic** (Logic kan kun Audio Units, og der er ingen Mac her):
   - `Source/SimdShim.h`: SSE2 findes ikke på Apple Silicon, så de 13 SSE2-funktioner, motoren bruger, er skrevet uden SSE2 (samme regnestykker, bit for bit samme lyd, målt). `-ffp-contract=off` på Mac, så compileren ikke ændrer regningen (FMA).
   - CMake: `FORMATS VST3 AU` på Apple (Windows uændret).
   - Skrifter på Mac: Bahnschrift → Avenir Next, Consolas → Menlo (ikke set endnu).
   - `HostSmoke "AU:<navn>"` indlæser Audio Units på Mac (samme vej som Logic).
   - `.github/workflows/mac.yml` (startes kun i hånden): bygger AU + VST3 universal, auval (Logics kontrol), HostSmoke + DAW-test på AU'en, AU mod VST3 (skal være bit-identisk), EditorShot-billeder, og lægger de pakkede plugins op.
   - `gh` (GitHub CLI) findes ikke her, så Claude kan ikke selv starte eller læse workflowen.

---

## 2026-11-09 (2) — Hjemmeside-mockup for SoundSo

**NÆSTE GANG (fortæl brugeren dette ved opstart; brugeren bad selv om at blive mindet):**
- **Test pluginnet:** scan plugins igen i Ableton og FL Studio og prøv **Pitch Bend Texture 1.4**: Sweep og Band Size, 96 dB/oct med Sine 0,1–0,5 Hz på bas / pad, Bypass, automation (højreklik), gerne også på et mono-spor.
- **Se hjemmesiden:** åbn `website/index.html` (eller kør `website/serve.ps1` og gå til http://localhost:8765/). Spørg, hvad der skal ændres: navn, tekster, farver, sektioner, sprog (nu engelsk).
- Stadig åbent: 192 dB/oct, latensen, AAX til Pro Tools.

**Status:**
- Brugeren skrev (midt i 1.4): "Du skal også lave en hjemmeside til produktet. Hjemmesiden skal repræsentere den overordnede virksomhed, og pluginnet er det første produkt fra virksomhed. Kald virksomheden for SoundSo indtil videre. Lav et html mockup af hjemmesiden. Mind mig næste gang, at jeg skal teste pluginnet og se hjemmesiden".
- **`website/index.html`**: én fil med indbygget CSS og et skærmbillede (`website/assets/pitch-bend-texture.png`, fra EditorShot 1.4). Mørk hardware-stil med pluginnets farver. Sektioner:
  - virksomheden ("Sound that moves.") med et animeret bånd, der glider hen over et spektrum;
  - første produkt, seks funktioner og "Three steps" med små tegninger;
  - lydeksempler (pladsholdere), specs (VST3, mono/stereo, 44,1–192 kHz, 1,4 ms, automation) og om SoundSo;
  - tilmelding til nyhedsbrev (ikke forbundet) og en strip øverst: "Design mockup".
- Teksten er på engelsk og er pladsholdere. "About" nævner, at pluginnet kom fra en multiband saturator.
- Tjekket i browseren ved 1280 × 800 og på mobil (375 px, ingen vandret rulning).
- **`website/serve.ps1`** + `.claude/launch.json` ("soundso-website"): en lille lokal server på port 8765. Den skal angive `ContentLength64`, ellers går den ned ved første fil.
- Ingen kodeændringer i pluginnet.

---

## 2026-11-09 — Version 1.4: Sweep og Band Size, virker i alle DAW'er (mono, hale, DAW-test)

**Status:**
- Bygget som **"Pitch Bend Texture 1.4"** (plugin-kode D104, stadig 12 parametre). Ingen fejl eller advarsler.
- **`HostSmoke --compare` mod 1.3:** bit-identisk i alle 14 situationer ("SAMME LYD").
- **Røgtest + ny DAW-test: alt OK** (se nedenfor). CPU 5,0 % af én kerne.
- **EditorShot:** "SWEEP / travel range" og "BAND SIZE / the band itself" passer i Band-modulet.
- **Installeret og verificeret med filhash** (`70BAE969…`, moduleinfo 1.4.0). Doppler Saturator 1.0–1.2 og Pitch Bend Texture 1.3 (`62F131FE…`) ligger uændret ved siden af.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.4", brugerens ord): nyt navn til Width (selve kurven forlænges/forkortes), tydeligere Amount, og virker det (og automation) i alle DAW'er?

**Lavet:**
1. **Width → "Band Size"** (undertekst "the band itself"), **Amount → "Sweep"** (undertekst "travel range", passer til "Sweep range"-displayet). "Bånd" er det præcise ord for kurven. Tooltips og "?"-teksten er rettet. ID'erne er de samme.
2. **DAW-test i `HostSmoke`** (ny), først kørt mod 1.3:
   - **Mono understøttedes ikke** (kun stereo → stereo). Nu også mono → mono og mono → stereo (Cubase, Studio One, Reaper, Bitwig har mono-spor). Mono kører den samme stereomotor med en kopi af venstre kanal; stereo er bit for bit som før.
   - **Halen:** pluginnet sagde 0 s, men kanterne ringer op til 1,32 s efter (til −80 dB, 20 Hz, 0,1 okt, 96 dB/oct). DAW'er, der stopper plugins ved stilhed eller bouncer, ville klippe det af. Nu 2 s.
   - Samplerater 44,1–192 kHz: samme pitch (195–196 cent), latens altid 60 samples. OK.
   - Tilfældige blokke 0–4096: samme niveau og ingen spring. OK. (Blokke større end lovet ved start får JUCE's VST3-del til at gå ned; det er forbudt i VST3, så ingen rigtig DAW gør det.)
   - Alle 12 parametre automatiseret på én gang midt i lyden: peak 0,50. Output Gain −12 dB via automation giver præcis × 0,251. OK.
   - Gem / hent projekt: alle 12 værdier ens. OK.
3. **Svar om "alle DAW'er":** VST3 dækker alle DAW'er på Windows undtagen Pro Tools (kræver AAX, Avid-konto og PACE-signering, som brugeren selv skal oprette). Logic findes kun på Mac.

---

## 2026-11-08 (7) — Version 1.3: nyt navn "Pitch Bend Texture", "0 ms" fjernet, Sync-teksten kan ses helt

**Status:**
- Bygget som **"Pitch Bend Texture 1.3"** (plugin-kode D103, stadig 12 parametre). Ingen fejl eller advarsler.
- **`HostSmoke --compare` mod 1.2:** bit-identisk i alle 14 situationer ("SAMME LYD").
- **Røgtesten er OK:** alle former, tilstande, Hz og Sync. Peak højst 0,88 for en indgang på 0,6. CPU 5,1 % af én kerne.
- **EditorShot:** billederne er set. Titlen er "PITCH BEND TEXTURE", "Sync" kan ses helt, knapperne hedder "24 / 48 / 96 dB/oct", og den nye linje i "?"-boksen er inden for boksen. Højreklik virker.
- **Installeret og verificeret med filhash** (`62F131FE…`, moduleinfo 1.3.0). Doppler Saturator 1.0 (`624E93D9…`), 1.1 (`C1FEA715…`) og 1.2 (`D0246096…`) ligger uændret ved siden af.
- **Ikke testet i en DAW.** Pluginnet dukker op under det nye navn, når DAW'en scanner igen.

**Ønsket:** se `KRAV.md` ("Version 1.3", brugerens ord): Sync-teksten skjult, nyt navn "Pitch shift Texture" (giver det mening?), hvad betyder "0 ms"?

**Lavet:**
1. **Hz / Sync:** LED og tekst står længere til venstre (LED x 10, tekst x 19 i stedet for 13 / 26, knap-egenskaben `compact`). Knapperne har samme størrelse.
2. **Navn:** Claude svarede, at "Pitch shift" normalt betyder en transposer (fast op/ned), mens pluginnet bøjer pitchen op og ned. Brugeren valgte **"Pitch Bend Texture"**. Ændret i `PRODUCT_NAME`, titlen i editoren og Ableton-vejledningen i højrekliksmenuen (bruger nu `JucePlugin_Name`). Mappe, repo, CMake-mål og klassenavne hedder stadig DopplerSat. Installeren finder nu det nyeste build uanset navn.
3. **"0 ms":** betød, at kanterne ikke giver forsinkelse (i Multiband Saturator skilte det dem fra Linear phase). Her er alle tre 0 ms, så det er fjernet fra knapperne og parameterens valg (indeks uændret, gemte sessioner virker). "?"-boksen har fået én linje: ingen af de tre giver forsinkelse; pluginnet som helhed forsinker 1,4 ms, som DAW'en udligner.

**NÆSTE GANG:**
- Scan plugins igen, og find **Pitch Bend Texture 1.3**. Test som skrevet under 2026-11-05 (3): 96 med Sine 0,1–0,5 Hz på bas / pad, Bypass, automation.
- Stadig åbent: 192 dB/oct og latensen (anbefaling: vent / behold 4x).

---

## 2026-11-08 (6) — Anbefaling

- Brugeren skrev: "hvad anbefaler du mig at gøre?"
- Anbefalet: test 1.2 i en DAW først (aldrig gjort), vent med 192 dB/oct, behold 4x oversampling. Næste udviklingsskridt besluttes ud fra, hvad brugeren hører.
- Ingen kodeændringer.

---

## 2026-11-08 (5) — Forklaret: niveausving / smøring ved 192, og hvad det koster at fjerne 4x oversampling

- Brugeren skrev: "to kontraspørgsmål: 1) Hvad betyder: "Til gengæld svinger niveauet +5 til +11 dB, og bassen bliver smurt ud i op til 0,5 s."? 2) [...] Hvilken konsekvens har det at fjerne 4x oversampling?"
- Forklaret ud fra målingerne i 2026-11-05 (2) og 1.1: niveauet stiger med pitchen (192: +5 dB ved 300 Hz, +11,3 dB ved 100 Hz); gruppeforsinkelsen ved båndets nederste kant (192: 88 ms ved 212–424 Hz, 466 ms ved 40–80 Hz) får bassen i fx en kick til at komme efter klikket.
- 4x væk: 0 latens og ca. halv CPU (oversamplingen er ca. halvdelen), men de øverste delefiltre kan ikke over 20,7 kHz, så diskanten ændrer sig og pitchen holder op, når båndet ligger højt; bånd 2 og 3 ville også parkeres ved ca. 19–22 kHz i stedet for 77–86 kHz. Ingen aliasing-grund mere (ingen saturation siden 1.1). DAW'en udligner de 60 samples selv. Ikke målt i denne omgang.
- Ingen kodeændringer.

---

## 2026-11-08 (4) — Status fortalt

- Brugeren skrev: "godt, fortæl mig, hvor vi kom til sidst".
- Fortalt status fra "Gem og luk" 2026-11-05 (3): 1.2 installeret, ikke testet i DAW, de to åbne spørgsmål (192 dB/oct, latens).
- Ingen kodeændringer.

---

## 2026-11-08 (3) — Copilot-chats i VS Code findes ikke; udviklingen kan fortsætte i desktop-appen

- Brugeren skrev: "ja gør det" (kig efter Claude-chats i VS Codes `workspaceStorage`) og derefter: "kan jeg fortsætte udviklinen af mit plugin her på samme vis som i Claude inde i Visual Studio code 2026?"
- **Fundet:** VS Code (`%APPDATA%\Code`) blev kun åbnet én gang, 2025-10-11 kl. 14:23–14:25. Der er én tom chat (ingen beskeder), ingen udvidelser og ingen Copilot-historik. Alle rigtige Claude-samtaler ligger i `C:\Users\sofus\.claude\projects\` (terminal-Claude Code, "cli").
- **Svar:** ja. Desktop-appen er samme Claude Code: den læser `CLAUDE.md`, `FREMDRIFT.md`, `KRAV.md` og hukommelsen, kan bygge, teste, installere, committe og pushe som før.
- Ingen kodeændringer.

---

## 2026-11-08 (2) — Hele Programmering-mappen er tilføjet sessionen

- Brugeren skrev: "Mappe Programmering tilføjet, så du har adgang til alt i mapperne til furure reference".
- Claude kan nu læse og skrive i hele `C:\Users\sofus\Documents\Programmering`: `BudgetApp`, `DelayDirectionPlugin`, `DopplerSat`, `Ideer`, `JUCE` og `MultibandSat`. Arbejdsmappen er nu `Programmering`, men DopplerSat er stadig projektet, og logbogen er stadig denne fil.
- Ingen kodeændringer.

---

## 2026-11-08 — Opstart: hvad er projektet, og kan Claude læse chathistorikken fra VS Code?

- Brugeren skrev: "Fortæl mig hvad det her er, og kan du tilgå chathistorikken anvendt i Claude inde i Visual Studio code 2026?"
- Første session i Claude-desktop-appen (Code-fanen). Fortalt brugeren, hvad Doppler Saturator er, og hvor vi kom til (1.2, se posten nedenfor).
- **Chathistorikken:** Claude Code gemmer alle samtaler som `.jsonl` i `C:\Users\sofus\.claude\projects\<mappenavn>\`, uanset om de kom fra terminalen i VS Code, VS Code-udvidelsen eller desktop-appen. Claude kan læse dem. Der ligger 2 for DopplerSat (den store fra 2026-11-05), 30 for MultibandSat, 4 for `source\repos` og 2 for BudgetApp.
- Ingen kodeændringer.

---

## 2026-11-05 (3) — Gem og luk

**Status ved lukning:**
- Brugeren skrev: "gem og luk, og fortæl mig næste gang, hvor vi er kommet til".
- Seneste version er **Doppler Saturator 1.2**.
  - Den er installeret og verificeret igen ved lukning: den installerede fil er identisk med buildet (`D0246096…`).
  - Buildet er fra 22:36:11, efter sidste kildeændring kl. 22:35:47.
  - 1.0 og 1.1 ligger uændret ved siden af.
- Alt er committed og pushet. Arbejdsmappen er ren.
- **Ikke testet i en DAW.**

**NÆSTE GANG (fortæl brugeren dette ved opstart):**
- **1.2 er det nyeste.** Det, der kom med:
  - en tredje knap, **0 ms - 96 dB/oct**: næsten 3 gange så meget pitch som 48 ved langsom Rate og 1,1–1,5 gange ved hurtigere Rate;
  - **Bypass** flugter nu med midten af den første 0 ms-knap.
  - 24 og 48 lyder bit for bit som før.
- **Scan plugins igen** i Ableton og FL Studio, og prøv:
  - 96 med Sine ved 0,1–0,5 Hz og Amount 100 % på en bas eller et pad. Er niveausvinget og smøringen ved båndets nederste kant o.k.?
  - at Bypass sidder rigtigt;
  - at Wobble 2 stadig virker. Den fungerede ifølge brugeren, og Steinberg-stien er væk fra FL Studio.
- **Spørg:**
  - Skal **192 dB/oct** med som fjerde knap? Den giver 2,5 gange så meget pitch som 96 ved langsom Rate, men kun 20–30 % mere ved normal Rate. Den svinger +5 til +11 dB i niveau og smører bassen op til 0,5 s. Se posten nedenfor.
  - Skal latensen (1,4 ms) fjernes? Se 1.1.

---

## 2026-11-05 (2) — Version 1.2: tredje kant "0 ms - 96 dB/oct", Bypass flugter med den første 0 ms-knap

**Status:**
- Bygget som **"Doppler Saturator 1.2"** (plugin-kode D102, stadig 12 parametre). Ingen fejl eller advarsler.
- **`HostSmoke --compare` mod 1.1:** bit-identisk i alle 14 situationer (24 og 48 lyder som før).
- **Røgtesten er OK**, også for 96: alle former, Hz og Sync. Peak højst 0,88 for en indgang på 0,6, hvor grænsen er 1,25. Ingen NaN.
- **EditorShot:** billederne er set, og højreklik virker. `doppler3.png` viser 96 valgt.
- **Installeret og verificeret med filhash** (`D0246096…`, moduleinfo 1.2.0). Buildet er fra 22:36:11, efter sidste kildeændring kl. 22:35:47. 1.0 (`624E93D9…`) og 1.1 (`C1FEA715…`) ligger uændret ved siden af.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.2", brugerens ord):
1. Bypass skal flugte med midten af den første 0 ms-knap (referencebillede `design/1.1/LayoutV1.1.png`, nu i repoet).
2. Endnu en 0 ms, der er stejlere og pitcher mere: "Hvad er maksimalt du kan gøre det?"
- Brugeren skrev også, at Wobble 2-flytningen "fungerer godt".

**Fundet** (måleprøve med DopplerCore direkte, uden vært; den gav præcis 1.1's pitch-tal for 24 og 48):
- Motoren kan bygge kanter op til 192 dB/oct med de filtre, den har (`sectionInfo` op til orden 16).
- **Pitch ved samme bevægelse** (300 Hz, 0,25 Hz, Amount 100 %): 24 → 16, 48 → 49, 96 → 131, 192 → 329 cent. Hver fordobling giver ca. 2,5–3 gange så meget.
- **Uden fartgrænse** ved 1 Hz: 48 → 202, 96 → 559, 192 → 1514 cent. Men niveauet hopper.
- **Niveauhop ved Square 20 Hz** (to toner, peak 0,6, værste af fire båndplaceringer):
  - 24 @ 40 okt/s: 0,68;
  - 48 @ 20 okt/s: 0,75;
  - 96 @ 10: 0,79, @ 20: 1,01, @ 160: 4,8;
  - 192 @ 5: 0,72.
- Derfor gælder samme regel som før: dobbelt så stejl, halv fart. **96 har fartgrænsen 10 okt/s.**
- **Niveauet svinger med pitchen, uanset stejlhed.** En tone midt i båndet bliver ca. 1,5 dB kraftigere for hver 200 cent ved 300 Hz, mest når pitchen går op:
  - 48: 201 cent, −1,2 / +1,5 dB;
  - 96: 295 cent, −2,0 / +2,9 dB;
  - 192 @ 5: 358 cent, −3,0 / +5,0 dB.
  - Ved 100 Hz: 48 giver 615 cent og +5,4 dB, 96 giver 914 cent og +9,7 dB, og 192 giver 1186 cent og +11,3 dB.
  - Det er en del af effekten, ikke en fejl. Uden fartgrænse gav en glat sinus med 192 allerede +5 dB ved 0,25 Hz.
- **Filterstrukturen:** en TPT-SVF-allpass er det samme filter i ro (forskel ca. 6e-5).
  - Den fjerner eksplosionerne ved ekstreme hastigheder: ved 320–1000 okt/s får biquads peaks på 7–27, SVF kun ca. 1,1–1,5.
  - Ved de hastigheder, vi tillader (10–40 okt/s), giver den de samme niveauhop. Derfor er den ikke brugt.
- **Forsinkelse omkring kanten** (største gruppeforsinkelse i bånd 1's allpass; det smører fx en kick ved båndets nederste kant):

| bånd | 48 | 96 | 192 |
|---|---|---|---|
| 212–424 Hz | 21 ms | 41 ms | 88 ms |
| 40–80 Hz | 110 ms | 218 ms | 466 ms |

  - Den er der også, når båndet står stille.
- **Konklusion om "maksimalt":** 192 kan bygges. Men når niveauhoppene skal holdes nede, vokser pitchen ved fartgrænsen kun ca. 30 % pr. fordobling. Ved 300 Hz er den 163 → 228 → 295 → 358 cent for 24 → 48 → 96 → 192.
  - Smøringen fordobles for hvert trin, og niveauudsvinget vokser.
  - Den store gevinst ligger ved langsom Rate.
- **CPU:** kernen alene koster præcis dobbelt så meget for hvert trin: 1,15 / 2,4 / 5,0 %. Hele processBlock med 96 koster 7,2 % af én kerne (målt som den første tilstand).
  - **Faldgrube:** i `DspBench` bliver de tilstande, der måles efter den første, målt op til 2 gange for højt, også oversamplingen, der slet ikke afhænger af tilstanden. Mål den tilstand, der skal sammenlignes, først, eller brug en separat proces.

**Pitch-tabel fra pluginnet** (HostSmoke, cent, Amount 100 %, Width 1 okt, båndet om tonen):

| tone, Rate | 24 dB | 48 dB | 96 dB | 96/48 |
|---|---|---|---|---|
| 300 Hz, 0,25 Hz | 16 | 48 | 131 | 2,70 |
| 300 Hz, 1 Hz | 64 | 201 | 295 | 1,46 |
| 300 Hz, 3 Hz | 162 | 228 | 294 | 1,29 |
| 300 Hz, 6 Hz | 154 | 185 | 201 | 1,08 |
| 300 Hz, 12 Hz | 110 | 152 | 186 | 1,22 |
| 100 Hz, 1 Hz | 196 | 620 | 917 | 1,48 |
| 1 kHz, 1 Hz | 20 | 59 | 85 | 1,45 |
| 3 kHz, 1 Hz | 7 | 20 | 29 | 1,39 |
| 8 kHz, 1 Hz | 5 | 13 | 15 | 1,15 |

**Bygget:**
1. **`SatDsp.h`:**
   - `kPhaseZl96` er tilføjet, og `zlN6` (24 → 4, 48 → 8, 96 → 16) bruges af `zlSpec`.
   - Murens afstand er stadig 5 / n6 oktaver.
   - Kopien afviger nu fra Multiband Saturator (der kun bruger 24 og 48).
2. **Processoren:**
   - `mode` har tre valg ("0 ms - 96 dB/oct" er tilføjet sidst, standard er stadig 48).
   - `modePhase` og `modeMaxSpeed` (40 / 20 / 10 okt/s), `kNumModes` = 3.
   - Skift af tilstand starter stadig motoren forfra (dry under opvarmningen).
3. **Editoren:**
   - Tre knapper under hinanden. De to gamle ligger, hvor de lå (y 34 / 64), og 96 ligger ved y 94.
   - "Edge steepness" er blevet lavere (96 px). Aksen går til −110 dB, viser −24 / −48 / −96 dB, og kurverne slutter ved +1 oct, så hver ender ved sit tal (før krydsede de tallene).
   - **Bypass** sættes efter midten af `modeButtons[0]` (7 px højere end før), så den følger med, hvis knapperne flyttes.
   - "?"-teksten, værktøjstips og Rate-tippet (10 okt/s for 96) er opdateret.
4. **Tests:**
   - HostSmoke finder selv antallet af tilstande, så den virker med både 1.1 og 1.2. Pitch-tabellen har fået en 96-kolonne og 96/48.
   - Røgtesten kører alle former × alle tilstande × Hz/Sync, i alt 24 trin.
   - DspBench har tre tilstande og sætter `mode` med `convertTo0to1`. Før blev 1.0 sendt rå, og med tre valg ville det have givet 96.
   - EditorShot viser 96 i `doppler3`.

**Åbent / til brugeren:**
- Skal **192 dB/oct** med som fjerde knap ("Extreme")? Målt koster den (se Fundet):
  - +5 dB niveausving ved 300 Hz og +11 dB ved 100 Hz;
  - op til 0,1–0,5 s smøring i bassen;
  - kun ca. 20–30 % mere pitch end 96 ved middel og hurtig Rate, men 2,5 gange så meget ved langsom Rate.
- Latensen (se 1.1).

**NÆSTE GANG (fortæl brugeren dette ved opstart):**
- Seneste version er **Doppler Saturator 1.2**. Den er installeret ved siden af 1.0 og 1.1, men ikke testet i en DAW. Scan plugins igen i Ableton og FL Studio.
- Prøv **0 ms - 96 dB/oct**:
  - Sine ved 0,1–0,5 Hz med Amount 100 % på en bas eller et pad, hvor forskellen til 48 er størst;
  - om niveausvinget og smøringen af en kick ved båndets nederste kant er o.k.;
  - at Bypass flugter med 24 dB-knappen.
- Spørg, om 192 dB/oct skal med (se Åbent).

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
