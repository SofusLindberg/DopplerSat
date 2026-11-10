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

// -----------------------------------------------------------------------------
//  1.7: baand 1's fire crossovers som TPT-SVF-allpasser (Zavalishin, "The Art of VA Filter Design")
// -----------------------------------------------------------------------------
// Brugeren (2026-11-10): "Bandet skal kunne nå helt ud i yderppunkterne af travel range, selv når rate (speed) er
// skruet helt op." Det kraever kanter, der flytter sig op til ca. 1200 oktaver i sekundet (Square, Sweep 100 %).
// Biquads (direkte form) giver niveauspring, naar koefficienterne aendres saa hurtigt (maalt i 1.2: peaks 7-27 ved
// 320-1000 okt/s); en TPT-SVF goer ikke (ca. 1.1-1.5). I ro er det det samme filter som designXoverAp (bilineaer med
// forvraengning ved fc, samme Q'er, samme fortegn): forskellen er kun afrunding.
// Koefficienten g = tan (pi fc / fs) glider lineaert sample for sample fra stykke til stykke, saa kanterne bevaeger
// sig glat i stedet for i trin hver 32. sample.
struct SvfXover
{
    // 2. orden: x = hp + 2R bp + lp, allpass = hp - 2R bp + lp = x - 4R bp (R = 1 / (2Q))
    // 1. orden: allpass = lp - hp = 2 lp - x, samme som designAllpass1 ((1 - s) / (1 + s))
    int    nSec = 0;
    bool   onePole = false;
    double R[8] = {};
    __m128d s1[8], s2[8], z1 = _mm_setzero_pd();
    double gNow = 0.0, gEnd = 0.0, gStep = 0.0;

    void design (int n6, double fc, double fs, bool snap)
    {
        n6 = (int) clampd (n6, 1, 32);
        const bool even = (n6 % 2) == 0;
        const SectionInfo si = sectionInfo (even ? n6 / 2 : n6);
        onePole = even && si.onePole;
        nSec = 0;
        for (int k = 0; k < si.nBiquad; ++k)                          // som designXoverAp
            if (even || ((k + 1) % 2) == 1) R[nSec++] = 0.5 / si.q[k];
        gEnd = std::tan (kPi * safeFreq (fc, fs) / fs);
        if (snap) gNow = gEnd;
    }
    void reset()
    {
        for (int i = 0; i < 8; ++i) s1[i] = s2[i] = _mm_setzero_pd();
        z1 = _mm_setzero_pd();
    }
    void beginBlock (int n) { gStep = n > 0 ? (gEnd - gNow) / (double) n : 0.0; }

