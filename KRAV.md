# Krav til Doppler Saturator

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

## Funktioner der skal blive ved med at virke

- **Ét bånd** med 0 ms-motoren fra Multiband Saturator 3.17. Valget står mellem **0 ms - 24 dB/oct** og **0 ms - 48 dB/oct** (to knapper, standard 48). Der er ingen lineær fase.
- **Spektrum: kun output** (fyldt, og kun "Output" i forklaringen; 1.1, brugeren: "Vis kun output på grafen og som forklaringstekst"), med båndet, der bevæger sig. Det skraverede område viser, hvor LFO'en flytter båndet hen. (1.0 viste også input som streg.)
- **"?" ved de to 0 ms-knapper** (1.1): en forklaringsboks med samme layout som i Multiband Saturator. Den fortæller kort, hvordan 24 og 48 lyder (48 bøjer pitchen ca. 3 gange så meget ved langsom og middel Rate, ca. 1,3 gange ved hurtig Rate, målt), og på begynderniveau, hvad 24 / 48 dB/oct betyder. Under knapperne viser "Edge steepness" de to kanters fald.
- **Edge Q er bestemt på forhånd** og kan ikke ændres: fra (et tydeligt bånd med 0 ms-tilstandens skrå kanter). Claude valgte det i 1.0, fordi Edge Q 50 % bredte saturationen ud over det meste af spektret. Doppler-effekten er den samme. Tegningen af båndet har samme form og højde som i 1.0 ved 100 % drive, men den viser kun, hvor båndet er.
- **Kanterne flytter sig højst 40 (24 dB) / 20 (48 dB) oktaver i sekundet.** Hurtigere gav niveauhop (op til 8,8 i peak for en tone på 0,6). Ved hurtig Rate og stor Amount bliver bevægelsen derfor mindre.
- **LFO:**
  - **Rate** i Hz, fra 0,01 Hz (én gang hvert 100. sekund) til 20 Hz.
  - **Sync** til tempoet: 8 / 4 / 2 / 1 takt, 1/2., 1/2, 1/2T, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16., 1/16, 1/16T og 1/32 (. = punkteret, T = triol). Når sangen spiller, følger fasen værtens position, så LFO'en ligger fast i takten.
  - **Former:** Sine, Triangle, Square (bløde overgange på ca. 8 ms, så den ikke klikker) og Random (glider blødt fra én tilfældig værdi til den næste hver periode).
  - **Offset** −100 til +100 %: kurven flyttes op eller ned og klippes ved top og bund, så båndet bliver længere tid i den ene ende (som i Abletons LFO).
- **Band:**
  - **Frequency**: midten af området.
  - **Amount**: hvor langt LFO'en flytter båndet. 100 % = 3 oktaver til hver side.
  - **Width**: båndets bredde, 0,1–6 oktaver.
- **Ingen saturation** (1.1): Drive er fast 0 %, og der er hverken Drive-knap eller Distortion-vælger (brugeren bad om at fjerne dem, 2026-11-04). Lyden er kun Doppler-effekten fra båndets kanter. (1.0 havde Drive 0–400 % og Distortion.)
- **Output:** Dry/Wet (mellem dry og wet blandes de som en phaser), Output gain og Bypass. Bypass er også DAW'ens egen bypass-knap (1.1), så der kun er én.
- **Displayet:** træk til venstre / højre = Frequency. Dobbeltklik nulstiller den. (Op / ned var Drive og er fjernet med den.)
- **Automation (1.1):** alle 12 parametre kan automatiseres. Højreklik på enhver knap, vælger eller displayet viser en menu: DAW'ens egne punkter øverst, hvis DAW'en har dem (VST3: fx Cubase, Bitwig, Reaper, Studio One), "Select ... for automation" (gør parameteren til "sidst rørt", fx til FL Studios Tools > Last tweaked > Create automation clip), "Reset ... to default" og en kort vejledning til Ableton og FL Studio. Et højreklik ændrer aldrig selve kontrollen.
- **Let at køre (1.1):** 2–3 gange mindre CPU end 1.0 med præcis samme lyd. Kun de filtre, man hører, regnes (`DopplerCore`). Latensen er 60 samples (1,4 ms ved 44,1 kHz) fra 4x-oversamplingen. DAW'en udligner den selv.
- **Ableton:** installeren skriver selv `Options.txt` (som i Multiband Saturator 3.18). Pluginnet har kun 12 parametre, så det er ikke nødvendigt her, men det skader ikke.
- **Versioner:** hver version hedder "Doppler Saturator 1.x" og har sin egen plugin-kode (D1xx). Den lægges i `C:\Program Files\Common Files\VST3` ved siden af de gamle, så alle versioner kan vælges i DAW'erne (brugeren gentog det 2026-11-04).

## Arbejdsregler (som i Multiband Saturator)
- Svar på dansk, og forklar enkelt. Kommentarer i koden skal være på dansk (ASCII: aa / oe / ae).
- Log alle prompts i `FREMDRIFT.md`. Commit og push selv.
- Installér altid den nyeste version i `C:\Program Files\Common Files\VST3` med eget versionsnummer, og verificér med filhash. Gamle versioner slettes aldrig.
