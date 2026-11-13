# Krav til Pitch Bend Texture (til og med 1.2: Doppler Saturator)

Alt hvad brugeren har bedt om til dette plugin, samlet ét sted. **Intet her må fjernes eller ændres i pluginnet, medmindre brugeren udtrykkeligt beder om det.** Claude læser denne fil, før pluginnet ændres, og tilføjer nye ønsker her i samme tur (med dato og brugerens ord).

Pluginnet er udskilt fra Multiband Saturator (`../MultibandSat`, se dens `KRAV.md` for kravene til 0 ms-motoren, som stadig gælder her: punkt 6 og 9 under "Edge Q og kanterne").

## Oprettelsen (2026-11-03)

Brugerens ord:
> 3) Her kommer en lidt større opgave: Følgende funktioner (0 ms - 24 db/oct og 0 ms - 48db/oct) skal udgå af dette plugin og i stedet oprettes som en seperat projektmakke og eget plugin med følgende beskrivelse:
>
> Dobbler-effekt (ptitch ændres, når man trækker båndet henover frekvenserne) i multiband saturation er faktisk super fedt. Det er noget, jeg gerne vil lave til et selvstændigt plugin for at kunne lave pitch-effekt/dobbler effekt. Opret et separat projekt for dobbler-effekt, hvor vi tager udgangspunkt i de dele af Multiband-saturator, som vi kan genbruge.
>
> Dobbler-effekt i Multiband-saturator.
> Tag spektogrammet med et enkelt bånd med phase-indstilligerne "0 ms - 24 db/oct" og "0 ms - 48 db/oct" (ikke medtag linear phase), så vi kan lave en saturator, hvis eneste formål er at give brugeren mulighed for at bruge dobbler-effekten (ptitch ændres, når man trækker båndet henover frekvenserne) kreativt.
>
> Funktions-mæssigt skal vi tænke ligeså simpelt som Abletons Phaser-flanger (se billede af Abletons Phaser-flanger "...\design\Sofus design billeder\3.17\Phaser-FlangerAbleton.png") : Edge Q er forudbestemt for brugeren og kan ikke ændres.
>
> LFO:
> Båndet skal konstant bevæge sig henover det valgte område med en minum meget lave rate. Fra meget langsomt, til meget hurtigt. Amount (jeg tænker mængden af drive? Eller hvad der påvirker pitchen mest) kan justeres, ligesom brugeren kan justere båndbredden af det tilgængelige bånd. Rate skal kunne indstilles frit i Hz og timet til bpm i forskellige rytmer (¼, ⅓, 4/4, alle de relevante og alment brugte)
> Båndet skal kunne bevæge sig fra side til side i forskellige kurver: Sinus, square, triangle, random. Med mulighed for offset (se reference af Abletons LFO-tool "...\design\Sofus design billeder\3.17\AbletonLFO.png"), så man blive længere tid på toppedal eller bund af en kurve.
>
> (undervejs) du må gerne genbruge designet af de to 0 ms til doppler-pluginnet for at spare tid og energi

Referencebillederne ligger i Multiband Saturator-repoet: `design/Sofus design billeder/3.17/Phaser-FlangerAbleton.png` og `AbletonLFO.png`.

Brugerens svar på Claudes spørgsmål (2026-11-03):
- Navn og sted: "Doppler Saturator, privat repo". Mappen `../DopplerSat` ved siden af MultibandSat, eget git og det private GitHub-repo `SofusLindberg/DopplerSat`. Versioner nummereres fra 1.0, og hver version får sit eget nummer og sin egen plugin-kode, som i Multiband Saturator.
- Amount: "Amount = hvor langt båndet flytter sig". Drive har sin egen knap, og Width er båndets bredde.

## Version 1.1 (2026-11-04)