    inline __m128d process (__m128d x)
    {
        const double g = gNow;
        gNow += gStep;
        if (onePole)
        {
            const double G = g / (1.0 + g);
            const __m128d v  = mul (_mm_sub_pd (x, z1), G);
            const __m128d lp = _mm_add_pd (v, z1);
            z1 = _mm_add_pd (lp, v);
            x = _mm_sub_pd (mul (lp, 2.0), x);
        }
        for (int i = 0; i < nSec; ++i)
        {
            const double r2 = 2.0 * R[i], d = 1.0 / (1.0 + r2 * g + g * g);
            const __m128d hp = mul (_mm_sub_pd (_mm_sub_pd (x, mul (s1[i], r2 + g)), s2[i]), d);
            const __m128d gh = mul (hp, g);
            const __m128d bp = _mm_add_pd (gh, s1[i]);
            s1[i] = _mm_add_pd (gh, bp);
            const __m128d gb = mul (bp, g);
            const __m128d lp = _mm_add_pd (gb, s2[i]);
            s2[i] = _mm_add_pd (gb, lp);
            x = _mm_sub_pd (x, mul (bp, 2.0 * r2));
        }
        return x;
    }
};

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
            // 1.7: SVF-allpasser, der glider hen til de nye kanter i loebet af stykket (foerste gang: straks)
            const bool snap = ! haveBand || p.phaseMode != bandMode;
            for (int k = 4; k < 8; ++k)
            {
                const int side = k % 4;                              // som designEnd: mur (0, 3) eller fade-graense
                band1[k - 4].design ((side == 0 || side == 3) ? sp.wallN6 : sp.tailN6, L.end[k], fs, snap);
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
        for (auto& b : band1) b.beginBlock (n);    // kanterne glider hen til stykkets nye position
        if (! treeOn)                              // Bypass: lyden er uroert (bit-identisk)
        {
            for (auto& b : band1) b.gNow = b.gEnd;
            return;
        }

        for (int s = 0; s < n; ++s)
        {
            treeMix.target ((treeWanted && warmLeft == 0) ? 1.0 : 0.0);
            const double tm = treeMix.next();
            if (warmLeft > 0) --warmLeft;

            const __m128d x = _mm_set_pd ((double) right[s], (double) left[s]);
            __m128d B = h23.process (x);
            for (auto& b : band1) B = b.process (B);
            const __m128d T = tch.process (B);

            alignas (16) double o[2];
            _mm_store_pd (o, _mm_add_pd (mul (x, 1.0 - tm), mul (T, tm)));
            left[s]  = (float) o[0];
            right[s] = (float) o[1];

            if (! treeWanted && tm <= 1.0e-6)
            {
                treeOn = false;                    // faerdig: resten af blokken er uroert (dry)
                for (auto& b : band1) b.gNow = b.gEnd;
                return;
            }
        }
    }

    // 1.9 Smooth Peaks: hovedmotoren (a, l / r) og reference-motoren (b, ud i rl / rr) i een loekke, med samme lyd ind.
    // Bit for bit det samme som a.process (l, r, n) og b.process paa en kopi af l / r, men baand 2+3's faste allpasser
    // (h23) regnes kun een gang: de faar samme lyd ind, har samme koefficienter (afhaenger kun af raten og 24 / 48 / 96)
    // og nulstilles samtidig i begge (prepare og Bypass), saa deres udgang er den samme. b's egen h23 bruges ikke.
    // Kraever, at begge har faaet de samme prepare- og Bypass-kald (saa er treeOn, treeMix og warmLeft ens).
    static void processPair (DopplerCore& a, DopplerCore& b, float* l, float* r, float* rl, float* rr, int n)
    {
        jassert (a.treeOn == b.treeOn && a.treeWanted == b.treeWanted && a.warmLeft == b.warmLeft);
        for (auto& x : a.band1) x.beginBlock (n);
        for (auto& x : b.band1) x.beginBlock (n);
        if (! a.treeOn)                            // Bypass: begge uroerte
        {
            for (auto& x : a.band1) x.gNow = x.gEnd;
            for (auto& x : b.band1) x.gNow = x.gEnd;
            std::copy (l, l + n, rl);
            std::copy (r, r + n, rr);
            return;
        }

        for (int s = 0; s < n; ++s)
        {
            a.treeMix.target ((a.treeWanted && a.warmLeft == 0) ? 1.0 : 0.0);
            const double tm = a.treeMix.next();
            if (a.warmLeft > 0) --a.warmLeft;
            b.treeMix.target ((b.treeWanted && b.warmLeft == 0) ? 1.0 : 0.0);
            const double tmB = b.treeMix.next();
            if (b.warmLeft > 0) --b.warmLeft;

            const __m128d x = _mm_set_pd ((double) r[s], (double) l[s]);
            const __m128d H = a.h23.process (x);
            __m128d B = H, BB = H;
            for (auto& q : a.band1) B  = q.process (B);
            for (auto& q : b.band1) BB = q.process (BB);
            const __m128d T = a.tch.process (B), TB = b.tch.process (BB);

            alignas (16) double o[2], oB[2];
            _mm_store_pd (o,  _mm_add_pd (mul (x, 1.0 - tm),  mul (T,  tm)));
            _mm_store_pd (oB, _mm_add_pd (mul (x, 1.0 - tmB), mul (TB, tmB)));
            l[s]  = (float) o[0];   r[s]  = (float) o[1];
            rl[s] = (float) oB[0];  rr[s] = (float) oB[1];

            if (! a.treeWanted && tm <= 1.0e-6)    // faerdig med at tone ud (sker samtidig i begge)
            {
                a.treeOn = b.treeOn = false;
                for (auto& q : a.band1) q.gNow = q.gEnd;
                for (auto& q : b.band1) q.gNow = q.gEnd;
                std::copy (l + s + 1, l + n, rl + s + 1);
                std::copy (r + s + 1, r + n, rr + s + 1);
                return;
            }
        }
    }

    // 1.9: er alt, der bestemmer lyden fremover, det samme bit for bit (undtagen h23, som deles i processPair)? Saa
    // giver de to motorer samme lyd, saa laenge de faar samme kanter, og reference-motoren behoever ikke regnes.
    bool sameStateAs (const DopplerCore& o) const
    {
        auto eq = [] (__m128d a, __m128d b)
        {
            alignas (16) double x[2], y[2];
            _mm_store_pd (x, a); _mm_store_pd (y, b);
            return x[0] == y[0] && x[1] == y[1];
        };
        if (fs != o.fs || warmSamples != o.warmSamples || warmLeft != o.warmLeft || treeOn != o.treeOn
             || treeWanted != o.treeWanted || haveStatic != o.haveStatic || haveBand != o.haveBand
             || staticMode != o.staticMode || bandMode != o.bandMode || lastLo != o.lastLo || lastHi != o.lastHi
             || treeMix.v != o.treeMix.v || treeMix.t != o.treeMix.t || treeMix.a != o.treeMix.a || tch.n != o.tch.n)
            return false;
        for (int k = 0; k < 4; ++k)
        {
            const auto& p = band1[k];
            const auto& q = o.band1[k];
            if (p.nSec != q.nSec || p.onePole != q.onePole || p.gNow != q.gNow || p.gEnd != q.gEnd) return false;
            if (p.onePole && ! eq (p.z1, q.z1)) return false;
            for (int i = 0; i < p.nSec; ++i)
                if (p.R[i] != q.R[i] || ! eq (p.s1[i], q.s1[i]) || ! eq (p.s2[i], q.s2[i])) return false;
        }
        for (int i = 0; i < tch.n; ++i)
        {
            const auto& p = tch.ap[i];
            const auto& q = o.tch.ap[i];
            if (p.n != q.n) return false;
            for (int j = 0; j < p.n; ++j)
            {
                const auto& u = p.s[j];
                const auto& v = q.s[j];
                if (! eq (u.z1, v.z1) || ! eq (u.z2, v.z2) || ! eq (u.b0, v.b0) || ! eq (u.b1, v.b1)
                     || ! eq (u.b2, v.b2) || ! eq (u.a1, v.a1) || ! eq (u.a2, v.a2))
                    return false;
            }
        }
        return true;
    }

private:
    void resetTree() { h23.reset(); for (auto& b : band1) b.reset(); tch.reset(); }

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

    ApChain2 h23, tch;                             // baand 2+3's og baand 0's allpasser (staar stille)
    SvfXover band1[4];                             // 1.7: baand 1's fire crossovers (flytter sig)
    Ramp     treeMix;
    double   fs = 176400.0;
    int      warmSamples = 10584, warmLeft = 0;
    bool     treeOn = false, treeWanted = false, haveStatic = false, haveBand = false;
    int      staticMode = -1, bandMode = -1;
    double   lastLo = 0.0, lastHi = 0.0;               // baand 1's kanter ved sidste design
};
}
