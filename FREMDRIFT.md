# Fremdrift

Logbog over arbejdet på Doppler Saturator. Nyeste øverst. Claude skriver en ny post **hver gang** noget er bygget, så en ny session altid kan fortsætte præcis herfra. (Multiband Saturator har sin egen logbog i `../MultibandSat/FREMDRIFT.md`.)

---

## 2026-11-13 (6) — Version 1.14: fejl i Sweep-displayet rettet (Offset), træk op/ned i tallene

**Status:**
- Bygget som **"Pitch Bend Texture 1.14"** (D114), uden fejl eller advarsler. **Installeret og verificeret med filhash** (`DC20B8B6…`, moduleinfo 1.14.0). Buildet (14:03:19) er nyere end sidste kildeændring (14:02:10). 1.0–1.13 ligger uændret ved siden af.
- Røgtest og DAW-test: OK (højeste peak 1,11 for 0,6; halen højst 1,45 s). EditorShot (alle tests) og `--reach`: OK.
- `--compare` mod 1.13 er bit-identisk i alle trin uden Offset. "Random 2 Hz, offset −40" er forskellig, fordi området skubbes efter den brugte del. De næste trin bærer forskellen med (båndet glider tilbage), indtil Bypass nulstiller.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.14"). Brugeren skrev: "Super. Det fungerer. Der er nogle bugs i sweep-displayet [...] Når man skriver 20 hz i laveste kant, så forsvinder tallet for øverste kant, og bandet bevæger sig ikke. 2) [...] Ved at trække lodret op og ned ud for hver tal i sweepdisplayet, kan man justere tallet".

**Fejlen:** brugerens billede viste Offset 19,1 %. 1.13 skubbede hele det symmetriske område ind, også den del, LFO'en aldrig bruger med Offset. Den viste bund kunne derfor ikke blive 20 Hz med positivt Offset. `sweepForEnds` lod så det skrevne vinde ved at sætte h = 0, så Sweep blev 0 %, toppen forsvandt, og båndet stod stille.

**Lavet:**
1. **`centreOctFor (freq, sweep, offset, v)`:** midten c klippes til [log2 20 + a·W/2, log2 20000 − b·W/2] med a = −max(−1, offset − 1) og b = min(1, offset + 1). Kun den brugte del skal ligge i spektret. Uden Offset er det som 1.13 (bit-identisk).
2. **`sweepForEnds`:** h = (top − bund)/(a + b), c = bund + a·h. Det kan altid laves, hvis 2h ≤ log2 1000. Ellers (kun med Offset) er h = maks., og den urørte ende står (svar 2). Sweep bliver aldrig 0 %, når bund < top.
3. **Træk** (`LcdText::beginDrag` / `dragBy` / `endDrag`):
   - Et klik registrerer tallet. Ved træk over 3 px ændres det med 0,02 okt/px, eller 0,002 med Shift, og op er højere. Værdien holdes for sig selv, så Shift kan skiftes midt i trækket, og den holdes inden for 20 Hz–20 kHz.
   - Hvert trin går gennem `DopplerSatEditor::applySweepHz` (fælles med skrivning). Hele trækket er én gestus (`onDragGesture`). Klik uden træk åbner feltet.
   - Viser displayet kun ét tal (Sweep 0 %), betyder træk op toppen og træk ned bunden.
   - Musen er en op/ned-pil, og tooltippen forklarer træk og skrivning.
4. **Test (EditorShot `bugAndDragTest`, offentlige `testSweepDrag`, `applySweepHz`, `sweepEndsNow`):**
   - Offset 19,1 %, bund 20 Hz: 20,0 Hz – 1691,0 Hz (toppen står).
   - Brugerens tilstand (20 Hz, Sweep 0 %), top 1 kHz: 20 Hz – 1 kHz.
   - Top 20 kHz med Offset 19,1 % (for bredt): bunden står, og toppen bliver 10,3 kHz (største område).
   - Træk bund 50 px ned halverer, top 50 px op fordobler, og med Shift er det 2^0,1. Bund forbi toppen gør intet. Sweep 0 % og træk op giver 600 Hz – 1200 Hz.
   - Skrivetesten: den urørte ende står altid. Svar 2 (53 gange, kun med Offset) giver det største område.

**NÆSTE GANG:** test 1.14 i en DAW: træk i tallene, skriv 20 Hz med og uden Offset. Hvis det ikke fungerer: mind brugeren om, at 1.10 fungerede godt. Band Size-displayet venter ("Efter 1.14").

---

## 2026-11-13 (5) — Version 1.13: symmetrisk Sweep (pilen i midten), båndet glider ud over enderne

**Status:**
- Bygget som **"Pitch Bend Texture 1.13"** (D113, 13 parametre), uden fejl eller advarsler. **Installeret og verificeret med filhash** (`A9B3CB78…`, moduleinfo 1.13.0). Buildet (13:39:01) er nyere end sidste kildeændring (13:37:59). 1.0–1.12 ligger uændret ved siden af.
- Røgtest og DAW-test: OK. Højeste peak ved fuld fart 1,11 for en 0,6-indgang.
- Halen er længst 1,45 s (20 Hz, Band Size 2 okt, 96 dB; kanten går nu ned til 13 Hz). Pluginnet melder 2 s.
- `EditorShot --reach`: midten når præcis 20 Hz – 20 kHz ved alle former og stejlheder, 1 og 20 Hz.
- **Ikke testet i en DAW.**
- **Brugeren skrev undervejs: "hvis det ikke fungerer, så mind mig om, at v. 1.10 fungerede godt. Så snakker vi om det."** → Virker 1.13 ikke for brugeren, så mind dem om 1.10 (Sweep ±3 okt, kanter klippet ved 20 Hz / 20 kHz, skrivning i displayet).