Brugerens ord:
> Doppler saturator 1.0 — Rigtigt godt.
> 1. Drive. Brugeren skal ikke kunne ændre drive. Pluginnet skal fra start være indstillet til mindst mulige drive, hvor doppler-effekten er lydbar. I mine egne test virker doppler-effekten allerede 0,0% indstillet Selve drive-effekten skal være så lidt lydbar som overhovedet muligt, gerne helt væk, hvis doppler-effekten så stadig virker. Så fjern drive knobben. Fjern også muligheden for at vælge distortion-type, med mindre du mener, at det gør en forskel for den lydbare effekt af doppler, så det opleves som havende forskellige lydlige kvaliteter mht. pitch-ændringen.
> 2) Spektrum: Vis kun output på grafen og som forklaringstekst.
> 3) Automations: Sørg for alle parametre kan automatiseres. Gerne så det virker i alle DAWS. F.eks. Ved at kunne højreklikke på en knob/knap inde i pluginnet, og så indstille til automation.
> 4) Lav en forklaringstekst, der kan aktiveres med et spørgsmåltegn som forklaringboks (med samme layout som pluginnet MultibandSaturator, som du også har lavet) ved de to 0 ms og forklar helt kort, hvad forskellen er. Ikke noget teknisk, men forklaring om den lydlige kvalitet: Laver den ene mere pitch end den anden? Og så ultra kort og på begynderniveau, hvad 24db vs 48 db rent teknisk betyder.
> 5) Hvis pluginnet skaber forsinkelse i DAW'en, så arbejd med at gøre pluginnet lettere at køre, uden at det påvirker den lydlige kvalitet af nogle af indstillingsmulighederne.
> 6) Der er blevet lagt alt muligt ind i pluginmappen, som ser lidt forkert ud: Ryd op. (skærmbillede af FL Studios plugin-liste med Qt6-filer)
>
> (undervejs) den nye version af pluginnet skal i navnet have versionnummer og lægges i VST3-mappen, hvor mine DAW's kan hente plugins, så jeg stadig har de ældre versioner af pluginnet tilgængeligt i mine DAW's

Det betyder (punkterne nedenfor er rettet efter det):
- **Drive og Distortion er fjernet** (brugerens udtrykkelige ønske). Drive er fast 0 %: der er ingen saturation overhovedet, kun Doppler-effekten. Distortion gjorde intet ved 0 % (formeren køres slet ikke), så den kunne ikke give pitch-ændringen en anden lyd.
- **Lyden må ikke ændre sig** ved at gøre pluginnet lettere: 1.1 er bit for bit som 1.0 med Drive 0 % (`HostSmoke --compare`). Derfor er 4x-oversamplingen og dens latens (60 samples) beholdt (uden den kan kanterne over 20 kHz ikke flytte sig, og det ville ændre lyden i diskanten, når båndet ligger højt).

## Oprydning i FL Studio, del 2 (2026-11-05)

