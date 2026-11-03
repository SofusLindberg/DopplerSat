# Fremdrift

Logbog over arbejdet på Doppler Saturator. Nyeste øverst. Claude skriver en ny post **hver gang** noget er bygget, så en ny session altid kan fortsætte præcis herfra. (Multiband Saturator har sin egen logbog i `../MultibandSat/FREMDRIFT.md`.)

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