**Ønsket:** se `KRAV.md` ("Version 1.13"): pilen i midten, når det er muligt (punkt 1), og bevægelsen som om enden ikke var der (punkt 3). Punkt 2 (farten i Sine) løses af de to.

**Lavet:**
1. **`centreOctFor`:**
   - Bredde W = s · log2 1000 oktaver. Midten c = Frequency klippet til [log2 20 + W/2, log2 20000 − W/2], og midten for LFO-værdien v er c + v·W/2. Så er farten den samme begge veje.
   - `maxSideOct` = W/2 (fartgrænsen).
   - Standard 30 % ≈ 213 Hz – 1,69 kHz ved 600 Hz.
2. **`bandEdgesFor`:** kanterne klippes ved `kEdgeMinHz` = 13 Hz og `kEdgeMaxOfProc` = 0,30 × den oversamplede rate i stedet for 20 Hz / 20 kHz. Ved Sweep 100 % og Band Size 1 okt går de ud til 14,1 Hz og 28,3 kHz. Band Size-displayet viser de rigtige kanter.
3. **`sweepForEnds`:** h = (top − bund)/(a + b), og c = bund + a·h. Det kan laves, hvis c ± h ligger i spektret og 2h ≤ log2 1000; så er Frequency = c. Ellers gælder den skrevne ende, og h gøres så stor, som der er plads til (svar 2). Uden Offset sker det aldrig.
4. **Tooltip:** "how wide the area is that the LFO moves the band across, centred on Frequency (100 % = the whole spectrum [...]) Near an end of the spectrum the area is pushed inwards, so it keeps its width."
5. **Test:**
   - HostSmoke: halen testes også med Band Size 2 og 6 okt.
   - EditorShot `centreTest`, uden Offset: 23 tilfælde med pilen i midten og 31 skubbet ind med samme bredde, og båndet går ud til 14,1 Hz / 28,3 kHz.
   - Skrivetesten: 369 tilfælde, og den anden ende flyttede sig 0 gange uden Offset. Med Offset 102 gange, fordi Offset kun bruger en del af området; den skrevne ende nås altid.
   - Feltet: 180 giver 180,0 Hz, og toppen står på 1691,0 Hz.

**Pitch ved Sweep 100 %** (HostSmoke, 300 Hz, Width 1 okt):

| | 24 | 48 | 96 |
|---|---|---|---|
| 1 Hz | 125 | 358 | 950 cent |
| 12 Hz | 1114 | 1847 | 2974 cent |

**NÆSTE GANG:** test 1.13 i en DAW: Sine ved Sweep 100 %, pilen i midten, skriv i displayet. Hvis det ikke fungerer: mind brugeren om, at 1.10 fungerede godt. Band Size-displayet venter ("Efter 1.13").

---

## 2026-11-13 (4) — Spørgsmål: pilen i midten, og er farten ujævn i Sine? (svar, intet bygget)

- Brugeren vil have Frequency-pilen midt mellem Sweep-områdets yderkanter, når det er muligt. De spurgte også, om det er kunstigt, at båndet i Sine ser ud til at sætte farten ned ved kanterne. Se `KRAV.md` ("Efter 1.12").
- **1) Pilen:** i 1.11/1.12 er Sweep en andel af vejen til hver ende, så siderne er forskellige, når Frequency ikke står ved ca. 630 Hz. Ved 100 Hz og Sweep 50 % går området fx fra 45 Hz til 1,4 kHz (midten 250 Hz).
  - **Bud:** Sweep = samlet bredde (s · log2 1000 ≈ 10 okt), symmetrisk om Frequency, skubbet ind hvis det rammer en ende.
  - Pilen står i midten, når der er plads, og farten er den samme begge veje. 100 % er stadig hele spektret, og 30 % ≈ 213 Hz – 1,69 kHz.
  - Skrivning: Frequency = midten af det skrevne, Sweep = bredden.
  - Pris: nær 100 % kan Frequency næsten ikke flytte området, og med Offset kan ikke alle områder skrives (Offset bruger kun en del af området).