Claude foreslog at flytte Wobble 2 til `C:\Program Files\Common Files\VST2` og fjerne søgestien `C:\Program Files\Steinberg\` i FL Studio, så Steinbergs DLL'er ikke kommer tilbage i plugin-listen. Brugerens ord:
> Ja, flyt Wobble to til VST2

Det betyder: kun Wobble 2's plugin-fil flyttes. Dens presets og samples bliver i `C:\Program Files\Steinberg\SONiVOX\Wobble 2`, fordi Wobble 2 har den sti gemt (`C:\ProgramData\SONiVOX\Wobble 2\Wobble.txt` og preset-databasen `Wobble.svxdb`). Wobble 2 skal stadig virke i FL Studio.

## Version 1.2 (2026-11-05)

Brugerens ord:
> Det fungerer godt. 1) GUI: Ryk bypass op, så den flugter med den første 0 - ms indstilling (se reference, og gerne flugte lige med midten af den første 0 - ms) "C:\Users\sofus\Documents\Programmering\DopplerSat\design\1.1\LayoutV1.1.png"), 2) Kan du lave yderligere en 0 - ms, som er endnu stejlere og pitcher lyden endnu mere? Hvad er maksimalt du kan gøre det?

Det betyder (punkterne nedenfor er rettet efter det):
- **Bypass** står i højde med den første 0 ms-knap (24 dB/oct), og dens midte flugter med knappens midte. Referencen er `design/1.1/LayoutV1.1.png` (den grønne streg).
- **En tredje 0 ms-knap: 0 ms - 96 dB/oct** (dobbelt så stejl som 48). 24 og 48 skal lyde præcis som før (bit for bit som 1.1).
- **"Hvad er maksimalt":** motoren kan bygge kanter op til 192 dB/oct. Claude valgte 96 og forklarede brugeren, hvad 192 koster (målt, se `FREMDRIFT.md` 1.2). Brugeren bestemmer, om 192 skal med.

## Version 1.3 (2026-11-08)

Brugerens ord:
> Super. 1) Billede 1: Skriften i Syncknappen bliver delvist skjult. Teksten (ikke boksen) skal rykkes lidt til venstre, så hele teksten er synlig. 2) Navnet på pluginnet skal hedde Pitch shift Texture i stedet for Doppler Saturator - giver dette nye navn mening i forhold til pluginnets funktionalitet? 3) Billede 2: Hvorfor står det 0 ms foran valget af edges? Hvad betyder det? Hvis teksten ikke har nogen repræsentativ betydning, skal '0 ms' fjernes. Hvis det handler om den forsinkelse, der skabes ved valget af edge, så beskriv det eventuelt super kort inde i infoboksen

Claude svarede, at "Pitch shift" normalt betyder en transposer, og foreslog "Pitch Bend Texture". Brugeren valgte **"Pitch Bend Texture"**.

Det betyder (punkterne nedenfor er rettet efter det):
- **Navnet er "Pitch Bend Texture"** fra 1.3. 1.0–1.2 hedder stadig "Doppler Saturator" og bliver liggende. Mappen, repoet og CMake-målet hedder stadig DopplerSat.
- **Hz / Sync:** teksten (og LED'en) står længere til venstre, så "Sync" kan ses helt. Knapperne har samme størrelse og plads.
- **"0 ms" er fjernet** fra de tre knapper (og fra parameterens valg): alle tre er 0 ms, så det skilte dem ikke ad. Infoboksen siger kort, at ingen af dem giver forsinkelse, og at pluginnet som helhed forsinker 1,4 ms, som DAW'en udligner.

## Version 1.4 (2026-11-09)

Brugerens ord:
> 1) Width skal hedde noget andet. Det skal fremgå tydeligere, at det er selve kurven, der forlænges eller forkortes. Overvej samtidig Amount: Kan det gøres tydeligere i beskrivelsen, at det kun vedrører det område, som kurven bevæger sig henover? Hvis du kan finde et mere præcist ord for kurve, så brug det. 2) Sørg for, at pluginnet virker til Alle DAW's. Automations skal virke i alle DAWS. Er det muligt at lave det?

Det betyder (punkterne nedenfor er rettet efter det):
- **Width hedder "Band Size"** med underteksten "the band itself" ("bånd" er det præcise ord for kurven). **Amount hedder "Sweep"** med underteksten "travel range", og den passer til displayet "Sweep range" ovenover. Tooltips siger, at Sweep kun ændrer området, båndet bevæger sig hen over, og at Band Size kun ændrer selve båndet. Parameterne hedder det samme i DAW'erne. ID'erne er de samme.
- **Alle DAW'er:** pluginnet virker i alle DAW'er på Windows, der kan VST3 (Ableton, FL Studio, Cubase, Studio One, Bitwig, Reaper m.fl.). Det testes af `HostSmoke`'s DAW-test (se Funktioner). Pro Tools kræver AAX-formatet (Avid-udviklerkonto og PACE-signering, som brugeren selv skal oprette), og Logic findes kun på Mac. Det er ikke lavet.

## Hjemmeside (2026-11-09)

Brugerens ord:
> Du skal også lave en hjemmeside til produktet. Hjemmesiden skal repræsentere den overordnede virksomhed, og pluginnet er det første produkt fra virksomhed. Kald virksomheden for SoundSo indtil videre. Lav et html mockup af hjemmesiden. Mind mig næste gang, at jeg skal teste pluginnet og se hjemmesiden

Det betyder:
- Virksomheden hedder **SoundSo** (foreløbigt navn). Siden er virksomhedens, og Pitch Bend Texture er dens første produkt.
- Et HTML-mockup i `website/index.html` (en fil, kan åbnes direkte i browseren).
- Ved næste opstart: mind brugeren om at teste pluginnet og se hjemmesiden.

## Version 1.5, webshop og Mac (2026-11-09)

Brugerens ord:
> 1) kan vi integrere webshop på hjemmesiden, så man kan købe produktet for 10 USD? 2) Se billede 1: Billedet af pluginnet på hjemmesiden er strukket lidt. Sørg for at billedformaet direkte afspejler den rigtige størrelse af pluginnet (specielt kan jeg se, at ikonerne er strukket), 3) Kan du sørge for, at pluginnet og alle automations virker fuldt i i Logic til Mac også?
>
> (undervejs) 4) Billede 1: Sørg for at de to displays, der viser range af frekvenser, har samme navn som de tilhørende knobs (Sweep og Band Size)

Det betyder:
- **Webshop:** pluginnet koster **10 USD** (engangskøb). Hjemmesiden har en købsboks og købsknapper. Checkout kommer fra en betalingsudbyder (forslag: en "merchant of record" som Lemon Squeezy eller Paddle, der selv klarer moms). Brugeren skal selv oprette kontoen. Linket sættes i `CHECKOUT_URL` i `website/index.html`, og indtil da forklarer en boks, hvad der sker.
- **Billedet af pluginnet** på hjemmesiden vises i pluginnets rigtige størrelse, 900 × 600, og aldrig strakt. På smalle skærme bliver det mindre i samme forhold (3:2).
- **Displayene i Band-modulet hedder "Sweep" og "Band Size"** som knapperne (til og med 1.4: "Sweep range" og "Band now").
- **Logic på Mac:** pluginnet skal også findes som Audio Unit, og alle parametre skal kunne automatiseres i Logic. Koden er gjort klar (1.5), og Mac-buildet laves i GitHub Actions (`.github/workflows/mac.yml`), som også kører auval (Logics egen kontrol) og DAW-testen på AU'en. Det er **ikke kørt endnu**. Udgivelse til andre Mac-brugere kræver et Apple Developer ID (betalt Apple-konto) til signering og notarisering. Brugeren skal selv oprette det.

## Version 1.6 (2026-11-10)

Brugerens ord:
> for at gøre effekten lidt mere i overenstemmelse med knoben, så skal vi have: 'Band size' ændrer lige nu også på sweep (traven range). Vi skal kun have, at band size ændrer bredden af bandet, ikke sweep (travel range). Før du bygger, fortæl mig, om dette giver mening ift. det jeg ønsker, eller om jeg blot reagerer på noget misledende grafisk? 2) Vi skal have en pause-knap, hvor effekten stopper, bandet er er nået til. Dette skal hjælpe til, at brugeren kan aflæse det spektrum, som band size-diplayet viser.

Claude svarede, at Band Size ikke ændrer båndets vej i lyden, kun i tegningen, og foreslog at rette tegningen. Brugeren: "lad mig se mock-up med dine anbefalinger for begge punkter", og derefter "godt. Installer 1.6".

Det betyder:
- **Sweep-displayet og det skraverede område** viser kun, hvor båndets midte bevæger sig hen (Frequency ± Sweep). Band Size ændrer dem ikke. Lyden er uændret (bit for bit som 1.5).
- **Pause** (ny parameter, kan automatiseres): knap med gul LED i LFO-modulets titellinje. LFO'en og båndet står stille, så spektret og Band Size-displayet kan aflæses. Frequency og Band Size kan stadig flytte båndet. I Sync glider båndet tilbage til takten, når pausen slippes.

## Version 1.7 (2026-11-10)

Brugerens ord:
> Hvis man skruer op for sweep (travel range) og skruer op for rate (speed), så når bandet ikke at bevæge sig ud i yderkanterne af travel range. Vi skal have: Bandet skal kunne nå helt ud i yderppunkterne af travel range, selv når rate (speed) er skruet helt op. Uanset rate (speed), skal båndet bevæge sig ud helt ud i kanterne af den valgte range.
>
> (undervejs) Ligesom du ser, at der sker her for Abletons LFO tool (se billede): Rate skrues op, men top og bund rammer stadig helt ud

Det betyder:
- Båndets midte når ud til Frequency ± Sweep ved alle Rate-værdier (0,01–20 Hz og alle Sync-værdier) for Sine, Triangle og Square. Random rammer som før tilfældige mål inden for området.
- Lyden ved langsom Rate er (næsten) som før; ved hurtig Rate bøjer pitchen meget mere, og der kommer korte toppe i niveauet (målt op til ca. +9 dB i peak, gennemsnittet højst ca. +2 dB). Det er en del af effekten.

## Version 1.8 (2026-11-10)

Claude forklarede, hvad en udjævning af niveautoppene gør (løsning 1: et loft, så kun de korte toppe dæmpes). Brugerens ord:
> 1) Ja, prøv at gøre løsning A til en knap, så jeg kan teste mellem nuværende effekt og den nye. 2) Lav et mockup, hvor vi fjerner illustrationen af edges. Brugeren ser jo alligevel forskellen på spektrogrammet, så den er lidt overflødig.

Det betyder:
- **Smooth Peaks** (ny parameter, kan automatiseres): en knap under Bypass i Output-modulet. Fra = effekten som i 1.7 (bit for bit). Til = det behandlede signal må højst nå 3 dB over toppene af en rolig version af effekten (båndet med den gamle fartgrænse, som 1.6). Langsom Rate røres ikke.
- **Edge steepness-tegningen** i Edges-modulet: et mockup uden den er vist brugeren. Den er stadig med i pluginnet, indtil brugeren beslutter sig.

## Version 1.9 (2026-11-10)

Brugerens ord (efter 1.8):
> Svar før, at du begynder at kode: 1) jeg kan simpelthen ikke høre forskel på med og uden smooth peaks. Tænker du, at giver mening at gøre smooth peaks til default og erstatte nuværende, så smooth peaks-knappen også forsvinder. Den er som default bare sat til? 2) Lav samtidig endnu et mockup, hvor den grafiske illustration af edges er væk, men GUI tilgængeligt er designet med henblik på at gøre spacing (efter illusatrionen er fjernet) mere balanceret elementerne imellem.

Claude anbefalede det og viste mockuppet (`doppler9_mockup_balanced.png`). Brugerens ord:
> Ja, byg 1.9 med smooth peaks som default og det nye layout (se billede). Forsøg gerne at gøre cpu mindre, hvis det på ingen måder påvirker lyden eller på anden vis kan ses i pluginnet.

Det betyder:
- **Smooth Peaks er altid til**, og knappen og parameteren er væk. Lyden er bit for bit som 1.8 med Smooth Peaks tændt.
- **Det nye layout:** tre moduler (LFO, Band, Output). De tre Edges-knapper ligger som en søjle i Band-modulet til højre for de to displays, med "Edges" og "?" øverst. **Edge steepness-tegningen er fjernet.**
- **Mindre CPU, uden at lyden eller pluginnet ændrer sig:** målt bit for bit mod 1.8 med Smooth Peaks tændt.

## Version 1.10 (2026-11-13)

Brugerens ord (med billede af Sweep-displayet):
> SWEEP display: Brugeren skal kunne skrive grænserne af top og bund ind ved at klikke på displayet (Før du bygger, så giv mig et bud på, hvad vi gør, hvis brugeren kun vil rette den ene ekstrem ind (top eller bund), fordi både top og bund justeres lige nu ved at dreje på Sweep-knoben?)

Claude gav tre bud (Sweep-displayet, Band Size-displayet og Sweep 100 %). Brugerens ord:
> vi tager lige én ting ad gangen. Prøv denne: "1) Skriv Sweeps bund og top ind i displayet. Klik på det tal, du vil rette. Det andet tal bliver stående. [...] Retter du toppen, regner pluginnet en ny Frequency og Sweep ud, så bunden står helt stille, og begge knapper drejer med. Der kommer ingen nye parametre, og automation virker som før. Bagefter virker knapperne som nu."

Det betyder:
- **Klik på et af Sweep-displayets tal** (bund eller top), og et felt åbner over netop det tal. Man kan skrive fx `180`, `1.2k` eller `1,2 kHz`. Enter bruger tallet, Esc fortryder, og Tab bruger det og går til det andet tal.
- **Det andet tal bliver stående.** Frequency og Sweep regnes ud og drejer med, så det er én ændring i DAW'en. Der er ingen nye parametre, og lyden ændres ikke.
- Er området bredere, end Sweep 100 % kan nå, gælder det skrevne tal, og det andet flyttes kun så meget, som det er nødvendigt. En bund over toppen (eller noget, der ikke er et tal) afvises.

## Version 1.11 / 1.12 (2026-11-13)

Brugerens ord (efter 1.10):
> Redigering af Sweep fungerer efter hensigten. Jeg kan se, at afsstanden mellem de to kanter når et maksimum, og den ene side trækkes med af den anden, hvis afstanden bliver for stor. Vi skal derfor have bygget, at 100% travel range går helt ud til kanterne (dybeste og lyseste frekvens).

Det betyder (Claudes bud fra 2026-11-13, se nedenfor):
- **Sweep 100 % = båndets midte når helt ud til 20 Hz og 20 kHz**, uanset Frequency. Sweep er en andel af vejen fra Frequency ud mod hver ende: 0 % = båndet står på Frequency, 50 % = halvvejs i oktaver. Frequency er stadig midten. Til og med 1.10: 100 % = 3 oktaver til hver side.
- Står Frequency langt fra midten af spektret (ca. 630 Hz), er den ene side længere, og båndet bevæger sig hurtigere og bøjer pitchen mere på den side.
- Standard-Sweep er 30 % (før 50 %), så standardlyden er næsten som før (ved 600 Hz: 216 Hz – 1,72 kHz mod 212 Hz – 1,70 kHz).
- Alle områder mellem 20 Hz og 20 kHz kan nu skrives ind i Sweep-displayet, også med Offset. Den anden ende trækkes ikke længere med.
- Frequency og Sweep har ingen trin længere (før 0,1 Hz og 0,1 %), så den urørte ende står helt stille, når man skriver.
- 1.11 viste Frequency og Sweep med 7 decimaler ("600.0001221 Hz"). 1.12 viser én decimal som før; ellers er 1.12 som 1.11.

## Efter 1.12 (2026-11-13, ikke bygget endnu)

Brugerens ord:
> 1) Pilen, der indikerer Frequency Middle skal være i midten af de to yderkanter i alle tilfælde, hvor det er muligt. 2) Jeg synes ikke, at hastigheden på bevægelsen fra den ene kant til den anden er jævn (test i sine). Er det kunstigt skabt, at det virker som om, at hastigheden på bevægelsen er langsommere, når vi nærmer os yderkanten? Svar, før du bygger.

Claudes bud (venter på brugerens ja):
- Sweep bestemmer områdets samlede bredde (100 % = hele spektret), og området ligger symmetrisk om Frequency. Rammer det en ende af spektret, skubbes hele området ind, så bredden holdes. Så står pilen i midten, når det kan lade sig gøre, og båndet har samme fart begge veje. 100 % er stadig 20 Hz – 20 kHz, og standard 30 % giver som før ca. 213 Hz – 1,69 kHz ved 600 Hz.
- Muligt (brugerens valg): at båndets yderste kant i stedet for midten når præcis ud til 20 Hz / 20 kHz ved Sweep 100 %, så båndet ikke klippes og ikke ser ud til at gå i stå ved enderne.

Brugeren vil tage én ting ad gangen. Band Size-displayet venter.

Brugerens ord:
> Band size skal også kunne justeres ved, at brugeren kan skrive i displayet.

Claudes bud (venter på brugerens ja):
- Klik på et af Band Size-displayets tal og skriv en ny kant. Kun Band Size ændres. Båndets midte bliver, hvor den er, så den anden kant flytter sig lige så meget den modsatte vej.
- Under Pause rammer båndet præcis det skrevne. Når LFO'en kører, står tallet stille, mens man skriver.

Undervejs, brugerens ord (bygget i 1.11 / 1.12, se ovenfor):
> Sweep (travel range): 100% skal gå helt ud til hver ende af spektrummet.

Claudes bud (bygget):
- Sweep bliver "hvor langt mod hver ende": 0 % = båndet står på Frequency, 100 % = båndets midte når helt ud til 20 Hz og 20 kHz, uanset Frequency. Til og med 1.10: 100 % = 3 oktaver til hver side.
- Standard-Sweep bliver 30 %, så standardlyden er næsten som nu.

## Funktioner der skal blive ved med at virke

- **Ét bånd** med 0 ms-motoren fra Multiband Saturator 3.17. Valget står mellem **24 dB/oct**, **48 dB/oct** og (1.2) **96 dB/oct** (tre knapper under hinanden, standard 48; til og med 1.2 stod der "0 ms - " foran; fra 1.9 i Band-modulet). Der er ingen lineær fase.
- **Spektrum: kun output** (fyldt, og kun "Output" i forklaringen; 1.1, brugeren: "Vis kun output på grafen og som forklaringstekst"), med båndet, der bevæger sig. Det skraverede område viser, hvor LFO'en flytter båndet hen. (1.0 viste også input som streg.)
- **"?" ved Edges-knapperne** (1.1): en forklaringsboks med samme layout som i Multiband Saturator. Den fortæller kort, hvordan 24, 48 og 96 lyder (48 bøjer pitchen ca. 3 gange så meget som 24 ved langsom og middel Rate; 96 næsten 3 gange så meget som 48 ved langsom Rate; ved hurtig Rate 1,1–1,5 gange for hvert trin, målt), og på begynderniveau, hvad dB/oct betyder. Den siger også (1.3), at ingen af de tre giver forsinkelse (0 ms), og at pluginnet som helhed forsinker 1,4 ms, som DAW'en udligner. Til og med 1.8 viste "Edge steepness" under knapperne kanternes fald. Den er fjernet i 1.9 (brugerens ønske).
- **Edge Q er bestemt på forhånd** og kan ikke ændres: fra (et tydeligt bånd med 0 ms-tilstandens skrå kanter). Claude valgte det i 1.0, fordi Edge Q 50 % bredte saturationen ud over det meste af spektret. Doppler-effekten er den samme. Tegningen af båndet har samme form og højde som i 1.0 ved 100 % drive, men den viser kun, hvor båndet er.
- **Båndet når altid helt ud i Sweep-områdets kanter, uanset Rate** (1.7, brugerens ønske 2026-11-10, se "Version 1.7"). Fartgrænsen er mindst LFO'ens egen topfart. Den gamle grænse, 40 (24 dB) / 20 (48 dB) / 10 (96 dB) oktaver i sekundet, gælder kun spring (Frequency, Pause, automation). Til og med 1.6 var den grænsen for alt, så bevægelsen blev mindre ved hurtig Rate. Båndets fire filtre er TPT-SVF, så hurtig bevægelse ikke giver niveauspring.
- **LFO:**
  - **Rate** i Hz, fra 0,01 Hz (én gang hvert 100. sekund) til 20 Hz.
  - **Sync** til tempoet: 8 / 4 / 2 / 1 takt, 1/2., 1/2, 1/2T, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16., 1/16, 1/16T og 1/32 (. = punkteret, T = triol). Når sangen spiller, følger fasen værtens position, så LFO'en ligger fast i takten.
  - **Former:** Sine, Triangle, Square (bløde overgange på ca. 8 ms, så den ikke klikker) og Random (glider blødt fra én tilfældig værdi til den næste hver periode).
  - **Offset** −100 til +100 %: kurven flyttes op eller ned og klippes ved top og bund, så båndet bliver længere tid i den ene ende (som i Abletons LFO).
- **Band:**
  - **Frequency**: midten af området.
  - **Sweep** (til og med 1.3: Amount): hvor langt LFO'en flytter båndet. **1.11: 100 % = båndets midte når helt ud til 20 Hz og 20 kHz** (brugerens ønske 2026-11-13; Sweep er en andel af vejen fra Frequency til hver ende, standard 30 %). Til og med 1.10: 100 % = 3 oktaver til hver side. Ændrer kun området, båndet bevæger sig hen over.
  - **Band Size** (til og med 1.3: Width): selve båndets bredde, 0,1–6 oktaver.
  - **Displayene** over knapperne hedder **Sweep** (området, båndets midte bevæger sig hen over; 1.6: uden Band Size) og **Band Size** (båndets kanter lige nu), som knapperne (1.5).
  - **Sweep-displayets tal kan skrives ind** (1.10): klik på bunden eller toppen og skriv. Det andet tal bliver stående, og Frequency og Sweep følger med.
- **Pause** (1.6): LFO'en og båndet står stille. Knap i LFO-modulets titellinje, kan automatiseres.
- **Smooth Peaks** (1.8 som knap under Bypass; **1.9 altid til**, uden knap og parameter, brugerens ønske 2026-11-10). Dæmper kun de korte niveautoppe ved hurtig Rate: det behandlede signal må højst nå 3 dB over toppene af en rolig version af effekten. Langsom Rate røres ikke.
- **Layout (1.9):** tre moduler, LFO, Band og Output. Edges-knapperne ligger som en søjle i Band, og Edge steepness-tegningen er væk (brugerens ønske 2026-11-10, efter mockuppet).
- **CPU (1.9):** Smooth Peaks' rolige reference-motor regnes kun, når den kan give en forskel, og deler de faste filtre med hovedmotoren. Det må hverken ændre lyden eller kunne ses i pluginnet (brugeren 2026-11-10).
- **Ingen saturation** (1.1): Drive er fast 0 %, og der er hverken Drive-knap eller Distortion-vælger (brugeren bad om at fjerne dem, 2026-11-04). Lyden er kun Doppler-effekten fra båndets kanter. (1.0 havde Drive 0–400 % og Distortion.)
- **Output:** Dry/Wet (mellem dry og wet blandes de som en phaser), Output gain og Bypass. Bypass er også DAW'ens egen bypass-knap (1.1), så der kun er én. Bypass flugter med midten af den første Edges-knap, 24 dB/oct (1.2).
- **Displayet:** træk til venstre / højre = Frequency. Dobbeltklik nulstiller den. (Op / ned var Drive og er fjernet med den.)
- **Automation (1.1):** alle 12 parametre kan automatiseres. Højreklik på enhver knap, vælger eller displayet viser en menu: DAW'ens egne punkter øverst, hvis DAW'en har dem (VST3: fx Cubase, Bitwig, Reaper, Studio One), "Select ... for automation" (gør parameteren til "sidst rørt", fx til FL Studios Tools > Last tweaked > Create automation clip), "Reset ... to default" og en kort vejledning til Ableton og FL Studio. Et højreklik ændrer aldrig selve kontrollen.
- **Let at køre (1.1):** 2–3 gange mindre CPU end 1.0 med præcis samme lyd. Kun de filtre, man hører, regnes (`DopplerCore`). Latensen er 60 samples (1,4 ms ved 44,1 kHz) fra 4x-oversamplingen. DAW'en udligner den selv.
- **Ableton:** installeren skriver selv `Options.txt` (som i Multiband Saturator 3.18). Pluginnet har kun 12 parametre, så det er ikke nødvendigt her, men det skader ikke.
- **Hz / Sync** (1.3): begge tekster kan ses helt i knapperne.
- **Alle DAW'er (1.4):** stereo-, mono- og mono-til-stereo-spor; samplerater 44,1–192 kHz; skiftende blokstørrelser (0–4096); alle 12 parametre kan automatiseres samtidig midt i lyden; gem / hent projekt giver samme værdier; pluginnet melder en hale på 2 s (længst målt 1,32 s), så DAW'er ikke klipper efterklangen af. `HostSmoke` tjekker det hele hver gang.
- **Mac (1.5):** samme kode bygger som Audio Unit og VST3 på Mac (Apple Silicon og Intel), og lyden er bit for bit den samme som på Windows (målt med SSE2 slået fra på Windows). Mac-buildet tjekkes med auval og `HostSmoke` i GitHub Actions.
- **Versioner:** hver version hedder "Pitch Bend Texture 1.x" (1.3 og frem; 1.0–1.2: "Doppler Saturator 1.x") og har sin egen plugin-kode (D1xx). Den lægges i `C:\Program Files\Common Files\VST3` ved siden af de gamle, så alle versioner kan vælges i DAW'erne (brugeren gentog det 2026-11-04).

## Arbejdsregler (som i Multiband Saturator)
- Svar på dansk, og forklar enkelt. Kommentarer i koden skal være på dansk (ASCII: aa / oe / ae).
- Log alle prompts i `FREMDRIFT.md`. Commit og push selv.
- Installér altid den nyeste version i `C:\Program Files\Common Files\VST3` med eget versionsnummer, og verificér med filhash. Gamle versioner slettes aldrig.
