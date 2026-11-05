#pragma once
#include "SatDsp.h"

// =============================================================================
//  DopplerCore (1.1): kun den del af 0 ms-motoren, man kan hoere ved 0 % drive
// =============================================================================
// Brugeren (2026-11-04): "Brugeren skal ikke kunne ændre drive. [...] Selve drive-effekten skal være så lidt
// lydbar som overhovedet muligt, gerne helt væk, hvis doppler-effekten så stadig virker" og "gør pluginnet
// lettere at køre, uden at det påvirker den lydlige kvalitet".
//
// Ved 0 % drive saturerer satdsp::Engine intet (BandSat er slukket), saa lyden er kun den toerre sum
//   T = tch (gch[0] (h23 (x)))
// = allpassene fra alle crossovers. Kun baand 1's fire crossovers flytter sig (det er Doppler-effekten); baand 0
// (10-12 Hz) og baand 2-3 (40-45 % af den oversamplede rate) staar stille. Engine regnede desuden hele baand 1's
// opdeling (8 kaeder + fades), tch, gch[1] og designede 16 crossovers + punkter + fades hver 32. sample - alt
// sammen smidt vaek ved 0 % drive.
//
// Denne klasse goer PRAECIS det samme som Engine ved 0 % drive (samme filtre, samme raekkefoelge, samme
// opvarmning og samme toning ind / ud ved Bypass og ved skift mellem 24, 48 og 96), saa lyden er bit for bit den
// samme som i 1.0 med Drive paa 0 % (tjekkes af HostSmoke --compare). Kun baand 1's fire crossovers designes
// pr. stykke; de faste designes, naar motoren startes.
namespace satdsp
{
// Kun allpass-delen af designXover (n6, fc, fs).ap - praecis samme regning, saa koefficienterne er de samme bit for bit.
// (designXover designer ogsaa LP og HP, som DopplerCore aldrig bruger: to tredjedele af arbejdet i setParams.)
inline SeqCoefs designXoverAp (int n6, double fc, double fs)
{
    n6 = (int) clampd (n6, 1, 32);
    SeqCoefs ap;
    const bool even = (n6 % 2) == 0;
    const int  N    = even ? n6 / 2 : n6;
    const SectionInfo si = sectionInfo (N);
    if (even)
    {
        if (si.onePole) ap.add (designAllpass1 (fc, fs));
        for (int k = 0; k < si.nBiquad; ++k) ap.add (designAllpass2 (fc, si.q[k], fs));
    }
    else
    {
        for (int k = 0; k < si.nBiquad; ++k)
            if (((k + 1) % 2) == 1) ap.add (designAllpass2 (fc, si.q[k], fs));
    }
    return ap;
}

class DopplerCore
{
public:
    void prepare (double fsProcess)
    {
        fs = fsProcess;
        warmSamples = (int) (0.060 * fs);
        treeMix.setTime (fs, 15.0); treeMix.snap (0.0);
        treeOn = treeWanted = false;
        warmLeft = 0;
        haveStatic = haveBand = false;
        resetTree();
    }

    // p: som Engine faar dem (kun baand 1 er taendt, Edge Q fra). Kaldes hvert stykke (32 samples ved vaertens rate).
    void setParams (const EngineParams& p)
    {
        const bool newStatic = ! haveStatic || p.phaseMode != staticMode;
        const bool newBand   = ! haveBand || p.band[1].lo != lastLo || p.band[1].hi != lastHi || p.phaseMode != bandMode;
        if (! newStatic && ! newBand)
        {
            setTree (p);
            return;
        }
        const ZlSpec sp = zlSpec (p.phaseMode);
        const Layout L  = makeLayout (p, fs);      // Edge Q fra og baandet saturerer aldrig: samme form som Engine

        if (newStatic)
        {
            // Baand 0, 2 og 3 staar stille (de afhaenger kun af raten og 24 / 48 / 96)
            XoverCoefs xc[kEnds];
            for (int k = 0; k < kEnds; ++k)
                if (k < 4 || k >= 8) xc[k] = designEnd (k, L, fs, sp);
            static const int e0[] = { 0, 1, 2, 3 }, e23[] = { 8, 9, 10, 11, 12, 13, 14, 15 };
            h23.set (e23, 8, xc);
            tch.set (e0, 4, xc);
            haveStatic = true;
            staticMode = p.phaseMode;
        }

        // Baand 1's fire crossovers: kun naar kanterne har flyttet sig (staar de stille - Amount 0, Square paa toppen,
        // offset der klipper - er koefficienterne de samme som sidst)
        if (newBand)
        {
            gch0.n = 4;
            for (int k = 4; k < 8; ++k)
            {
                const int side = k % 4;                              // som designEnd: mur (0, 3) eller fade-graense
                gch0.ap[k - 4].set (designXoverAp ((side == 0 || side == 3) ? sp.wallN6 : sp.tailN6, L.end[k], fs));
            }
            haveBand = true;
            lastLo   = p.band[1].lo;
            lastHi   = p.band[1].hi;
            bandMode = p.phaseMode;
        }
        setTree (p);
    }

    void process (float* left, float* right, int n)
    {
        if (! treeOn) return;                      // Bypass: lyden er uroert (bit-identisk)

        for (int s = 0; s < n; ++s)
        {
            treeMix.target ((treeWanted && warmLeft == 0) ? 1.0 : 0.0);
            const double tm = treeMix.next();
            if (warmLeft > 0) --warmLeft;

            const __m128d x = _mm_set_pd ((double) right[s], (double) left[s]);
            const __m128d T = tch.process (gch0.process (h23.process (x)));

            alignas (16) double o[2];
            _mm_store_pd (o, _mm_add_pd (mul (x, 1.0 - tm), mul (T, tm)));
            left[s]  = (float) o[0];
            right[s] = (float) o[1];

            if (! treeWanted && tm <= 1.0e-6)
            {
                treeOn = false;                    // faerdig: resten af blokken er uroert (dry)
                return;
            }
        }
    }

private:
    void resetTree() { h23.reset(); gch0.reset(); tch.reset(); }

    void setTree (const EngineParams& p)
    {
        treeWanted = ! p.bypass;
        if (treeWanted && ! treeOn)
        {
            treeOn = true;                         // koldstart: varm filtrene op mens dry stadig hoeres
            resetTree();
            warmLeft = warmSamples;
        }
    }

    ApChain2 h23, gch0, tch;                       // baand 2+3's, baand 1's og baand 0's allpasser
    Ramp     treeMix;
    double   fs = 176400.0;
    int      warmSamples = 10584, warmLeft = 0;
    bool     treeOn = false, treeWanted = false, haveStatic = false, haveBand = false;
    int      staticMode = -1, bandMode = -1;
    double   lastLo = 0.0, lastHi = 0.0;               // baand 1's kanter ved sidste design
};
}