- **2) Farten:**
  - Sinus er langsomst ved yderpunkterne af natur (90 % ude: 44 % af topfarten; som et pendul og som Abletons LFO). Jævn fart = Triangle.
  - **Kunstigt i 1.11/1.12:** forskellig fart på de to sider og et brat skift ved Frequency. Det forsvinder med buddet.
  - **Kunstigt:** `bandEdgesFor` klipper kanterne ved 20 Hz / 20 kHz, så ved Sweep 100 % står den yderste kant stille, mens midten fortsætter. Med Sine og Band Size 1 okt sker det ca. 30 % af tiden (|sin| > 0,9). Mulig løsning: midten stopper en halv Band Size før enden. Så viser Sweep-displayet fx 28 Hz – 14 kHz ved Band Size 1 okt.
  - Fartgrænsen (mindst 1,25 × LFO'ens topfart) og Smooth Peaks bremser ikke.
- Undervejs, punkt 3: "Selvom den ene kant af Sweep travel range rører ved en ende af spektrummet, så skal bevægelsen mellem de to punkter i travel range stadig ske, som hvis ikke den ende kant rører ved en ende af spektrummet".
- **Bud for punkt 3:**
  - Området skubbes ind i stedet for at blive klippet (punkt 1).
  - Båndets kanter må gå ud over 20 Hz / 20 kHz, som om spektret fortsatte: `bandEdgesFor` klipper i stedet ved ca. 13 Hz (over bånd 0's 10–12 Hz) og ca. 0,3 × den oversamplede rate (≥ 53 kHz; bånd 2 starter ved 0,40).
  - Det kan motoren: `makeLayout` lægger murene ned til 5 Hz og op til 0,47 · fs, og `bandEdges` klipper bånd 1 ved bånd 0's top. Det uden for spektret ses og høres ikke, så bevægelsen ser og lyder som midt i spektret.
  - Band Size-displayet viser de rigtige kanter (fx 14 Hz).
  - "Kant-stop" er fravalgt: Sweep-området ville afhænge af Band Size, imod KRAV 1.6.

---

## 2026-11-13 (3) — Version 1.11 og 1.12: Sweep 100 % = hele spektret

**Status:**
- **1.12 er den, der skal bruges.** Bygget som "Pitch Bend Texture 1.12" (D112, 13 parametre), uden fejl eller advarsler. **Installeret og verificeret med filhash** (`0114FC80…`, moduleinfo 1.12.0). Buildet (12:50:54) er nyere end sidste kildeændring (12:49:56).
- **1.11** (`E826A2C9…`) blev installeret, før jeg så, at tal-felterne under Frequency og Sweep viste 7 decimaler ("600.0001221 Hz", "30.0000019 %"). Det har JUCE som standard for parametre uden trin. Efter reglen (hvert installeret nummer er en version, intet slettes) er rettelsen 1.12. 1.11 ligger der stadig, og ellers er 1.12 som 1.11.
- 1.0–1.10 ligger uændret ved siden af.
- Røgtest og DAW-test (alle 13 parametre, gem/hent, samplerater, blokke, hale): OK. Højeste peak ved fuld fart 1,14 for en 0,6-indgang (som 1.9), og de værste toppe er +5,6 dB peak / +1,8 dB RMS.
- `EditorShot --reach`: båndets midte når 20 Hz – 20 kHz (de klippede kanters midte 21,8 Hz – 18,3 kHz) ved 1 og 20 Hz for alle former og stejlheder.
- `--compare` mod 1.10: "FORSKELLIG LYD", som forventet (Sweep betyder noget andet). CPU som 1.10: ca. 2,4 % standard og 2,8–3,0 % ved 12 Hz, Sweep 100 %.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.11 / 1.12"). Brugeren skrev: "Redigering af Sweep fungerer efter hensigten [...] den ene side trækkes med af den anden, hvis afstanden bliver for stor. Vi skal derfor have bygget, at 100% travel range går helt ud til kanterne".

**Lavet:**
1. **`DopplerSatProcessor::centreOctFor (freq, sweep, v)`:** midten = log2 F + v·s·(log2 F − log2 20) for v < 0 og log2 F + v·s·(log2 20000 − log2 F) for v ≥ 0. `bandEdgesFor` og `bandCentreFor` bruger den. `kMaxSweepOct` er væk.
2. **Fartgrænsen** bruger den længste side (`maxSideOct`). Ved 600 Hz, Sweep 100 % er den ca. 5 oktaver, så Square 20 Hz kan nå ca. 2000 okt/s. TPT-SVF'erne klarer det (ingen NaN eller niveauspring i røgtesten).
3. **Standard-Sweep 30 %.** **Frequency og Sweep har ingen trin** (`NormalisableRange` interval 0). Med trin på 0,1 % flyttede den faste ende sig op til 1 %, når man skrev nær spektrets ender (19 af 384 testtilfælde). 1.12: `withStringFromValueFunction` med én decimal.
4. **`sweepForEnds` er regnet om:** bund = Fo − a·s·(Fo − L20), top = Fo + b·s·(H20 − Fo), hvor a = −v1 og b = v2 (Offset). Med den ene ende fast findes s ved halvering, og Fo regnes fra den faste ende. Svar 0 = afvist, 1 = OK, 2 = den anden ende måtte flyttes. Fordi den ene af a og b altid er 1 (Offset ≥ 0 giver b = 1, ≤ 0 giver a = 1), kan alle områder mellem 20 Hz og 20 kHz laves, så svar 2 forekommer ikke.
5. **Test (EditorShot):** 384 tilfælde (5 Offset, 8 indstillinger inkl. 20 Hz og 20 kHz, bund og top, 6 faktorer inkl. ud over spektret). Begge ender rammer inden for 0,1 %, og den anden ende blev aldrig flyttet. "Bund 20 Hz, så top 20 kHz" giver 20 Hz – 20 kHz og Sweep 100 %. Feltet med 180 giver 180,0 Hz, toppen står på 1718,0 Hz. Rækkeviddetesten måler mod `bandEdgesFor (±1)`.
6. Sweep-tooltippen: "how far the LFO moves the band from Frequency toward each end of the spectrum (100 % = all the way down to 20 Hz and up to 20 kHz)".

**Pitch ved Sweep 100 %** (HostSmoke-tabellen, 300 Hz, Width 1 okt; båndet rejser nu længere):

| | 24 | 48 | 96 |
|---|---|---|---|
| 1 Hz | 148 | 433 | 1086 cent |
| 12 Hz | 1345 | 2060 | 3373 cent |

**NÆSTE GANG:** test 1.12 i en DAW: Sweep 100 % hele vejen ud, standardlyden, skriv i displayet. Derefter Band Size-displayet (venter, se KRAV "Efter 1.12").

---

## 2026-11-13 (2) — Version 1.10: skriv Sweeps bund og top ind i displayet

**Status:**
- Bygget som **"Pitch Bend Texture 1.10"** (D110, 13 parametre). Ingen fejl eller advarsler.
- **Installeret og verificeret med filhash** (`DFF6A954…`, moduleinfo 1.10.0). Buildet (11:55:53) er nyere end sidste kildeændring (11:54:56). 1.0–1.9 ligger uændret ved siden af.
- **`HostSmoke --compare` mod 1.9: "SAMME LYD"**, bit-identisk i alle 16 trin. Kun editoren er ændret.
- Røgtest og DAW-test: OK. EditorShot med den nye skrivetest: OK.
- **Ikke testet i en DAW.** Det er især værd at tjekke, at tastaturet når feltet: nogle DAW'er tager tasterne selv.

**Ønsket:** se `KRAV.md` ("Version 1.10"). Brugeren skrev: "vi tager lige én ting ad gangen. Prøv denne", og valgte bud 1 (Sweep-displayet). Band Size-displayet og Sweep 100 % venter ("Efter 1.10").

**Lavet:**
1. **`LcdText` kan skrives i** (`onTyped`, `setEditable`, `beginEdit`). Et klik på et tal (venstre for midten af " - " = bunden) åbner et `juce::TextEditor` over netop det tal, med teksten markeret. Enter bruger tallet, Esc fortryder, og Tab (`Field::keyPressed`) bruger det og åbner det andet. Fokus væk eller et nyt klik på displayet bruger det også. Uændret tekst gør intet, så det afrundede tal ikke flytter enden. Feltet slettes asynkront. Musen er en tekstmarkør over Sweep-displayet, og tooltippen forklarer det.
2. **`parseHz`:** "180", "180 Hz", "1.2k", "1,2 kHz" (komma = punktum, k = tusind).
3. **`DopplerSatProcessor::sweepForEnds`:** midtens vej er log2 F + v·A for v fra v1 = max(−1, offset−1) til v2 = min(1, offset+1). A = (log2 top − log2 bund)/(v2 − v1) rundes til Sweeps trin (0,1 %), og F regnes fra den ende, der skal stå. Bliver A over 3 oktaver, regnes F fra den skrevne ende, og den anden flyttes. Bund ≥ top giver false. Bevist: F holder sig inden for 20 Hz–20 kHz.
4. **`DopplerSatEditor::applySweepText`:** bruger displayets ender, som de vises (Offset regnet med, og klippet til 20 Hz–20 kHz, så det viste tal står stille). Viser displayet kun ét tal (Sweep 0), er et lavere tal en ny bund og et højere en ny top. Frequency og Sweep sættes som én gestus (begin/endChangeGesture).
5. **Test (EditorShot):**
   - 170 tilfælde (Offset 0 / +40 / −60 / +100, seks indstillinger, bund og top, fire faktorer): den anden ende står inden for 0,3 %, og den skrevne nås inden for 0,5 %.
   - For bredt (bund 20 Hz ved 212 Hz–1,70 kHz) giver 20 Hz–1280 Hz.
   - Bund over top og "abc" afvises.
   - "1,2 kHz" og "2.5k" virker.
   - Den rigtige vej gennem feltet (Enter): bund 212,1 → 180,4 Hz, top 1697,1 → 1697,2 Hz.
   - Billeder: `doppler8_sweep_edit.png` (feltet åbent med "180") og `doppler9_sweep_typed.png` (bagefter).

**NÆSTE GANG:** test 1.10 i en DAW: klik, skriv, Enter / Tab / Esc, og automation-optagelse af Frequency + Sweep. Derefter næste ønske: Band Size-displayet eller Sweep 100 % = hele spektret.

---

## 2026-11-13 (1) — Ønske: skriv Sweep-områdets bund og top ind i displayet (bud, intet bygget)

- Brugeren vil kunne klikke på Sweep-displayet og skrive bund og top ind. De spurgte først, hvad vi gør, hvis man kun vil rette den ene ende, når Sweep-knappen i dag flytter begge. Se `KRAV.md` ("Efter 1.9").
- **Bud:**
  - Klik på det tal, du vil rette. Kun dét tal åbner som tekstfelt (Enter = brug, Esc = fortryd, Tab = det andet tal), og det andet tal bliver stående.
  - Bag kulissen er området altid Frequency ± Sweep (symmetrisk i oktaver, Offset regnet med). Ethvert par bund/top svarer derfor til præcis én Frequency og én Sweep. Pluginnet regner dem ud, og begge knapper drejer med. Der kommer ingen nye parametre, og lyden ændres ikke.
  - Bagefter virker knapperne som før.
  - Grænser: højst 6 oktaver (Sweep 100 %, mindre med Offset) og 20 Hz–20 kHz. Det skrevne tal gælder, og det andet flyttes kun så meget, som det er nødvendigt. Bund over top afvises.
  - Ikke anbefalet: separate Low/High-knapper. Det giver nye parametre og en anden Sweep, og det er ikke nødvendigt, fordi alle områder allerede kan laves med Frequency + Sweep.
- Undervejs: "Band size skal også kunne justeres ved, at brugeren kan skrive i displayet."
- **Bud for Band Size:**
  - Klik på det kant-tal, du vil rette. Kun Band Size ændres, og båndets midte bliver, hvor den er (den følger stadig LFO'en). Den anden kant flytter sig derfor lige så meget den modsatte vej, og Sweep røres ikke.
  - Under Pause rammer båndet præcis det skrevne. Når LFO'en kører, står tallet stille, mens man skriver, og den nye bredde gælder fra Enter. Grænser 0,1–6 oktaver.
  - Ikke anbefalet: at den anden kant står stille. Det kræver, at Frequency flyttes, og så flytter Sweep-området sig, når man retter Band Size.
  - Feltet under Band Size-knappen ("1.00 oct") kan man allerede klikke i og skrive bredden i oktaver. Det gælder alle knappernes tal-felter (JUCE-label, redigerbar med ét klik).
- Undervejs: "Sweep (travel range): 100% skal gå helt ud til hver ende af spektrummet."
- **Bud for Sweep 100 %:**
  - Sweep bliver en andel af vejen ud mod hver ende. Midtens vej går fra F · (20/F)^s til F · (20000/F)^s, hvor s = Sweep 0–1. LFO-værdier under 0 bruger den nedre side og værdier over 0 den øvre, så 100 % = 20 Hz – 20 kHz uanset Frequency, og Frequency er stadig midten.
  - Konsekvenser:
    - Står Frequency langt fra ca. 630 Hz, er den ene side længere. Båndet bevæger sig da hurtigere på den side, og pitchen bøjer mere dér; farten skifter ved midten.
    - Ved 100 % rejser båndet ca. 10 oktaver i stedet for 6, så hurtig Rate bøjer mere. Fartgrænsen skal bruge den længste side.
    - Standard 30 % giver ved 600 Hz 216 Hz – 1,72 kHz (i dag 212 Hz – 1,70 kHz).
    - Skrive-funktionen mister grænsen på 6 oktaver. s = log2(top/bund) / log2(1000), og Frequency regnes ud; med Offset findes de ved søgning.
  - Fravalgt: 100 % = ±10 oktaver symmetrisk. Så står båndet stille i enderne en stor del af tiden (ca. 2/3 ved 600 Hz).

---

## 2026-11-10 (11) — Version 1.9: Smooth Peaks altid til, nyt layout, mindre CPU

**Status:**
- Bygget som **"Pitch Bend Texture 1.9"** (D109, 13 parametre). Ingen fejl eller advarsler.
- **Installeret og verificeret med filhash** (`B0EB9272…`, moduleinfo 1.9.0). Buildet (20:44:30) er nyere end sidste kildeændring (20:43:00). 1.0–1.8 ligger uændret ved siden af.
- **`HostSmoke --compare` mod 1.8 med Smooth Peaks tændt: "SAMME LYD", bit-identisk i alle 16 trin.** Der er to nye trin, "Square 1 Hz (spring, saa ro)" og "Sine 0,3 Hz efter spring", hvor motorerne skilles og fortsætter hver for sig.
- Røgtest, DAW-test (alle 13 parametre, gem/hent, samplerater, blokke 0–4096, hale) og `EditorShot --reach`: OK.
- **Mac-kodevejen** (`build-shim`, `-DDS_FORCE_SIMD_SHIM=ON`) bygger, og `--compare` mod det almindelige 1.9-build giver "SAMME LYD", bit-identisk i alle 16 trin.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.9", brugerens ord): Smooth Peaks som standard uden knap, layoutet fra mockuppet, og mindre CPU, hvis det på ingen måde påvirker lyden eller kan ses i pluginnet.

**Lavet:**
1. **Smooth Peaks altid til:** parameteren `smooth` er væk (13 parametre). Hver version har sin egen plugin-kode, så 1.8-projekter røres ikke.
2. **Layout:** som `doppler9_mockup_balanced.png`. `layout::widths` = 0,33 / 0,41 / 0 / 0,26, og Edges-modulet har bredde 0. Edges-knapperne er en søjle i Band (`layout::edgeColW` 118, højde 20, midt for midt 25 px), med "Edges" og "?" øverst. `SlopeView`, Smooth Peaks-knappen og mockup-koden (`setMockupLayout`) er fjernet. Hjælpeteksten siger nu, at de korte toppe holdes højst 3 dB over en rolig version af effekten.
3. **CPU, bit for bit samme lyd:**
   - **Reference-motoren springes over, når den er ens med hovedmotoren** (`refSame`). Det gælder, når de har samme tilstand bit for bit og får de samme kanter, og så er dens lyd = hovedmotorens. Skilles de, kopieres hovedmotoren først (`refCore = core`), så reference-motoren fortsætter præcis, som hvis den havde kørt hele tiden. `DopplerCore::sameStateAs` tjekker hvert stykke med ens kanter, om de er blevet ens igen.
   - **Fælles h23:** `DopplerCore::processPair` regner begge motorer i én løkke. Bånd 2+3's faste allpasser regnes kun én gang, fordi de får samme lyd ind, har samme koefficienter og nulstilles samtidig.
   - **Målt** (`HostSmoke --benchone`, én måling pr. proces, 3 runder; senere målinger i samme proces kommer ud for højt, som DspBench-faldgruben):

     | | 1.8 fra | 1.8 Smooth | 1.9 |
     |---|---|---|---|
     | Sine 0,5 Hz, Sweep 50 % | 2,25 % | 3,58 % | 2,29 % |
     | samme efter Frequency-spring | – | 3,75 % | 2,79 % |
     | Sine 12 Hz, Sweep 100 % | 2,36 % | 3,71 % | 2,75 % |

   - **Hvornår springes den over?** (DspBench, de sidste 5 s)
     - 100 %: ved langsom Sine (0,5 Hz, også Sweep 100 % ved 96 dB), og efter Bypass.
     - 0 %: ved 2 Hz og 12 Hz, ved Square, efter et Frequency-spring, i Pause efter et spring, og efter 3 s stilhed.
     - Efter et spring forbliver de to motorer forskellige på sidste decimal, så reference-motoren regnes, indtil Bypass, skift af 24/48/96 eller DAW'en starter pluginnet forfra. Det koster så ca. 0,5 procentpoint, mod 1,4 i 1.8.
4. **Test:**
   - `HostSmoke --compare` giver den gamle Smooth Peaks til, når den nye ikke har parameteren.
   - Nye værktøjer: `--benchone <Rate> <Sweep> <Smooth> <spring>`. `--bench` måler nu også Sine 12 Hz, Sweep 100 %.
   - DspBench viser, hvor tit reference-motoren springes over.
   - EditorShot: doppler8–10-mockupperne er væk, og `doppler1` viser nu det nye layout.

**NÆSTE GANG:** test 1.9 i en DAW (det nye layout, og at lyden er som 1.8 med Smooth Peaks). Se hjemmesiden (dens plugin-billede er stadig fra 1.5). Beslut om Mac-buildet.

---

## 2026-11-10 (10) — Spørgsmål: er 3,8 % CPU meget for et plugin?

- Brugeren skrev: "er 3,8 % cpu meget for et plugin?"
- **Svar: nej, det er lavt til middel.** Tallet er procent af én kerne ved 44,1 kHz og 512-blokke (`HostSmoke --bench`). Ca. 25 instanser fylder én kerne, og DAW'en fordeler spor på flere kerner. Til sammenligning (groft, typiske tal): simple EQ'er under 1 %, kompressorer og saturatorer 1–5 %, reverbs og analog-modellering ofte 5–20 % eller mere. Ved 96 kHz og ved meget små buffere stiger det, ved 96 kHz ca. det dobbelte. Intet bygget.

---

## 2026-11-10 (9) — Spørgsmål: bliver forsinkelsen større med Smooth Peaks altid til?

- Brugeren skrev: "bliver forsinkelsen på pluginnet større, hvis smooth peaks altid er til?"
- **Svar: nej.** Latensen meldes kun ud fra oversamplingen (`setLatencySamples` i `prepareToPlay`): 60 samples (1,4 ms ved 44,1 kHz) med og uden Smooth Peaks. Dæmpningen har intet lookahead. Den regnes sample for sample ud fra reference-motoren, som kører samtidig med den rigtige motor på det samme oversamplede signal. Intet bygget.

---

## 2026-11-10 (8) — Spørgsmål om Smooth Peaks som standard, mockup med bedre balance

- Brugeren skrev, at de ikke kan høre forskel med og uden Smooth Peaks. De spurgte, om den skal være fast til og knappen forsvinde. De bad også om et nyt mockup uden Edge steepness, hvor afstandene er mere balancerede. Se `KRAV.md` ("Efter 1.8").
- **Svar (intet ændret i lyden):** ja, anbefalet. Toppene varer få ms og høres ikke som lydstyrke, men ses på peak-meteret og kan klippe efterfølgende plugins. Prisen er CPU 2,3 → 3,8 % hele tiden (måske mindre, hvis referencemotoren kun kører ved hurtig Rate; ikke målt), og hurtig Rate er ikke længere bit for bit som 1.7. Brugeren kan se forskellen på DAW'ens peak-meter: Rate 12–20 Hz, Sweep 100 %, knappen til og fra.
- **Mockup:** `DopplerSatEditor::setMockupLayout (variant, withSmooth)` (kun EditorShot bruger det; pluginnet er uændret). Edges-modulet er væk, og de tre knapper (højde 20, midt for midt 25 px) ligger som en søjle i Band, til højre for de to displays. Der er en "Edges"-label og "?" øverst. Øverste og nederste knaps midte flugter med de to displays, og Bypass flugter stadig med den første knap. Bredder 0,33 / 0,41 / 0,26, så drejeknapperne ligger næsten lige langt fra hinanden i alle tre moduler (112 / 108 / 102 px). Billeder: `doppler9_mockup_balanced.png` (uden Smooth Peaks) og `doppler10_mockup_balanced_smooth.png` (med, i rækken under Bypass).
  - Også prøvet: fire moduler med et smallere Edges-modul og knapperne fordelt over hele højden. Det gav store huller mellem knapperne og blev forkastet.

**NÆSTE GANG:** Hvis brugeren siger ja, bygges 1.9: Smooth Peaks altid til (parameteren `smooth` fjernes; hver version har sin egen plugin-kode, så gamle projekter påvirkes ikke), layoutet fra mockuppet bliver det eneste, og `SlopeView` og mockup-koden fjernes.

---

## 2026-11-10 (7) — Version 1.8: Smooth Peaks-knap, mockup uden Edge steepness

**Status:**
- Bygget som **"Pitch Bend Texture 1.8"** (D108, 14 parametre). Ingen fejl eller advarsler.
- **Installeret og verificeret med filhash** (`8750F4B4…`, moduleinfo 1.8.0). Buildet (18:46:56) er nyere end sidste kildeændring (18:46:49). 1.0–1.7 ligger uændret ved siden af.
- **`HostSmoke --compare` mod 1.7 (Smooth fra): "SAMME LYD"** (bit-identisk).
- Røgtest, DAW-test og `EditorShot --reach`: OK.
- **Ikke testet i en DAW, og Smooth Peaks er ikke lyttet til.**

**Ønsket:** se `KRAV.md` ("Version 1.8", brugerens ord): løsning 1 som en knap til A/B, og et mockup uden "Edge steepness"-tegningen.

**Lavet:**
1. **Smooth Peaks** (ny parameter `smooth`, sidst; knap med lilla LED under Bypass). Tre forsøg:
   - Kort RMS (2 ms) mod det tørre signal: toppene faldt kun 0–2 dB (de er kortere end vinduet). Forkastet.
   - Top mod det tørre signals top: toppene faldt til højst +5,6 dB, men en savtak ved 0,25 Hz blev ændret med op til −19 dB, fordi filtrene flytter rundt på overtonerne, også når båndet næsten står stille. Forkastet (et langsomt tophold af det våde signal hjalp ikke).
   - **Valgt:** en ekstra motor (`refCore`), hvor båndet flytter sig med den gamle fartgrænse (som 1.6). Det våde signal må højst nå 3 dB over dens toppe. Kører kun, når knappen er tændt.
   - Målt: savtakken ved 0,25 Hz er bit for bit uændret med Smooth tændt (alle tre stejlheder). Ved fuld fart: højeste peak 1,65 → 1,14 (indgang 0,6), dvs. +8,8 → +5,6 dB; RMS stort set uændret. CPU ved 512-blokke: 2,3 % fra, 3,8 % til.
2. **Mockup uden Edge steepness:** `doppler8_mockup_no_slope.png` (EditorShot skjuler tegningen) er vist brugeren. Tegningen er stadig med i 1.8.

**NÆSTE GANG:** test 1.8 i en DAW: A/B med Smooth Peaks ved høj Sweep og hurtig Rate (lyt efter forvrængning i dyb bas, dæmpningen går ned på 0,3 ms). Skal loftet være lavere end 3 dB? Skal Edge steepness fjernes? Se også hjemmesiden og beslut om Mac-buildet.

---

## 2026-11-10 (6) — Spørgsmål: hvad sker der med lyden, hvis toppene udjævnes?

- Brugeren skrev: "Hvad sker der med lyden, hvis du forsøger at udjævne?"
- Forklaret (intet bygget). Toppene i 1.7 er korte (peak op til +8,8 dB, RMS højst +2,1 dB) og kommer, når båndet farer forbi og presser lyden sammen (pitchen op). Muligheder:
  1. **Relativt loft** (anbefalet, evt. som knap): vådt signal må højst ligge fx 3 dB over det tørre; kun toppene dæmpes. Ved langsom Rate rører det intet (svinget er ca. 1,5 dB). Koster lidt "kant" på de hurtigste sving; en meget hurtig dæmpning kan give lidt forvrængning i dyb bas.
  2. **Fuld niveau-udligning:** både toppe og dyk glattes ud, så kun pitchen er tilbage. Effekten bliver fladere og mindre levende.
  3. **Limiter på udgangen:** fanger alt over et fast niveau, også det tørre signal; mindre præcis.
  4. Lavere fart: modsiger ønsket fra 1.7.
- Ikke målt endnu, hvordan det lyder. Tilbudt at bygge en version med en knap, så brugeren kan sammenligne.

---

## 2026-11-10 (5) — Version 1.7: båndet når helt ud i Sweep-området ved alle Rate-værdier

**Status:**
- Bygget som **"Pitch Bend Texture 1.7"** (D107). Ingen fejl eller advarsler.
- **Installeret og verificeret med filhash** (`D7967F77…`, moduleinfo 1.7.0). Buildet (16:57:47) er nyere end sidste kildeændring (16:57:40). 1.0–1.6 ligger uændret ved siden af.
- **Ikke testet i en DAW.**

**Ønsket:** se `KRAV.md` ("Version 1.7", brugerens ord): ved høj Sweep og høj Rate nåede båndet ikke ud i kanterne; det skal det altid, som Abletons LFO Tool (billede).

**Årsagen:** fartgrænsen på kanterne (40 / 20 / 10 okt/s). Ved Sweep 100 % og 20 Hz kræver Sine ca. 380 okt/s, Square ca. 1200 okt/s. Med de gamle biquads gav så hurtig bevægelse niveauspring (målt i 1.2).

**Lavet:**
1. **Båndets fire filtre er nu TPT-SVF-allpasser** (`SvfXover` i `DopplerCore.h`): det samme filter i ro, men stabilt ved hurtig bevægelse. Koefficienten glider sample for sample, så kanterne bevæger sig glat i stedet for i trin hver 32. sample.
2. **Fartgrænsen er mindst LFO'ens egen topfart** (+25 %), beregnet for formen (Sine, Triangle, Square, Random). Den gamle grænse gælder kun spring (Frequency, Pause, automation).
3. Tekster: Rate-tippet og "?"-boksen siger nu, at båndet altid når hele Sweep-området, og at hurtig Rate bøjer pitchen meget og giver korte toppe.
4. Tests: `EditorShot <mappe> --reach` (rækkevidde) og en "Niveau ved fuld fart"-tabel i HostSmoke. HostSmoke's niveaugrænse er hævet fra 1,25 til 2,0 (ustabilitet gav 4,8–27).

**Målt:**
- **Rækkevidde** (600 Hz, Sweep 100 % = 75 Hz – 4,80 kHz): Sine, Triangle og Square når ud ved 1 og 20 Hz i alle tre stejlheder (Triangle 20 Hz: 75,1 Hz – 4,79 kHz). Random rammer tilfældige mål som før.
- **Lyden ved langsom Rate:** næsten som 1.6 (Sine 0,5 Hz: største forskel 0,0095). Ikke længere bit-identisk.
- **Pitch** (300 Hz, Sweep 100 %, 24 / 48 / 96): 1 Hz 64 / 201 / 558 cent (1.6: 64 / 201 / 295), 3 Hz 195 / 617 / 1389, 12 Hz 789 / 2335 / 2174.
- **Niveau ved fuld fart** (peak / RMS i forhold til indgangen): op til +8,8 dB peak (96 dB, Random, 20 Hz), RMS højst +2,1 dB. 1.6: højst +3,1 dB peak.
- Røgtest og DAW-test OK. CPU næsten som 1.6 (`--bench` skiftevis: 1.6 3,0 / 2,5 %, 1.7 3,1 / 2,8 %).

**NÆSTE GANG:** test 1.7 i en DAW: høj Sweep + høj Rate (når båndet ud?), og er de korte niveautoppe o.k.? Ellers kan der laves en udjævning af toppene. Se også hjemmesiden og beslut om Mac-buildet.

---

## 2026-11-10 (4) — Version 1.6 installeret

- Brugeren skrev: "godt. Installer 1.6".
- **Installeret og verificeret med filhash** (`C4BDDB28…`, moduleinfo 1.6.0). Buildet (12:43:14) er nyere end sidste kildeændring (12:41:10). Doppler Saturator 1.0–1.2 og Pitch Bend Texture 1.3–1.5 ligger uændret ved siden af.
- Indhold: se mockup-posten (2): Sweep-displayet og skraveringen viser midtens vej, og en Pause-knap. Lyden er bit for bit som 1.5 med Pause slukket.
- KRAV ("Version 1.6") og CLAUDE.md er opdateret.
- **Ikke testet i en DAW.**

**NÆSTE GANG:** test **1.6** i Ableton / FL Studio (Sweep, Band Size, Pause, automation af Pause, 96 dB/oct), se hjemmesiden, og beslut om Mac-buildet (se 2026-11-09 (3)). Hjemmesidens billede er stadig fra 1.5 (uden Pause): skal det skiftes?

---

## 2026-11-10 (3) — Bekræftet: mockuppet er netop det forslag

- Brugeren spurgte, om forslaget "Sweep-displayet og den skraverede flade viser kun midtens vej (Frequency ± Sweep) [...] lyden er uændret" var det, der blev bygget ind i mockuppet. Svar: ja, præcis det (se posten nedenfor). Lyden er bit for bit som 1.5. Ingen kodeændringer.

---

## 2026-11-10 (2) — Mockup af 1.6: Sweep viser kun midtens vej, Pause-knap (bygget, IKKE installeret)

- Brugeren skrev: "lad mig se mock-up med dine anbefalinger for begge punkter".
- Lavet i koden (version 1.6 i CMakeLists, bygget men **ikke installeret**, venter på brugerens godkendelse):
  1. **Sweep-displayet og det skraverede område** viser midtens vej, Frequency ± Sweep (`bandCentreFor`), ikke længere fra laveste til højeste kant. Standard (600 Hz, Sweep 50 %): "212 Hz - 1.70 kHz" (før "150 Hz - 2.40 kHz"). Med Band Size 4 okt er det stadig "212 Hz - 1.70 kHz" (før ville det have været ca. "53 Hz - 6.79 kHz"). Teksten under displayet: "Shaded: where the LFO moves the band's middle".
  2. **Pause:** ny parameter `pause` (navn "Pause", tilføjet sidst, 13 parametre). Knap med gul LED i LFO-modulets titellinje (linjen stopper før den). LFO'ens fase står stille, og i Sync følger den ikke værten, mens Pause er tændt (bagefter glider kanterne tilbage til takten med fartgrænsen).
- **Tjek:** `HostSmoke --compare` mod 1.5: "SAMME LYD" (Pause slukket). Røgtest + DAW-test OK med 13 parametre (alle automatiseret på én gang, gem / hent 0 af 13 forkerte). EditorShot: ny `doppler6_bandsize4.png` (Sweep uændret) og `doppler7_pause.png`; Pause målt: fase 0,8940 → 0,8940 og båndets bund 223,1 → 223,1 Hz efter 1 s.
- Billederne er vist brugeren (før / efter / Band Size 4 / Pause).
- Venter på godkendelse. Derefter: KRAV, installér 1.6 og verificér.

---

## 2026-11-10 — Spørgsmål: Band Size og Sweep, pause-knap (intet bygget endnu)

- Brugeren skrev: "for at gøre effekten lidt mere i overenstemmelse med knoben, så skal vi have: 'Band size' ændrer lige nu også på sweep (traven range). Vi skal kun have, at band size ændrer bredden af bandet, ikke sweep (travel range). Før du bygger, fortæl mig, om dette giver mening ift. det jeg ønsker, eller om jeg blot reagerer på noget misledende grafisk? 2) Vi skal have en pause-knap, hvor effekten stopper, bandet er er nået til. Dette skal hjælpe til, at brugeren kan aflæse det spektrum, som band size-diplayet viser."
- Fortalt status fra "Gem og luk" 2026-11-09 (4) og mindet om at teste 1.5 og se hjemmesiden.
- **Svar 1 (målt i koden):** det er mest grafikken.
  - I lyden flytter LFO'en båndets midte Frequency ± Sweep (`bandEdgesFor`: midten = log2(freq) + v · Sweep · 3 okt). Band Size ændrer ikke, hvor langt midten eller kanterne flytter sig, og dermed heller ikke pitchen.
  - Men "Sweep"-displayet og det skraverede område tegnes fra laveste kant (v = −1) til højeste kant (v = +1), dvs. midtens vej **plus** Band Size. Derfor vokser de, når Band Size skrues op.
  - Eneste undtagelse i lyden: når båndet er bredt og når 20 Hz / 20 kHz, bliver kanterne stoppet der, og så flytter de sig lidt mindre.
  - Forslag: "Sweep"-displayet og det skraverede område viser kun midtens vej (Frequency ± Sweep). Lyden uændret (bit-identisk). Ikke anbefalet: at ændre lyden, så Band Size gør vejen kortere (så ville et bredere bånd give mindre pitch).
- **Svar 2:** pause giver mening. Forslag: en ny parameter "Pause" (automatiserbar, ny ID sidst), en knap i LFO-modulet. LFO-prikken og båndet står stille, Band Size-displayet står stille, Frequency og Band Size kan stadig flytte båndet. Bemærk: et bånd, der står stille, bøjer ikke pitchen, så med Dry/Wet 100 % lyder det næsten som det rene signal (kun fasen er ændret); under 100 % står phaser-hullerne stille.
- Venter på brugerens svar. Ingen kodeændringer.

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
