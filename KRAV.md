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

## Funktioner der skal blive ved med at virke

- **Ét bånd** med 0 ms-motoren fra Multiband Saturator 3.17. Valget står mellem **0 ms - 24 dB/oct** og **0 ms - 48 dB/oct** (to knapper, standard 48). Der er ingen lineær fase.
- **Spektrum** (output fyldt, input som streg) med båndet, der bevæger sig. Det skraverede område viser, hvor LFO'en flytter båndet hen.
- **Edge Q er bestemt på forhånd** og kan ikke ændres: fra (et tydeligt bånd med 0 ms-tilstandens skrå kanter). Claude valgte det i 1.0, fordi Edge Q 50 % bredte saturationen ud over det meste af spektret. Doppler-effekten er den samme.
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
- **Saturation:** Drive 0–400 % og Distortion (Soft, Hard Clip, Tube, Fold, Warm Analog). Doppler-effekten er der også ved 0 % drive.
- **Output:** Dry/Wet (mellem dry og wet blandes de som en phaser), Output gain og Bypass.
- **Displayet:** træk til venstre / højre = Frequency, op / ned = Drive. Dobbeltklik nulstiller begge.
- **Ableton:** installeren skriver selv `Options.txt` (som i Multiband Saturator 3.18). Pluginnet har kun 14 parametre, så det er ikke nødvendigt her, men det skader ikke.

## Arbejdsregler (som i Multiband Saturator)
- Svar på dansk, og forklar enkelt. Kommentarer i koden skal være på dansk (ASCII: aa / oe / ae).
- Log alle prompts i `FREMDRIFT.md`. Commit og push selv.
- Installér altid den nyeste version i `C:\Program Files\Common Files\VST3` med eget versionsnummer, og verificér med filhash. Gamle versioner slettes aldrig.
