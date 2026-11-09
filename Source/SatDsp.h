// SatDsp.h - JUCE-fri lydmotor + filtermatematik til Multiband Saturator.
// Bruges baade af processoren (lyd) og editoren (tegning af kurver), saa
// det der TEGNES altid er det der FAKTISK sker i lyden.
//
// Tre fasetilstande (global parameter, tre knapper i editoren):
//   Lineaer fase:  baandene deles med lange FIR-filtre (nul fase, ca. 0.2 / 0.4 / 0.8 s forsinkelse efter
//                  skarphed). Ingen pitch-aendring naar baandene flyttes, og kanterne er (naesten) lodrette.
//                  Se LinSplit.
//   0 ms 24 / 48:  ingen forsinkelse. Hvert baand deler lyden i tre dele (under / omraadet / over) med
//                  Linkwitz-Riley crossovers, og alle allpass-faserne laegges paa alt, saa alle dele er
//                  "i fase" og den toerre sum er en ren allpass. Stejle mure der flyttes giver Doppler
//                  (pitch-aendring), derfor kun 24 eller 48 dB/okt. Se Engine.
// Hvert baand kan have op til 3 frekvenspunkter (EQ-kurver der styrer hvor haardt der drives), og
// saturationen laegges som en DELTA oven paa den toerre lyd (BandSat).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

// Pitch Bend Texture 1.5: SSE2 kun paa Intel / AMD. Paa Apple Silicon (og med DS_FORCE_SIMD_SHIM=1) de samme
// funktioner uden SSE2 (SimdShim.h, bit for bit samme lyd).
#if (defined (_M_X64) || defined (__x86_64__) || defined (_M_IX86) || defined (__i386__)) && ! DS_FORCE_SIMD_SHIM
 #include <emmintrin.h>
#else
 #include "SimdShim.h"
#endif

namespace satdsp
{
constexpr int    kBands   = 4;
constexpr int    kPoints  = 3;
constexpr double kPi       = 3.141592653589793238462643383279502884;
constexpr double kDbPerPct  = 36.12359947967774 / 400.0;   // 20*log10(64)/400 : 1 % drive = 0.0903 dB
constexpr double kDriveAxisExp   = 0.4;    // editorens drive-akse: hoejde = (drive / 400) ^ 0.4 (samme skew som slideren)
constexpr double kDisplayRangeDb = 30.0;   // Edge Q-vaegten tegnes i dB ned til -30 dB (= 0 hoejde)

// Fasetilstande (parameteren "phase")
constexpr int kPhaseLinear = 0;            // lineaer fase (FIR), ca. 0.2 s forsinkelse
constexpr int kPhaseZl24   = 1;            // 0 ms, 24 dB/okt
constexpr int kPhaseZl48   = 2;            // 0 ms, 48 dB/okt
constexpr int kPhaseZl96   = 3;            // 0 ms, 96 dB/okt (kun Doppler Saturator, 1.2)

inline double clampd (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double driveToGain (double pct)               { return std::exp (pct * std::log (64.0) / 400.0); }

// ---------------------------------------------------------------------------
//  Biquad-design (RBJ). Alt i double.
// ---------------------------------------------------------------------------
struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coef normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    Coef c;
    c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
    return c;
}

inline double safeFreq (double f, double fs) { return clampd (f, 1.0, fs * 0.499); }

inline Coef designLowpass1 (double fc, double fs)
{
    const double K = std::tan (kPi * safeFreq (fc, fs) / fs);
    Coef c; c.b0 = K / (1.0 + K); c.b1 = c.b0; c.a1 = (K - 1.0) / (K + 1.0);
    return c;
}

inline Coef designHighpass1 (double fc, double fs)
{
    const double K = std::tan (kPi * safeFreq (fc, fs) / fs);
    Coef c; c.b0 = 1.0 / (1.0 + K); c.b1 = -c.b0; c.a1 = (K - 1.0) / (K + 1.0);
    return c;
}

inline Coef designAllpass1 (double fc, double fs)          // (1 - s)/(1 + s)
{
    const double K = std::tan (kPi * safeFreq (fc, fs) / fs);
    const double a = (K - 1.0) / (K + 1.0);
    Coef c; c.b0 = a; c.b1 = 1.0; c.a1 = a;
    return c;
}

struct Rbj
{
    double cosw, sinw, alpha, A;
    Rbj (double fc, double q, double gainDb, double fs)
    {
        const double w0 = 2.0 * kPi * safeFreq (fc, fs) / fs;
        cosw  = std::cos (w0);
        sinw  = std::sin (w0);
        alpha = sinw / (2.0 * std::max (q, 0.05));
        A     = std::pow (10.0, gainDb / 40.0);
    }
};

inline Coef designLowpass  (double fc, double q, double fs)
{ Rbj r (fc, q, 0, fs); return normalise ((1 - r.cosw) / 2, 1 - r.cosw, (1 - r.cosw) / 2, 1 + r.alpha, -2 * r.cosw, 1 - r.alpha); }

inline Coef designHighpass (double fc, double q, double fs)
{ Rbj r (fc, q, 0, fs); return normalise ((1 + r.cosw) / 2, -(1 + r.cosw), (1 + r.cosw) / 2, 1 + r.alpha, -2 * r.cosw, 1 - r.alpha); }

inline Coef designAllpass2 (double fc, double q, double fs)
{ Rbj r (fc, q, 0, fs); return normalise (1 - r.alpha, -2 * r.cosw, 1 + r.alpha, 1 + r.alpha, -2 * r.cosw, 1 - r.alpha); }

inline Coef designNotch    (double fc, double q, double fs)
{ Rbj r (fc, q, 0, fs); return normalise (1, -2 * r.cosw, 1, 1 + r.alpha, -2 * r.cosw, 1 - r.alpha); }

inline Coef designBandpass (double fc, double q, double fs)
{ Rbj r (fc, q, 0, fs); return normalise (r.alpha, 0, -r.alpha, 1 + r.alpha, -2 * r.cosw, 1 - r.alpha); }

inline Coef designPeak (double fc, double q, double gainDb, double fs)
{
    Rbj r (fc, q, gainDb, fs);
    return normalise (1 + r.alpha * r.A, -2 * r.cosw, 1 - r.alpha * r.A,
                      1 + r.alpha / r.A, -2 * r.cosw, 1 - r.alpha / r.A);
}

inline Coef designLowShelf (double fc, double q, double gainDb, double fs)
{
    Rbj r (fc, q, gainDb, fs);
    const double A = r.A, c = r.cosw, b = 2.0 * std::sqrt (A) * r.alpha;
    return normalise (A * ((A + 1) - (A - 1) * c + b), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - b),
                      (A + 1) + (A - 1) * c + b, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - b);
}

inline Coef designHighShelf (double fc, double q, double gainDb, double fs)
{
    Rbj r (fc, q, gainDb, fs);
    const double A = r.A, c = r.cosw, b = 2.0 * std::sqrt (A) * r.alpha;
    return normalise (A * ((A + 1) + (A - 1) * c + b), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - b),
                      (A + 1) - (A - 1) * c + b, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - b);
}

// Punkt-typer: 0 Bell, 1 Low shelf, 2 High shelf, 3 High pass, 4 Low pass, 5 Notch, 6 Band pass
inline Coef designPoint (int type, double fc, double q, double gainDb, double fs)
{
    switch (type)
    {
        case 1:  return designLowShelf  (fc, q, gainDb, fs);
        case 2:  return designHighShelf (fc, q, gainDb, fs);
        case 3:  return designHighpass  (fc, q, fs);
        case 4:  return designLowpass   (fc, q, fs);
        case 5:  return designNotch     (fc, q, fs);
        case 6:  return designBandpass  (fc, q, fs);
        default: return designPeak      (fc, q, gainDb, fs);
    }
}

inline std::complex<double> coefResponse (const Coef& c, double f, double fs)
{
    const double w = 2.0 * kPi * f / fs;
    const std::complex<double> z1 (std::cos (w), -std::sin (w));      // e^{-jw}
    const std::complex<double> z2 = z1 * z1;
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}


// ---------------------------------------------------------------------------
//  Butterworth-sektioner og crossover-familien (6..192 dB/oktav i trin af 6)
// ---------------------------------------------------------------------------
struct SectionInfo { bool onePole = false; int nBiquad = 0; double q[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; };

inline SectionInfo sectionInfo (int order)
{
    SectionInfo s;
    s.onePole = (order % 2) == 1;
    s.nBiquad = order / 2;
    for (int k = 1; k <= s.nBiquad; ++k)
    {
        const double ang = s.onePole ? (k * kPi / order) : ((2.0 * k - 1.0) * kPi / (2.0 * order));
        s.q[k - 1] = 1.0 / (2.0 * std::cos (ang));
    }
    return s;
}

struct SeqCoefs
{
    static constexpr int kMax = 16;
    int count = 0; Coef c[kMax];
    void add (const Coef& k) { if (count < kMax) c[count++] = k; }
};

inline SeqCoefs designChain (int order, bool hp, double fc, double fs)
{
    SeqCoefs cc;
    const SectionInfo si = sectionInfo (order);
    if (si.onePole) cc.add (hp ? designHighpass1 (fc, fs) : designLowpass1 (fc, fs));
    for (int i = 0; i < si.nBiquad; ++i)
        cc.add (hp ? designHighpass (fc, si.q[i], fs) : designLowpass (fc, si.q[i], fs));
    return cc;
}

inline std::complex<double> seqResponse (const SeqCoefs& s, double f, double fs)
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < s.count; ++i) h *= coefResponse (s.c[i], f, fs);
    return h;
}

// Et crossover-par. n6 = haeldning/6 (1..32):
//   lige n6  -> Linkwitz-Riley (BW(n6/2) i anden): LP og HP er i fase, sum = allpass
//   ulige n6 -> Butterworth (BW(n6)): LP og HP er 90 grader forskudt, sum = allpass
//   sign = -1 for LR2 og LR6 (HP skal vendes for at summen bliver en allpass)
struct XoverCoefs
{
    int n6 = 1; double sign = 1.0; int p = 1;      // p: potens der goer vaegtene til en "partition of unity"
    SeqCoefs lp, hp, ap;                           // ap = allpass der kompenserer baand udenfor dette crossover
};

inline XoverCoefs designXover (int n6, double fc, double fs)
{
    n6 = (int) clampd (n6, 1, 32);
    XoverCoefs x;
    x.n6 = n6;
    const bool even = (n6 % 2) == 0;
    const int  N    = even ? n6 / 2 : n6;
    const SeqCoefs lpc = designChain (N, false, fc, fs), hpc = designChain (N, true, fc, fs);
    for (int r = 0; r < (even ? 2 : 1); ++r)
    {
        for (int i = 0; i < lpc.count; ++i) x.lp.add (lpc.c[i]);
        for (int i = 0; i < hpc.count; ++i) x.hp.add (hpc.c[i]);
    }
    x.sign = (even && (N % 2) == 1) ? -1.0 : 1.0;
    x.p    = even ? 1 : 2;

    const SectionInfo si = sectionInfo (N);
    if (even)
    {
        if (si.onePole) x.ap.add (designAllpass1 (fc, fs));
        for (int k = 0; k < si.nBiquad; ++k) x.ap.add (designAllpass2 (fc, si.q[k], fs));
    }
    else
    {
        for (int k = 0; k < si.nBiquad; ++k)
            if (((k + 1) % 2) == 1) x.ap.add (designAllpass2 (fc, si.q[k], fs));   // kun ulige k
    }
    return x;
}

// ---------------------------------------------------------------------------
//  Parametre
// ---------------------------------------------------------------------------
struct PointParams { bool on = false; int type = 0; double freq = 1000, gainDb = 0, q = 1; };

struct BandParams
{
    double lo = 20, hi = 20000;
    int    type = 0;                 // 0 Soft, 1 Hard, 2 Tube, 3 Fold, 4 Warm
    double drive = 0;                // 0..400 %
    double mix = 1;                  // 0 = dry (uden drive), 1 = wet (med drive)
    bool   edgeOn = false;           // Edge Q til: saturationen fader ud udenfor kanterne. Fra: lodret kant
    double edgeQ  = 50;              // 0..100 %: 0 = langsomt fade over flere oktaver, 100 = naesten lodret mur
    bool   on = true;                // baandet er taendt (3.8). Slukket = ingen saturation; kanterne holder
                                     // ikke naboerne paa plads, og baandet husker sin placering
    PointParams pts[kPoints];
};

struct EngineParams
{
    BandParams band[kBands];
    int    soloBand  = -1;
    int    soloPoint = -1;           // solo af et frekvenspunkt i soloBand (3.8, se PointSolo), -1 = fra
    bool   bypass    = false;
    int    phaseMode = kPhaseLinear;
    int    linSharp  = 0;            // lineaer fase: kanternes skarphed 0 = normal, 1 = skarp (2x FIR), 2 = ekstrem (4x)
    double baseRate  = 44100;        // vaertens samplingrate (uden oversampling) - de lineaere filtre koerer paa den
};

constexpr int kPointNotch    = 5;
constexpr int kPointBandpass = 6;
constexpr int kRelSections   = 4;          // en relativ kurve er 4 ens svage sektioner (se designPoints)
constexpr int kBellCopies    = 4;          // Bell (3.17): 4 ens kopier af 3 biquads (se designBell)
constexpr int kPassCopies    = 8;          // High / Low pass' kant (3.17): 8 kopier af to shelves (var 4: med den dybere
                                           // kant under resonansbulen afveg formen ved 20 og 200 % 0.11)
constexpr int kPointSections = 32;         // Band pass (3.18) 32, High / Low pass med resonans 24, Bell 12, notchen 5
constexpr double kShelfQMax  = 0.70710678118654752;   // High / Low pass: op til denne Q er kanten een RBJ-shelf med
                                                      // punktets Q, over den bliver den stejlere (se passShape)
constexpr double kPassQMax   = 18.0;       // punktets hoejeste Q (3.16, som EQ Eight; 3.15: 10)
constexpr double kPassSteepQ = 10.0;       // High / Low pass' kant er stejlest (4. orden Butterworth) fra Q 10; over
                                           // det bliver kun resonanstoppen smallere og hoejere
constexpr double kButterQ4[2] = { 0.54119610014619698, 1.3065629648763766 };   // 4. ordens Butterworth-shelfens to Q
constexpr double kPassAnchor = 0.1;        // High / Low pass: paa punktets frekvens er 10 % af driven fjernet
constexpr double kPassMargin = 0.25;       // ... og kanten skaerer 25 % dybere end driven, saa den naar 0 % tydeligt
constexpr double kResHeight  = 1.5;        // High / Low pass' resonansbule (3.17) ved Q 18: toppen loefter driven med
                                           // 1.5 * ref dB (3.16: 3.1 * ref, en spids naal; brugeren: "alt for højt-ragende")
constexpr double kResWide    = 1.5;        // bulens hale: dens Bell-sektioner har Q fra 1 / kResWide (bredest) ...
constexpr double kResComp    = 2.0;        // stopbaandets kant skaerer kResComp * bulens dB dybere, saa bulens nederste
                                           // skulder ikke loefter stopbaandet (hoejden dér falder stadig med Q)
constexpr double kNotchWidth    = 2.685;   // notchens Bell-sektioner har Q = punktets Q * kNotchWidth
constexpr double kNotchKill  = 0.002;      // notchens nul: dens skuldre fjerner kNotchKill / u^2 af driven (se nedenfor)
constexpr double kPointMaxDb = 400.0 * kDbPerPct + 24.0;   // resonansen loefter aldrig over 400 % + 24 dB
constexpr double kPointGainMax = 36.13;    // punkternes gain-omraade +/- dB (3.17; var 24): 400 % = 36.12 dB, saa en Bell kan
                                           // traekkes helt op til 400 % ved 0 % baseline (brugeren 31-10)
constexpr int    kPeakProbes = 24;         // proever pr. omraade, naar de faste punkters hoejeste loeft findes

// Et punkts biquads. De sidste nBoost (3.17) er loeft (Bell / shelves med positiv gain, High / Low pass'
// resonansbule) og bestaar af to ens halvdele S S: BandSat laegger 1 / S efter formeren (se BandSat).
struct PointCoefs { Coef c[kPointSections]; int n = 1; int nBoost = 0; };

// 1 / H for en biquad med taelleren inden for enhedscirklen (loeftene er minimumsfase)
inline Coef inverseCoef (const Coef& c) { return normalise (1.0, c.a1, c.a2, c.b0, c.b1, c.b2); }

inline double coefsDb (const PointCoefs& pc, double f, double fs)
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < pc.n; ++i) h *= coefResponse (pc.c[i], f, fs);
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-9));
}

// Bell, Low shelf og High shelf er faste punkter (de har gain); de andre typer er relative
inline bool isFixedPoint (int type) { return type <= 2; }

// En RBJ low shelfs form i dB ved lille styrke: 1 under, 0.5 paa og 0 over shelf-frekvensen (w = f / fm).
// Under Q 0.71 er den monoton og bredere jo lavere Q; over 0.71 slaar den over (resonans).
inline double shelfShape (double w, double q)
{
    const double a = 1.0 - w * w, b = w * w / (q * q);
    return (a + 0.5 * b) / (a * a + b);
}

// High / Low pass' kant (3.15): to RBJ-shelves med Q qa og qb, hver med halvdelen af dybden. Op til Q 0.71 er
// begge punktets Q (een shelf: jo lavere Q, jo bredere og bloedere). Over 0.71 glider de mod 4. ordens
// Butterworth (Q 0.54 og 1.31) ved Q 10 (kPassSteepQ): stejlere kant, stadig uden at slaa over. Kanten forankres, saa
// kPassAnchor af driven er fjernet paa punktets frekvens: r = fc / fm (High pass; Low pass spejlet).
struct PassShape { double qa, qb, r; };

inline PassShape passShape (double q)
{
    PassShape s;
    if (q <= kShelfQMax) s.qa = s.qb = q;
    else
    {
        const double t = std::sqrt (std::min (1.0, std::log (q / kShelfQMax) / std::log (kPassSteepQ / kShelfQMax)));
        s.qa = kShelfQMax * std::pow (kButterQ4[0] / kShelfQMax, t);
        s.qb = kShelfQMax * std::pow (kButterQ4[1] / kShelfQMax, t);
    }
    const double target = kPassAnchor / (1.0 + kPassMargin);
    double lo = 0.0, hi = std::log (1.0e4);                  // log r: formen er 0.5 i r = 1 og falder mod 0
    for (int i = 0; i < 50; ++i)
    {
        const double m = 0.5 * (lo + hi), w = std::exp (m);
        (0.5 * (shelfShape (w, s.qa) + shelfShape (w, s.qb)) > target ? lo : hi) = m;
    }
    s.r = std::exp (0.5 * (lo + hi));
    return s;
}

// Band pass (3.18, brugeren 03-11: "en smule mere rundet og mindre spids ved høje Q-værdier for at gøre den
// anderledes fra bell [...] både høje og lave Q-værdier [skal] have mere lodret kurve ned i siderne med en blød
// runding i bølgetoppen. Høj og lav Q-værdi er derfor mere et spørgsmål om, at de tilnærmelsesvist lodrette kanter
// medtager flere eller færre frekvenser", med Pro-Q's og en lærebogs band pass som reference): to stejle kanter, en
// High pass-kant under og en Low pass-kant over frekvensen. Hver kant er en Butterworth-shelf af orden
// 2 * kBpOrder (samme fm, lige store dele af dybden, saa den er monoton ved al styrke, Holters-Zoelzer). Kanterne
// har altid samme stejlhed; Q bestemmer kun bredden (bpWidthOct) mellem de to halvhoejde-punkter. Er baandet
// smallere end kanterne, overlapper de, og toppen bliver en rund bule; den skaleres med k, saa toppen altid er
// praecis baseline (k <= kBpMaxK). Formen i dB er s = k (1 - (1 + kPassMargin) (L + H)) af ref, saa den naar 0 %
// tydeligt i begge sider.
constexpr int    kBpOrder  = 4;            // sektioner pr. kant (8. ordens Butterworth-shelf)
constexpr int    kBpCopies = 4;            // ens kopier af hver kant (formen holder ved al drive), 2 * 4 * 4 = 32 biquads.
                                           // En RBJ-shelfs dB er ulige i gain'en, saa formfejlen er ~ (gain pr. sektion)^2:
                                           // med 3 kopier afveg Q 18 (kanterne overlapper, k = 1.25) 0.08 ved 20 og 200 %
static_assert (2 * kBpOrder * kBpCopies <= kPointSections, "Band pass skal kunne vaere i et punkts sektioner");
static_assert (2 * kPassCopies + 2 * kRelSections <= kPointSections, "High / Low pass skal kunne vaere i et punkts sektioner");
constexpr double kBpQ[kBpOrder] = { 0.50979557910415917, 0.60134488693504528, 0.89997622313641570, 2.5629154477415055 };
constexpr double kBpMaxK   = 2.0;          // toppen loeftes hoejst 2 x tilbage, naar kanterne overlapper
constexpr double kBpMinOct = 0.42;         // bredden i halv hoejde ved Q 18 (smallest; en rund bule, ikke en spids)

// Bredden i oktaver mellem halvhoejde-punkterne: op til Q 1 et RBJ-baandpas' (Q 0.1 = 6.6 okt, Q 1 = 1.39), derfra
// logaritmisk ned til kBpMinOct ved Q 18, saa baandet bliver ved med at blive smallere, jo hoejere Q
inline double bpWidthOct (double q)
{
    auto rbj = [] (double qq) { return 2.0 * std::asinh (0.5 / qq) / std::log (2.0); };
    if (q <= 1.0) return rbj (q);
    const double t = std::log (q) / std::log (kPassQMax);
    return rbj (1.0) * std::pow (kBpMinOct / rbj (1.0), std::min (t, 1.0));
}

struct BandpassShape { double r, k; };       // kanterne i fc / r og fc * r, toppen skaleret med k

inline double bpEdge (double w)              // en kants form i dB ved lille styrke: 1 under fm, 0 over (w = f / fm)
{
    double s = 0.0;
    for (double q : kBpQ) s += shelfShape (w, q);
    return s / kBpOrder;
}

inline BandpassShape bandpassShape (double q)
{
    const double half = std::exp2 (0.5 * bpWidthOct (q));            // halvhoejde-punktet i fc * half
    auto shape = [] (double r, double x)                             // (k, s (x)) for kanter i 1 / r og r
    {
        const double c0 = 2.0 * bpEdge (r);                          // L + H paa frekvensen
        const double k  = 1.0 / (1.0 - (1.0 + kPassMargin) * c0);
        return std::make_pair (k, k * (1.0 - (1.0 + kPassMargin) * (bpEdge (r / x) + bpEdge (r * x))));
    };
    // mindste r: k = kBpMaxK (bpEdge (r) aftager med r)
    const double cMax = 0.5 * (1.0 - 1.0 / kBpMaxK) / (1.0 + kPassMargin);
    double lo = std::log (1.0e-3), hi = std::log (1.0e3);
    for (int i = 0; i < 60; ++i)
    {
        const double m = 0.5 * (lo + hi);
        (bpEdge (std::exp (m)) > cMax ? lo : hi) = m;
    }
    const double rMin = std::exp (hi);
    BandpassShape b { rMin, shape (rMin, half).first };
    if (shape (rMin, half).second >= 0.5) return b;                  // smallere er umuligt: den runde bule
    lo = std::log (rMin); hi = std::log (1.0e3);
    for (int i = 0; i < 60; ++i)
    {
        const double m = 0.5 * (lo + hi);
        (shape (std::exp (m), half).second < 0.5 ? lo : hi) = m;
    }
    b.r = std::exp (0.5 * (lo + hi));
    b.k = shape (b.r, half).first;
    return b;
}

// Bell (3.17, brugeren 31-10: "Bell skal kunne blive endnu smallere, lige så smal som Abletons", med et billede af
// EQ Eight ved Q 18). Drive-aksen er skaev (hoejde = drive^0.4), saa en almindelig Bell's svage fod ser bred ud dér,
// selvom den er usynlig paa EQ Eights lineaere dB-akse. Bell'ens form i dB er derfor (1 + z^2)^-3 i stedet for et
// RBJ-filters 1 / (1 + z^2), z = Q (W - 1/W): paa drive-aksen ser den saa ud som EQ Eights Bell med samme Q (ved
// 0 % baseline hoejde ~ (1 + z^2)^-1.2, lidt smallere end EQ Eights 1 / (1 + z^2) alle steder).
//   Kernen P(x) = 3/8 x + 3/8 x^2 + 1/4 x^3, x = RBJ-baandpasset (paa enhedscirklen x = 1 / (1 + jz)), har
//   Re P = (1 + z^2)^-3 og |P| <= 1. En kopi er (1 + d P) / (1 - d P) med d = tanh (gain_kopi * ln10 / 40): toppen er
//   praecis gain_kopi dB, og taeller og naevner er stabile og minimumsfase for |d| < 1 (altid). Ved lille gain er
//   kopiens dB = gain_kopi * Re P; ved hoej gain bliver foden en smule lavere (smallere), aldrig bredere.
//   kBellCopies kopier med hver gain / kBellCopies. Hver kopi er 3 biquads: 1 + d P(x) = prod (1 - x / x_i) for
//   roedderne x_i af den kubiske ligning, og (1 - x / x_i) = (Dd - Nd / x_i) / Dd med x = Nd / Dd. Dd^3 gaar ud.
//   Et komplekst rodpar giver en reel 4. grads faktor, der deles i to reelle 2. grads faktorer (to resonanser over og
//   under frekvensen). Taellerens og naevnerens faktorer parres efter frekvens.
namespace detail
{
    struct Quad { double c0, c1, c2; };      // c0 + c1 w + c2 w^2, w = z^-1

    // Faktorerne af prod_i (Dd - Nd / x_i) for roedderne af 1 + d P(x) = 0, d != 0
    inline void bellFactors (double d, double alpha, double cosw, Quad out[3])
    {
        // x^3 + 1.5 x^2 + 1.5 x + 4 / d = 0; t = x + 0.5 giver t^3 + 0.75 t + q = 0 (een reel, to komplekse roedder)
        const double p = 0.75, q = 4.0 / d - 0.5;
        const double s = std::sqrt (q * q / 4.0 + p * p * p / 27.0);
        const double u = std::cbrt (-q / 2.0 - (q >= 0.0 ? s : -s));    // den store rod (ingen udslukning)
        const double t1 = u - p / (3.0 * u);
        const std::complex<double> t2 (-t1 / 2.0, std::sqrt (3.0 * t1 * t1 / 4.0 + p));

        const double br = alpha * (1.0 - 1.0 / (t1 - 0.5));
        out[0] = { 1.0 + br, -2.0 * cosw, 1.0 - br };
        const std::complex<double> bc = alpha * (1.0 - 1.0 / (t2 - 0.5));
        const std::complex<double> sq = std::sqrt (cosw * cosw - 1.0 + bc * bc);
        std::complex<double> w[2] = { (cosw + sq) / (1.0 - bc), (cosw - sq) / (1.0 - bc) };
        if (std::abs (std::arg (w[0])) > std::abs (std::arg (w[1]))) std::swap (w[0], w[1]);
        const double m = std::abs (1.0 - bc);
        for (int i = 0; i < 2; ++i)
            out[1 + i] = { m * std::norm (w[i]), -2.0 * m * w[i].real(), m };
    }
}

// Bell'ens 3 * kBellCopies biquads (out skal have plads til dem)
inline void designBell (double fc, double q, double gainDb, double fs, Coef* out)
{
    for (int i = 0; i < 3 * kBellCopies; ++i) out[i] = Coef();
    if (std::abs (gainDb) < 1.0e-9) return;
    const Rbj r (fc, q, 0.0, fs);
    const double d = std::tanh (gainDb / kBellCopies * std::log (10.0) / 40.0);
    detail::Quad n[3], dn[3];
    detail::bellFactors (d, r.alpha, r.cosw, n);
    detail::bellFactors (-d, r.alpha, r.cosw, dn);
    for (int i = 0; i < 3; ++i)
    {
        const Coef c = normalise (n[i].c0, n[i].c1, n[i].c2, dn[i].c0, dn[i].c1, dn[i].c2);
        for (int k = 0; k < kBellCopies; ++k) out[3 * k + i] = c;
    }
}

// Den hoejeste drive i dB i [lo, hi], som baseline + de taendte faste punkter giver (aldrig under 0 dB).
// Proeves i kPeakProbes logaritmiske skridt og praecis paa hvert fast punkts frekvens (en Bells top ligger dér).
inline double peakDriveDb (double driveDb, const PointCoefs out[kPoints], const bool fixed[kPoints], const double fc[kPoints],
                           double lo, double hi, double fs)
{
    if (! (fixed[0] || fixed[1] || fixed[2])) return driveDb;
    auto at = [&] (double f)
    {
        double db = driveDb;
        for (int k = 0; k < kPoints; ++k)
            if (fixed[k]) db += coefsDb (out[k], f, fs);
        return db;
    };
    double best = 0.0;
    for (int i = 0; i <= kPeakProbes; ++i)
        best = std::max (best, at (lo * std::pow (hi / lo, (double) i / kPeakProbes)));
    for (int k = 0; k < kPoints; ++k)
        if (fixed[k] && fc[k] >= lo && fc[k] <= hi)
            best = std::max (best, at (fc[k]));
    return best;
}

// Et baands punktfiltre (faelles for lyden og tegningen). fc holdes indenfor baandets kanter.
//
// Bell / Low shelf / High shelf (3.14, brugeren 31-10: "Vi skal have mulighed for at skrue op for gainen af de
// seperate frequency points [...] selvom drive (baseline) for båndet er skruet helt ned. [...] når drive (baseline)
// skrues op eller ned, skal værdien af frequency points tilsvarende skrues op eller ned"): eet fast RBJ-filter med
// punktets Q, der laegger sine dB til driven, som i 3.13's A "Fixed dB". 1 dB = 11 % drive, saa punktet foelger
// baseline op og ned med lige mange procent, og det loefter driven, ogsaa naar baseline er 0 %.
// Bell (3.17, se designBell): en smal kerne med formen (1 + z^2)^-3 i dB, saa den paa drive-aksen er lige saa smal
// som EQ Eights Bell med samme Q (3.16: fire RBJ-peaks med hver en fjerdedel af gain'en). Q gaar til 18, og gain til
// +/- kPointGainMax (36.13 dB = 400 %).
//
// De andre typer foelger driven (3.13's B, brugeren 31-10: "Vi kommer helt sikkert til at beholde indstilling B og
// fjerne indstilling A"). Deres dB er proportionale med den drive, de skal fjerne: ref = den hoejeste drive i det
// omraade, typen tager ned til 0 % (High / Low pass: baseline + de faste punkters loeft, se peakDriveDb; Band pass
// og notchens Bell-sektioner: se nedenfor). Uden faste punkter er ref
// baseline, og kurven har samme form ved al drive (den skaleres bare i hoejden). En svag sektion har samme form
// (i dB) uanset styrke, saa hver kurve er kRelSections ens svage sektioner (een dyb Bell blev smallere i bunden jo
// dybere: op til 0.16 i relativ hoejde mellem 20 % og 300 % drive):
//   High pass / Low pass (3.15, brugeren 31-10: "Jo højere Q skal gøre kanten endnu skarpere og skære flere dybere
//                         frekvenser af. Samtidigt skal højere Q ikke tilføje flere af de lyse frekvenser", med
//                         Abletons EQ Eight som reference): kanten starter paa frekvensen (10 % fjernet dér) og
//                         skaerer ned til 0 % under (High pass) / over (Low pass) den, lige saa dybt som ref + 25 %.
//                         Q bestemmer kun, hvor stejlt (passShape): Q 0.1 er en meget bloed skraaning, Q 0.71 een
//                         Butterworth-shelf, Q 10-18 en 4. ordens Butterworth-shelf. Den anden side bliver ved fuld
//                         drive (op til Q 0.71 helt; over det kommer kun resonansbulens skulder oveni).
//                         Over Q 0.71 en rund resonansbule paa frekvensen med en lang skulder paa den anden side
//                         som i EQ Eight (3.17; se koden nedenfor). Ved Q 18 loefter toppen driven med 1.5 * ref dB,
//                         og skulderen er 0.43 af toppen 2/3 oktav ude, 0.08 ved 1.7 oktav. Ingen sektion er en
//                         resonant shelf: dens toppe ligger ved fm * A^(-/+ 1/2), saa de flytter sig med styrken
//                         (maalt: op til 0.77 i relativ hoejde). 4. ordens Butterworth-shelfens to sektioner har
//                         hver halvdelen af dybden og samme fm, saa den er monoton ved al styrke.
//   Band pass:            hele baandet ned til 0 % (-ref) og Bell-loeft tilbage om frekvensen. ref = baseline (3.16,
//                         brugeren 31-10: en Bell, der loefter paa Band pass' frekvenser, skal kun aendre dem, ligesom
//                         en Bell, der saenker): de faste punkter laegges oveni, og Band pass' form er som indstillet.
//   Notch (3.12, brugeren 30-10: "Det skal være tilsvarende smalt relativt fra toppen af notchen til bunden, uanset
//          hvor højt drive (baseline) er skruet op"): Bell-daempninger, der tilsammen er lige saa dybe som hele
//          driven ved notchens frekvens (baseline + de andre punkter). Midt i er driven 0 %. 3.13 (brugeren 30-10:
//          "Notch skal kunne cutte hele vejen ned under -80db, men fortsat være lige smalt"): oveni et rigtigt
//          notch-filter (nul paa frekvensen = uendelig daempning), saa der slet ingen saturation er midt i.
//          Dets skuldre traekker kNotchKill / u^2 af driven (u = afstand i Bell-sektionernes bredder), naar
//          Q_nul = Q_bell / sqrt (kNotchKill * dB / 4.34). Det er ogsaa proportionalt med driven, saa formen
//          holder, og bunden af notchen bliver flad ved 0 % i u < kNotchKill^0.25 (0.21).
inline void designPoints (const BandParams& bp, const bool on[kPoints], double edgeLo, double edgeHi, double fs,
                          PointCoefs out[kPoints])
{
    double fc[kPoints];
    bool   fixed[kPoints];                                                           // taendte faste punkter
    const double driveDb = clampd (bp.drive, 0.0, 400.0) * kDbPerPct;
    for (int k = 0; k < kPoints; ++k)                                                // de faste foerst
    {
        const PointParams& pp = bp.pts[k];
        fc[k] = clampd (pp.freq, edgeLo, edgeHi);
        out[k] = PointCoefs();
        fixed[k] = on[k] && isFixedPoint (pp.type);
        if (pp.type == 0)                                                            // Bell: smal kerne (3.17)
        {
            out[k].n = 3 * kBellCopies;
            designBell (fc[k], pp.q, pp.gainDb, fs, out[k].c);
        }
        else if (isFixedPoint (pp.type))                                             // shelf: to ens med halv gain
        {
            out[k].n = 2;
            out[k].c[0] = out[k].c[1] = designPoint (pp.type, fc[k], pp.q, 0.5 * pp.gainDb, fs);
        }
        if (isFixedPoint (pp.type) && pp.gainDb > 0.0) out[k].nBoost = out[k].n;     // loeft: to ens halvdele
    }
    for (int k = 0; k < kPoints; ++k)                                                // de relative (notchen til sidst)
    {
        const PointParams& pp = bp.pts[k];
        if (isFixedPoint (pp.type) || pp.type == kPointNotch) continue;

        // Band pass foelger kun baseline (3.16): de faste punkter laegger deres dB oveni, op som ned, saa en Bell
        // paa Band pass' frekvenser kun aendrer sine egne frekvenser (med ref over hele baandet blev Band pass
        // smallere, naar en Bell loeftede). High / Low pass tager stadig de faste punkters loeft i stopbaandet ned.
        const double ref  = pp.type == 3 ? peakDriveDb (driveDb, out, fixed, fc, edgeLo, fc[k], fs)
                          : pp.type == 4 ? peakDriveDb (driveDb, out, fixed, fc, fc[k], edgeHi, fs)
                                         : driveDb;
        const double q = std::max (pp.q, 0.05);
        if (pp.type == kPointBandpass)
        {
            // 3.18: to stejle kanter (bandpassShape); Q aendrer kun bredden. Netto er det et snit overalt (toppen
            // er 0 dB), saa der er ingen loeft-sektioner. Toppens skalering k - 1 laegges paa sektion 0.
            const BandpassShape bs = bandpassShape (q);
            const double fa  = safeFreq (fc[k] / bs.r, fs), fb = safeFreq (fc[k] * bs.r, fs);
            const double cut = -ref * bs.k * (1.0 + kPassMargin) / (kBpOrder * kBpCopies);
            out[k].n = 2 * kBpOrder * kBpCopies;
            for (int c = 0; c < kBpCopies; ++c)
                for (int i = 0; i < kBpOrder; ++i)
                {
                    out[k].c[2 * (c * kBpOrder + i)]     = designLowShelf  (fa, kBpQ[i], cut, fs);
                    out[k].c[2 * (c * kBpOrder + i) + 1] = designHighShelf (fb, kBpQ[i], cut, fs);
                }
            const double a = std::pow (10.0, (bs.k - 1.0) * ref / 20.0);
            out[k].c[0].b0 *= a; out[k].c[0].b1 *= a; out[k].c[0].b2 *= a;
            continue;
        }

        // High / Low pass (3.15): kanten starter paa frekvensen og skaerer nedad (High pass) / opad (Low pass),
        // stejlere jo hoejere Q (passShape). Over 0.71 kommer resonansbulen (3.17) paa frekvensen oveni, og
        // kanten skaerer kResComp * bulens hoejde dybere, saa bulens nederste skulder ikke loefter stopbaandet.
        //   Bulen (brugeren 31-10: resonansen er "alt for højt-ragende og spids", "Design i stedet ressonansen på
        //   maks Q ud fra dette billede", EQ Eights High pass ved Q 18, toppen beskaaret): EQ Eights skulder paa
        //   den anden side falder som c * ln ((x^2 + a^2) / (x^2 + b^2)), x = W - 1/W (maalt paa billedet: 9.2 dB
        //   2/3 oktav ude, 2.3 dB ved 1.7 oktav, 0.4 dB ved 3 oktav). Det er en sum af Bell-former med bredder
        //   t = b..a (ln = integralet af 2t / (x^2 + t^2) dt), saa bulen er kRelSections svage Bells med lige
        //   meget gain og Q = 1 / t, t logaritmisk fordelt fra b til a. a = kResWide; b = a * sqrt (0.71 / Q), saa
        //   bulen er rund paa toppen og forsvinder ved Q 0.71. Toppen er kResHeight * ref ved Q 18 og falder
        //   som ln (Q / 0.71) (som et filters resonans i dB).
        const PassShape ps  = passShape (q);
        const double    fm  = safeFreq (pp.type == 3 ? fc[k] / ps.r : fc[k] * ps.r, fs);
        const double    rho = q > kShelfQMax ? kResHeight * std::log (q / kShelfQMax) / std::log (kPassQMax / kShelfQMax) : 0.0;
        const double    cut = -ref * (1.0 + kPassMargin + kResComp * rho) / (2 * kPassCopies);
        out[k].n = 2 * kPassCopies;
        for (int i = 0; i < kPassCopies; ++i)
        {
            out[k].c[2 * i]     = pp.type == 3 ? designLowShelf (fm, ps.qa, cut, fs) : designHighShelf (fm, ps.qa, cut, fs);
            out[k].c[2 * i + 1] = pp.type == 3 ? designLowShelf (fm, ps.qb, cut, fs) : designHighShelf (fm, ps.qb, cut, fs);
        }
        if (rho > 0.0)                                                               // resonansbulen paa kanten
        {
            // to ens halvdele (hver kRelSections Bells med halv gain), saa den er et loeft med 1 / S efter formeren
            const double res = std::min (rho * ref, std::max (kPointMaxDb - ref, 0.0)) / (2 * kRelSections);
            const double b   = kResWide * std::sqrt (kShelfQMax / q);
            out[k].n = 2 * kPassCopies + 2 * kRelSections;
            out[k].nBoost = 2 * kRelSections;
            for (int i = 0; i < kRelSections; ++i)
            {
                const double t = b * std::pow (kResWide / b, (i + 0.5) / kRelSections);
                out[k].c[2 * kPassCopies + i] = out[k].c[2 * kPassCopies + kRelSections + i] = designPeak (fc[k], 1.0 / t, res, fs);
            }
        }
    }

    for (int k = 0; k < kPoints; ++k)
    {
        if (bp.pts[k].type != kPointNotch) continue;
        out[k].n = 0;
        if (! on[k]) continue;
        double db = driveDb;                                                         // driven ved notchens frekvens
        for (int j = 0; j < kPoints; ++j)
            if (j != k && on[j] && bp.pts[j].type != kPointNotch)
                db += coefsDb (out[j], fc[k], fs);
        if (db < 0.01) continue;                                                     // ingen drive at fjerne: fladt
        const double qb = std::max (bp.pts[k].q, 0.05) * kNotchWidth;
        out[k].n = kRelSections + 1;
        for (int i = 0; i < kRelSections; ++i)
            out[k].c[i] = designPeak (fc[k], qb, -db / kRelSections, fs);
        out[k].c[kRelSections] = designNotch (fc[k], qb / std::sqrt (kNotchKill * db / 4.3429448190325), fs);
    }
}

// Baandkanterne, klemt saa de altid ligger i orden og under Nyquist. Nabobaand deler kant, naar kanten
// er magnetisk (lo = forrige hi), eller har et hul imellem sig, naar den er fri. Taendte baand overlapper
// aldrig. Et slukket baand (3.8) staar udenfor raekkefoelgen: det husker bare sin placering, og de
// taendte naboer maa gerne fylde dets plads ud.
struct BandEdges { double lo[kBands], hi[kBands]; };

inline BandEdges bandEdges (const EngineParams& p, double fs)
{
    BandEdges e;
    double prevHi = -1.0;                        // forrige taendte baands top (-1 = intet endnu)
    for (int b = 0; b < kBands; ++b)
    {
        const bool chained = p.band[b].on && prevHi > 0.0;
        e.lo[b] = chained ? clampd (p.band[b].lo, prevHi, fs * 0.46)
                          : clampd (p.band[b].lo, 10.0, b == 0 ? fs * 0.40 : fs * 0.46);
        e.hi[b] = clampd (p.band[b].hi, e.lo[b] * 1.02, fs * 0.46);
        if (p.band[b].on) prevHi = e.hi[b];
    }
    return e;
}

// ---------------------------------------------------------------------------
//  0 ms-tilstandene: baandets opdeling og Edge Q
// ---------------------------------------------------------------------------
// Brugerens krav (24-10):
//   Edge Q fra: en "firkant" - fuld drive over HELE baandets bredde (uanset drive), lodret mur ved kanten.
//   Edge Q til: fuld drive paa kanten, derfra et lineaert fade ud til punktet valgt med Edge Q;
//               ved Wall-ekstremen naesten en lodret mur med et lille bitte fade.
// Hver side af et baand har to crossovers (nederste side vist, oeverste er spejlvendt):
//   A = muren  anchorOct + D oktaver udenfor kanten
//   B = fade-graensen, paa kanten naar Edge Q er til, oven i A naar Edge Q er fra
// Det baandet saturerer:  [over B] * 1  +  [mellem A og B] * tail  +  [under A] * 0
//   tail = (1 - beta) + beta * K * HP1(kant) * svung   (beta = 0 fra, 1 til; svung og K: se kSwingA, 3.10)
// Alle stykker er i fase med den toerre sum (se Engine), saa vaegtene er rene styrker.
// Fadet er et 1. ordens filter (6 dB/okt = en ret linje i plottet). Det maa IKKE vaere stejlere: kun
// 1. orden holder fasen indenfor 90 grader, saa niveauloeftet aldrig kan traekke fra den toerre lyd
// (2. orden gav dyk paa 1-2 dB, op til ca. -6 dB ved 400 % drive). Stejlhed kommer kun fra crossovers.
// kTailK goer HP1 med knaek paa kanten til fuld styrke paa kanten (B's to halvdele er 0.5 + 0.5 der).
//
// Murenes stejlhed: 24 eller 48 dB/okt. Stejlere mure (192) gav en pitch-aendring paa over en halv tone
// naar baandet blev flyttet (Doppler: en flyttet crossover drejer fasen over tid). Maalt 24-10, kant
// 150 -> 250 Hz paa 0.5 s, tone 200 Hz, 100 % drive: 192 -> 115 cent, 96 -> 56, 48 -> 28, 24 -> 9 cent.
struct ZlSpec
{
    int    wallN6, tailN6;           // murens og fade-graensens stejlhed / 6
    double anchorOct;                // murens afstand fra kanten: 96 % drive paa selve kanten
};

// Murens afstand: LR-vaegten er 1 / (1 + r^n6), og 0.97 naas ved r^n6 = 1/32
// Doppler Saturator 1.2 (brugeren 2026-11-05: "yderligere en 0 - ms, som er endnu stejlere og pitcher lyden endnu
// mere"): 96 dB/okt. Multiband Saturator bruger kun 24 og 48.
inline int zlN6 (int phaseMode)
{
    return phaseMode == kPhaseZl24 ? 4 : phaseMode == kPhaseZl96 ? 16 : 8;
}

inline ZlSpec zlSpec (int phaseMode)
{
    const int n6 = zlN6 (phaseMode);
    return { n6, n6, 5.0 / n6 };
}

constexpr int    kSoloN6    = 8;       // solo-filtrene i 0 ms-tilstandene: 48 dB/okt
constexpr double kSoftOct   = 5.5;     // hvor langt ude punktet ligger ved Edge Q = 0 %
constexpr double kTailK     = 1.6457513110645907;   // (-0.5 + sqrt (0.25 + 1.5)) / 0.5

// Svunget udenfor kanten (brugeren 30-10, 3.10): "det lille, subtile svung, der kommer paa kurven udenfor
// baandet, naar Edge Q er slaaet til samtidigt med et frequency point" skal ogsaa vaere der uden punkter.
// Fadet er derfor ikke en ret linje: stejlere lige udenfor kanten, fladere ud mod punktet. Hoejden i plottet
// (andel af baandets hoejde) ved d = 0..1 fra kanten til Edge Q-punktet:
//   s(d) = (1 - d) * (1 - kSwingA * d + kSwingB * d^2)
// Konstanterne er maalt paa brugerens skaermbillede (Bell naer kanten): halvvejs 0.36 i stedet for 0.5,
// startende 1.6x saa stejlt som den rette linje og sluttende 0.5x. Det er hoejst 4.2 dB under linjen.
constexpr double kSwingA = 0.62, kSwingB = 0.13;

inline double swingHeight (double d)
{
    d = clampd (d, 0.0, 1.0);
    return (1.0 - d) * (1.0 - kSwingA * d + kSwingB * d * d);
}

// 0 ms: svunget er en bred, svag Bell-daempning paa fade-delene: -4.2 dB midt i fadet (d = 0.5), bredde
// (mellem halv dybde) 0.5 af fadet. Fadet uden svung buer op (1. orden), saa resultatet ved Edge Q 0 %
// ligger taet paa s(d): halvvejs 0.35 (24 dB) og 0.42 (48 dB) mod 0.36 i lineaer fase (foer 0.46 / 0.53).
// Fasen er hoejst nogle faa grader, saa den kan ikke traekke fra den toerre lyd. Korte fades (Edge Q naer
// Wall) faar mindre svung, og under 1 oktav intet: der er ikke plads, og Bell'en ville lave en bule ved kanten.
constexpr double kSwingBellDb  = -4.2;
constexpr double kSwingBellPos = 0.5;
constexpr double kSwingBellBw  = 0.5;

inline double swingBellScale (double spanOct) { return clampd ((spanOct - 1.0) / 1.5, 0.0, 1.0); }

// Fadets forstaerkning, saa vaegten er praecis fuld paa kanten: crossoveren dér giver 0.5 + 0.5 * K * z,
// hvor z er fade-filtrenes (kompleks) respons paa kanten. |1 + K z| = 2. Uden svung er K = kTailK.
inline double tailGainFor (std::complex<double> z)
{
    const double re = z.real(), m2 = std::norm (z);
    return (-re + std::sqrt (re * re + 3.0 * m2)) / std::max (m2, 1.0e-12);
}

// Hvor hurtigt mur, fade-graense og beta maa glide - i TID, ikke pr. blok, saa det ikke afhaenger af
// vaertens blokstoerrelse (LR192-mure blev ustabile ved 0.25 oktav pr. blok paa 64 samples).
constexpr double kGlideOctPerSec  = 6.0;    // hele Edge Q-slideren (5.5 oktav) paa ca. 1 sekund
constexpr double kGlideBetaPerSec = 8.0;    // Edge Q til/fra paa ca. 0.13 sekund

// Opdelingens form for et baand: afstande (oktaver udenfor kanten) til mur og fade-graense, og beta
struct FadeShape { double wallOct = 0.0, tailOct = 0.0, beta = 0.0; };

inline double stepToward (double v, double target, double maxStep)
{
    return v + clampd (target - v, -maxStep, maxStep);
}

inline FadeShape fadeShape (const BandParams& bp, const ZlSpec& sp)
{
    FadeShape s;
    s.wallOct = s.tailOct = sp.anchorOct;
    if (bp.edgeOn)
    {
        const double q = clampd (bp.edgeQ * 0.01, 0.0, 1.0);
        s.wallOct = sp.anchorOct + kSoftOct * (1.0 - q);      // punktet flytter sig lineaert i oktaver
        s.tailOct = 0.0;
        s.beta    = 1.0;
    }
    return s;
}

// dt = tid (sekunder) siden sidste skridt
inline FadeShape stepShape (const FadeShape& cur, const FadeShape& tgt, double dt)
{
    FadeShape s;
    s.wallOct = stepToward (cur.wallOct, tgt.wallOct, kGlideOctPerSec * dt);
    s.tailOct = stepToward (cur.tailOct, tgt.tailOct, kGlideOctPerSec * dt);
    s.beta    = stepToward (cur.beta,    tgt.beta,    kGlideBetaPerSec * dt);
    return s;
}

// end[4b + 0..3] = baand b's crossovers fra bund til top: A (mur) nederst, B nederst, B oeverst, A oeverst
constexpr int kEnds = 4 * kBands;
struct Layout { BandEdges edge; double end[kEnds]; };

inline Layout makeLayout (const EngineParams& p, double fs, const FadeShape shapes[kBands])
{
    Layout L;
    L.edge = bandEdges (p, fs);
    for (int b = 0; b < kBands; ++b)
    {
        const FadeShape& s = shapes[b];
        const double lo = L.edge.lo[b], hi = L.edge.hi[b];
        double* e = &L.end[4 * b];
        e[0] = std::max (lo * std::exp2 (-s.wallOct), 5.0);
        e[1] = clampd (lo * std::exp2 (-std::min (s.tailOct, s.wallOct)), e[0], lo);
        e[3] = std::min (hi * std::exp2 (s.wallOct), fs * 0.47);
        e[2] = clampd (hi * std::exp2 (std::min (s.tailOct, s.wallOct)), hi, e[3]);
    }
    return L;
}

inline Layout makeLayout (const EngineParams& p, double fs)
{
    const ZlSpec sp = zlSpec (p.phaseMode);
    FadeShape shapes[kBands];
    for (int b = 0; b < kBands; ++b) shapes[b] = fadeShape (p.band[b], sp);
    return makeLayout (p, fs, shapes);
}

inline XoverCoefs designEnd (int k, const Layout& L, double fs, const ZlSpec& sp)
{
    const int side = k % 4;
    return designXover ((side == 0 || side == 3) ? sp.wallN6 : sp.tailN6, L.end[k], fs);
}

// Fadets 1. ordens filtre (knaek paa kanterne), svungets Bell-filtre, forstaerkningen og beta
struct FadeCoefs { Coef lo, hi, swLo, swHi; double kLo = kTailK, kHi = kTailK, beta = 0.0; };

inline FadeCoefs designFade (const FadeShape& s, double edgeLo, double edgeHi, double fs)
{
    FadeCoefs fc;
    const double hi = std::min (edgeHi, fs * 0.45);
    fc.lo   = designHighpass1 (edgeLo, fs);
    fc.hi   = designLowpass1 (hi, fs);
    fc.beta = s.beta;
    // Svunget strakker sig fra kanten ud til muren (= Edge Q-punktet)
    const double span = std::max (s.wallOct, 0.05);
    const double q    = 1.0 / (2.0 * std::sinh (0.5 * std::log (2.0) * kSwingBellBw * span));
    const double db   = kSwingBellDb * swingBellScale (span);
    fc.swLo = designPeak (clampd (edgeLo * std::exp2 (-kSwingBellPos * span), 5.0, fs * 0.4), q, db, fs);
    fc.swHi = designPeak (clampd (hi * std::exp2 (kSwingBellPos * span), 5.0, fs * 0.4), q, db, fs);
    fc.kLo  = tailGainFor (coefResponse (fc.lo, edgeLo, fs) * coefResponse (fc.swLo, edgeLo, fs));
    fc.kHi  = tailGainFor (coefResponse (fc.hi, hi, fs) * coefResponse (fc.swHi, hi, fs));
    return fc;
}

// Kompleks respons af en biquad ved en frekvens hvor e^{-jw} (z1) og e^{-2jw} (z2) er regnet paa forhaand
inline std::complex<double> coefResponseZ (const Coef& c, std::complex<double> z1, std::complex<double> z2)
{
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}

inline std::complex<double> seqResponseZ (const SeqCoefs& s, std::complex<double> z1, std::complex<double> z2)
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < s.count; ++i) h *= coefResponseZ (s.c[i], z1, z2);
    return h;
}

// ---------------------------------------------------------------------------
//  Lineaer fase: baandets vaegt (nul fase), som FIR-filteret designes efter
// ---------------------------------------------------------------------------
// En lodret kant er umulig: for at skelne to frekvenser df Hz fra hinanden skal filteret "lytte" ca. 1/df
// sekunder (usikkerhedsprincippet). Overgangen er derfor fast i Hz, og paa den logaritmiske akse ser den
// bredere ud jo dybere kanten ligger. Skarpheden (parameteren "linsharp") vaelger FIR-laengden:
//   Normal  ca. 0.37 s filter, 0.20 s forsinkelse, overgang 6.0 Hz (44.1 kHz)
//   Skarp   2x laengde,        0.38 s,              3.0 Hz
//   Ekstrem 4x laengde,        0.75 s,              1.5 Hz
// Normal-laengden er den potens af 2, der ligger naermest 0.37 s, saa forsinkelsen er den samme ved 44.1 og
// 48 kHz. Kaiser-vindue med 40 dB daempning (foer 60 dB: overgangen er nu 38 % smallere uden mere
// forsinkelse; -40 dB = 1 % af lyden lige udenfor kanten naar stadig saturationen, under plottets -30 dB).
// Overgangen fra fuld vaegt til -40 dB er kLinTransK * fs / L Hz bred. Muren ved en kant ligger HELT udenfor
// kanten: fuld vaegt paa kanten og nul transHz udenfor. Normal: 150 Hz -> 0.06 okt, 40 Hz -> ca. 0.2 okt.
constexpr double kLinTransK      = 2.229;  // Kaiser, 40 dB: (40 - 8) / (2.285 * 2 pi)
constexpr double kLinKaiserBeta  = 3.395;  // 0.5842 * (40 - 21)^0.4 + 0.07886 * (40 - 21)
constexpr double kLinMinFadeOct  = 0.1;    // Edge Q 100 %: et lille bitte fade foer muren
constexpr int    kLinPartitions  = 32;     // Normal-FIR'en deles i 32 blokke (partitioneret FFT-foldning)
constexpr int    kLinSharpMax    = 2;      // 0 = normal, 1 = skarp (2x), 2 = ekstrem (4x)

inline int linFirLength (double fsBase, int sharp = 0)
{
    int L = 16384;
    while (L < 524288 && L * 1.41421356 < fsBase * 0.37) L *= 2;   // naermeste potens af 2 (i oktaver)
    return L << std::clamp (sharp, 0, kLinSharpMax);
}

// Blokstoerrelsen er den samme for alle skarpheder (en laengere FIR = flere blokke)
inline int linBlockSize (double fsBase) { return linFirLength (fsBase) / kLinPartitions; }
inline int linLatency (double fsBase, int sharp) { return linFirLength (fsBase, sharp) / 2 - 1 + linBlockSize (fsBase); }
inline double linTransHz (double fsBase, int sharp = 0) { return kLinTransK * fsBase / (linFirLength (fsBase, sharp) - 1); }

// Kaiser-vinduets spektrum (hovedloben) med x = pi * nu * T (T = FIR'ens laengde i sekunder): under beta
// sinh (sqrt (beta^2 - x^2)) / sqrt (...), derover sin (...) / ..., nul ved x0 = sqrt (beta^2 + pi^2).
// FIR'ens frekvensgang er maalet udglattet med denne kerne. Sidelobene (under -38 dB) er udeladt.
inline double kaiserLobe (double x)
{
    const double d = kLinKaiserBeta * kLinKaiserBeta - x * x;
    if (d > 1e-12)  { const double s = std::sqrt (d);  return std::sinh (s) / s; }
    if (d < -1e-12) { const double s = std::sqrt (-d); return std::sin (s) / s; }
    return 1.0;
}

// Hovedlobens halve bredde i Hz (x0 omregnet: nu = x / (pi T), og T = kLinTransK / transHz)
inline double linLobeHalfHz (double transHz)
{
    return std::sqrt (kLinKaiserBeta * kLinKaiserBeta + kPi * kPi) / (kPi * kLinTransK) * transHz;
}

// Andelen af hovedlobens areal fra -x0 til u * x0 (u i [-1, 1]). Tabel + lineaer interpolation.
inline double kaiserCdf (double u)
{
    static const std::array<double, 257> tab = []
    {
        std::array<double, 257> t {};
        const double x0 = std::sqrt (kLinKaiserBeta * kLinKaiserBeta + kPi * kPi);
        double acc = 0.0;
        for (int i = 0; i < 256; ++i)
        {
            for (int j = 0; j < 64; ++j) acc += kaiserLobe ((-1.0 + (i + (j + 0.5) / 64.0) / 128.0) * x0);
            t[(size_t) i + 1] = acc;
        }
        for (auto& v : t) v /= acc;
        return t;
    }();
    const double pos = (std::clamp (u, -1.0, 1.0) + 1.0) * 128.0;
    const int i = std::min (255, (int) pos);
    return tab[(size_t) i] + (pos - i) * (tab[(size_t) i + 1] - tab[(size_t) i]);
}

struct LinMask
{
    double lo = 20, hi = 20000;      // kanterne
    double fadeOct = 0.0;            // 0 = Edge Q fra (lodret mur lige udenfor kanten), ellers fadets laengde
    double transHz = 6.0;            // FIR'ens overgangsbredde (se kLinTransK)
    double nyq = 22050.0;
    int    sharp = 0;                // skarphed = FIR-laengde (samme for alle filtre i et saet)

    bool operator== (const LinMask& o) const
    {
        return lo == o.lo && hi == o.hi && fadeOct == o.fadeOct && transHz == o.transHz && nyq == o.nyq && sharp == o.sharp;
    }

    // Maalet FIR'en designes efter. Kaiser-vinduet udglatter maalet over transHz (symmetrisk), saa maalet
    // er 1 helt ud til en halv overgang udenfor kanterne - dermed er vaegten fuld PAA kanten.
    // Edge Q fra: en firkant, nul udenfor (vaegten falder fra 1 paa kanten til nul transHz udenfor).
    // Edge Q til: derfra falder vaegten i dB til -30 dB ved punktet med svunget (swingHeight: hoejden i
    // plottet er s(d)), og er nul derefter. (Foer 3.10 en ret linje i dB.)
    double ideal (double f) const
    {
        const double loF = lo - 0.5 * transHz, hiF = hi + 0.5 * transHz;
        if (f >= loF && f <= hiF) return 1.0;
        const bool below = f < loF;
        if (! below && hiF >= nyq) return 1.0;                          // oeverste baand helt op til Nyquist
        if (fadeOct <= 0.0 || f <= 0.0) return 0.0;
        const double d = below ? std::log2 (loF / f) : std::log2 (f / hiF);
        return d < fadeOct ? std::pow (10.0, -(kDisplayRangeDb / 20.0) * (1.0 - swingHeight (d / fadeOct))) : 0.0;
    }

    // Maalet som designet ser det paa et frekvensgitter med afstand df: en bin der skaerer et af de lodrette
    // trin (loF / hiF) faar den andel, der ligger paa hver side. Ellers hopper trinnet til naermeste
    // gitterpunkt, og kanten flytter op til en halv bin (0.7 Hz ved normal skarphed = 11 % af overgangen).
    double binValue (double f, double df) const
    {
        const double a = f - 0.5 * df, b = f + 0.5 * df;
        const double loF = lo - 0.5 * transHz, hiF = hi + 0.5 * transHz;
        if (a < loF && loF < b)
        {
            const double in = (b - loF) / df;
            return in * ideal (0.5 * (loF + b)) + (1.0 - in) * ideal (0.5 * (a + loF));
        }
        if (a < hiF && hiF < b)
        {
            const double in = (hiF - a) / df;
            return in * ideal (0.5 * (a + hiF)) + (1.0 - in) * ideal (0.5 * (hiF + b));
        }
        return ideal (f);
    }

    // Som det tegnes = hvad FIR'en giver: maalet udglattet med Kaiser-vinduets hovedlobe (se kaiserLobe).
    // Delen indenfor trinnene vejer 1 og regnes eksakt (tabellen), fadet udenfor er glat og tages i 8 stykker.
    double drawn (double f) const
    {
        if (f >= lo + 0.5 * transHz && f <= hi - 0.5 * transHz) return 1.0;
        const double half = linLobeHalfHz (transHz);
        const double loF = lo - 0.5 * transHz;
        const double hiF = hi + 0.5 * transHz >= nyq ? 1e30 : hi + 0.5 * transHz;   // oeverste baand: op til Nyquist
        const double ua = std::clamp ((loF - f) / half, -1.0, 1.0), ub = std::clamp ((hiF - f) / half, -1.0, 1.0);
        double sum = std::max (0.0, kaiserCdf (ub) - kaiserCdf (ua));
        if (fadeOct > 0.0)
        {
            auto outside = [&] (double u0, double u1)
            {
                double s = 0.0;
                for (int j = 0; j < 8 && u1 > u0; ++j)
                {
                    const double a = u0 + (u1 - u0) * j / 8.0, b = u0 + (u1 - u0) * (j + 1) / 8.0;
                    s += (kaiserCdf (b) - kaiserCdf (a)) * ideal (f + 0.5 * (a + b) * half);
                }
                return s;
            };
            sum += outside (-1.0, ua) + outside (ub, 1.0);
        }
        return sum;
    }
};

// Baand b's vaegt i lineaer fase. solo = kun lyden mellem kanterne (uden Edge Q-fadet)
inline LinMask linMaskFor (const EngineParams& p, int b, bool solo)
{
    const BandEdges edge = bandEdges (p, p.baseRate);
    LinMask m;
    m.lo = edge.lo[b];
    m.hi = edge.hi[b];
    m.sharp = std::clamp (p.linSharp, 0, kLinSharpMax);
    m.transHz = linTransHz (p.baseRate, m.sharp);
    m.nyq = 0.5 * p.baseRate;
    const BandParams& bp = p.band[b];
    if (! solo && bp.edgeOn)
        m.fadeOct = kLinMinFadeOct + kSoftOct * (1.0 - clampd (bp.edgeQ * 0.01, 0.0, 1.0));
    return m;
}

// ---------------------------------------------------------------------------
//  Visningsmodel (editoren): kurver
// ---------------------------------------------------------------------------
struct CurveModel
{
    bool      linear = false;
    LinMask   lin[kBands];               // lineaer fase: baandenes vaegt
    XoverCoefs xo[kEnds];                // 0 ms: hvert baands fire crossovers (se Layout)
    PointCoefs pc[kBands][kPoints];
    bool      on[kBands][kPoints] = {};
    double    drive[kBands] = {};
    FadeCoefs fade[kBands];
    double    fs = 44100;

    // Hvor stor en del af hvert baands drive der virker ved f: styrken af det signal baandet saturerer
    // (samme opdeling som i lyden; de faelles allpass-faser har styrke 1 og kan udelades).
    // 0 ms: crossovers a, b, c, d fra bund til top. Murene (a, d) deles FOERST, saa intet slipper forbi dem.
    // Med LR-crossovers er HP = AP - LP:
    //   fade under = HPa*LPd*LPb*APc*tailLo,  omraade = HPa*LPd*HPb*LPc,  fade over = HPa*LPd*HPb*HPc*tailHi
    void edgeWeights (double f, double w[kBands]) const
    {
        if (linear)
        {
            for (int b = 0; b < kBands; ++b) w[b] = lin[b].drawn (f);
            return;
        }
        const double ww = 2.0 * kPi * f / fs;
        const std::complex<double> z1 (std::cos (ww), -std::sin (ww)), z2 = z1 * z1;
        for (int b = 0; b < kBands; ++b)
        {
            std::complex<double> lp[4], ap[4], hp[4];
            for (int i = 0; i < 4; ++i)
            {
                lp[i] = seqResponseZ (xo[4 * b + i].lp, z1, z2);
                ap[i] = seqResponseZ (xo[4 * b + i].ap, z1, z2);
                hp[i] = ap[i] - lp[i];
            }
            const FadeCoefs& fc = fade[b];
            const double bt = fc.beta;
            const std::complex<double> tlo = (1.0 - bt) + bt * fc.kLo * coefResponseZ (fc.lo, z1, z2) * coefResponseZ (fc.swLo, z1, z2);
            const std::complex<double> thi = (1.0 - bt) + bt * fc.kHi * coefResponseZ (fc.hi, z1, z2) * coefResponseZ (fc.swHi, z1, z2);
            w[b] = std::abs (hp[0] * lp[3] * (lp[1] * ap[2] * tlo + hp[1] * lp[2] + hp[1] * hp[2] * thi));
        }
    }

    double edgeWeight (int b, double f) const { double w[kBands]; edgeWeights (f, w); return w[b]; }

    double ptsDb (int b, double f) const
    {
        std::complex<double> h (1.0, 0.0);
        for (int k = 0; k < kPoints; ++k)
            if (on[b][k])
                for (int i = 0; i < pc[b][k].n; ++i) h *= coefResponse (pc[b][k].c[i], f, fs);
        const double m = std::abs (h);
        return 20.0 * std::log10 (m < 1.0e-9 ? 1.0e-9 : m);
    }
    // Baandets egen effektive drive ved f (baseline + punkter). 1 dB = 11.07 % drive.
    double bandDrivePct (int b, double f) const { return clampd (drive[b] + ptsDb (b, f) / kDbPerPct, 0.0, 400.0); }
    // Hvert baands BIDRAG ved f, som det tegnes. Edge Q-vaegten vises i dB over kDisplayRangeDb, og som
    // en andel af kurvens HOEJDE paa drive-aksen (kDriveAxisExp), saa formen er den samme uanset drive:
    //   hoejde = baandets hoejde * (1 + dB (vaegt) / kDisplayRangeDb)
    // Et 1. ordens fade (6 dB/okt) bliver derved en ret linje. (Tidligere forsoeg: vaegt * drive overdrev
    // halerne kraftigt, og vaegt som dB af drive goer baandet smallere ved lav drive - begge forkerte.)
    static double weightedPct (double drivePct, double w)
    {
        const double s = clampd (1.0 + 20.0 * std::log10 (std::max (w, 1.0e-9)) / kDisplayRangeDb, 0.0, 1.0);
        return drivePct * std::pow (s, 1.0 / kDriveAxisExp);
    }
    void contribPct (double f, double out[kBands]) const
    {
        double w[kBands];
        edgeWeights (f, w);
        for (int b = 0; b < kBands; ++b) out[b] = weightedPct (bandDrivePct (b, f), w[b]);
    }
    double bandContribPct (int b, double f) const { double o[kBands]; contribPct (f, o); return o[b]; }
};

// fs = den rate saturationen (og punkterne) koerer paa: 4x baseRate naar oversampling er til
inline CurveModel makeCurveModel (const EngineParams& p, double fs)
{
    CurveModel m;
    m.fs = fs;
    BandEdges edge;
    if (p.phaseMode == kPhaseLinear)
    {
        m.linear = true;
        edge = bandEdges (p, p.baseRate);
        for (int b = 0; b < kBands; ++b) m.lin[b] = linMaskFor (p, b, false);
    }
    else
    {
        const ZlSpec sp = zlSpec (p.phaseMode);
        const Layout L = makeLayout (p, fs);
        for (int k = 0; k < kEnds; ++k) m.xo[k] = designEnd (k, L, fs, sp);
        for (int b = 0; b < kBands; ++b) m.fade[b] = designFade (fadeShape (p.band[b], sp), L.edge.lo[b], L.edge.hi[b], fs);
        edge = L.edge;
    }
    for (int b = 0; b < kBands; ++b)
    {
        m.drive[b] = p.band[b].on ? p.band[b].drive : 0.0;          // et slukket baand saturerer intet
        for (int k = 0; k < kPoints; ++k)
            m.on[b][k] = p.band[b].pts[k].on && p.band[b].on;
        designPoints (p.band[b], m.on[b], edge.lo[b], edge.hi[b], fs, m.pc[b]);   // samme filtre som lyden
    }
    return m;
}

// ---------------------------------------------------------------------------
//  Formere
// ---------------------------------------------------------------------------
// tanh via expm1: samme resultat som std::tanh (fejl under 1e-15), men 3-5x hurtigere med MSVC.
// std::tanh var den dyreste del af hele motoren (op til 90 ns pr. kald, flere kald pr. sample).
inline double fastTanh (double x)
{
    if (x > 20.0)  return 1.0;
    if (x < -20.0) return -1.0;
    const double e = std::expm1 (2.0 * x);
    return e / (e + 2.0);
}

inline double warmShape (double u)
{
    // Asymmetrisk bloed kurve (lige harmoniske, som et roer) blandet med symmetrisk tanh (let 3. harmoniske)
    const double b    = 0.22;
    const double tb   = 0.21651806149302883;                     // tanh (0.22)
    const double asym = (fastTanh (u + b) - tb) / (1.0 - tb * tb);
    return 0.6 * asym + 0.4 * fastTanh (u);
}

inline double shapeFn (int type, double u)
{
    switch (type)
    {
        case 1:  return clampd (u, -1.0, 1.0);                                   // Hard clip
        case 2:  return (fastTanh (u + 0.3) - 0.2913126124515909) / 0.9151369;   // Tube (enhedsstigning i 0)
        case 3:  return std::sin (0.5 * kPi * u) / (0.5 * kPi);                  // Fold
        case 4:  return warmShape (u);                                           // Warm Analog
        default: return fastTanh (u);                                            // Soft
    }
}

// ---------------------------------------------------------------------------
//  Glidende parametre
// ---------------------------------------------------------------------------
struct Ramp
{
    double v = 0, t = 0, a = 1;
    void setTime (double fs, double ms) { a = 1.0 - std::exp (-1.0 / (0.001 * ms * fs)); }
    void snap (double x) { v = t = x; }
    void target (double x) { t = x; }
    inline double next()
    {
        v += a * (t - v);
        if (std::abs (t - v) < 1.0e-9) v = t;
        return v;
    }
};

// ---------------------------------------------------------------------------
//  Stereo-filtre: venstre og hoejre regnes samtidig med SSE2 (lane 0 = venstre, lane 1 = hoejre)
// ---------------------------------------------------------------------------
struct Biquad2
{
    __m128d b0 = _mm_set1_pd (1.0), b1 = _mm_setzero_pd(), b2 = _mm_setzero_pd(), a1 = _mm_setzero_pd(), a2 = _mm_setzero_pd();
    __m128d z1 = _mm_setzero_pd(), z2 = _mm_setzero_pd();

    void set (const Coef& c)
    {
        b0 = _mm_set1_pd (c.b0); b1 = _mm_set1_pd (c.b1); b2 = _mm_set1_pd (c.b2);
        a1 = _mm_set1_pd (c.a1); a2 = _mm_set1_pd (c.a2);
    }
    void reset() { z1 = z2 = _mm_setzero_pd(); }
    inline __m128d process (__m128d x)
    {
        const __m128d y = _mm_add_pd (_mm_mul_pd (b0, x), z1);
        z1 = _mm_add_pd (_mm_sub_pd (_mm_mul_pd (b1, x), _mm_mul_pd (a1, y)), z2);
        z2 = _mm_sub_pd (_mm_mul_pd (b2, x), _mm_mul_pd (a2, y));
        return y;
    }
};

// En kaede af biquads (fx en crossover-halvdel eller en allpass)
struct Chain2
{
    Biquad2 s[SeqCoefs::kMax];
    int n = 0;
    void set (const SeqCoefs& c) { n = c.count; for (int i = 0; i < n; ++i) s[i].set (c.c[i]); }
    void reset() { for (auto& q : s) q.reset(); }
    inline __m128d process (__m128d x) { for (int i = 0; i < n; ++i) x = s[i].process (x); return x; }
};

// Allpassene fra flere crossovers efter hinanden
struct ApChain2
{
    Chain2 ap[8];
    int n = 0;
    void set (const int* ends, int count, const XoverCoefs xc[kEnds])
    {
        n = count;
        for (int i = 0; i < n; ++i) ap[i].set (xc[ends[i]].ap);
    }
    void reset() { for (auto& a : ap) a.reset(); }
    inline __m128d process (__m128d x) { for (int i = 0; i < n; ++i) x = ap[i].process (x); return x; }
};

inline __m128d mul (__m128d a, double s) { return _mm_mul_pd (a, _mm_set1_pd (s)); }

// ---------------------------------------------------------------------------
//  Et baands saturation (faelles for alle tilstande): punkter, formeren og deltaen
// ---------------------------------------------------------------------------
// KUN forskellen (saturationen af u minus u selv) laegges oven paa den toerre lyd. Da u er i fase med den,
// kan baade niveauloeft og kompression tages herfra uden kamfilter-huller, og drive virker praecis saa
// meget som kurven viser - ogsaa udenfor kanten.
//
// Punkternes loeft (3.17): et punkts loeft B (Bell / shelves med positiv gain, High / Low pass' resonansbule) er
// to ens halvdele S S. Raekkefoelgen er: snittene C (resten af punkterne) -> x1, loeftene B, formeren, 1 / S, og
// deltaen er (det) - x1. Ved svag lyd er det (sqrt (g B) - 1) x1: et punkt, der loefter driven til 400 %, loefter
// ogsaa lyden lige saa meget som 400 % baseline (halvdelen af driven i dB), og ved kraftig lyd er det begraenset.
// Foer 3.17 var deltaen form (g B C u) / sqrt g - B C u: ved kraftig lyd trak den det forstaerkede rene signal fra, saa
// en Bell paa +24 dB ved 0 % baseline gav tonen +21 dB (0.3 i amplitude ind, 3.45 ud), og +36 dB gav +35 dB.
struct BandSat
{
    Biquad2 pt[kPoints][kPointSections];       // punkternes biquads (designPoints): snittene foerst, loeftene sidst
    Biquad2 post[kPoints][kPointSections / 2]; // 1 / S efter formeren (3.17)
    int     ptN[kPoints] = { 0, 0, 0 };        // antal biquads i brug pr. punkt (0 = slukket / fladt)
    int     cutN[kPoints] = { 0, 0, 0 };       // heraf snit (resten er loeft)
    int     postN[kPoints] = { 0, 0, 0 };      // antal 1 / S-biquads
    bool    ptOn[kPoints] = { false, false, false };
    Ramp    g, mk, mix;
    int     type = 0;
    bool    active = false;
    __m128d dcX = _mm_setzero_pd(), dcY = _mm_setzero_pd(), warmZ = _mm_setzero_pd();
    double  warmA = 0;

    void resetSat()
    {
        for (auto& p : pt) for (auto& f : p) f.reset();
        for (auto& p : post) for (auto& f : p) f.reset();
        dcX = dcY = warmZ = _mm_setzero_pd();
    }

    void prepare (double fs)
    {
        g.setTime (fs, 10.0);  g.snap (1.0);
        mk.setTime (fs, 10.0); mk.snap (1.0);
        mix.setTime (fs, 10.0); mix.snap (0.0);
        active = false;
        resetSat();
    }

    // Returnerer om baandet saturerer. edgeLo/edgeHi: punkterne holdes indenfor baandets kanter.
    bool setParams (const BandParams& bp, double edgeLo, double edgeHi, double fs, bool bypass)
    {
        double maxPosDb = 0.0;
        PointCoefs pc[kPoints];
        for (int k = 0; k < kPoints; ++k) ptOn[k] = bp.pts[k].on;
        designPoints (bp, ptOn, edgeLo, edgeHi, fs, pc);                  // samme filtre som tegningen (CurveModel)
        for (int k = 0; k < kPoints; ++k)
        {
            const int prevN = ptN[k], prevPost = postN[k];
            ptN[k]   = ptOn[k] ? pc[k].n : 0;
            cutN[k]  = ptN[k] - (ptOn[k] ? pc[k].nBoost : 0);
            postN[k] = (ptN[k] - cutN[k]) / 2;
            for (int i = 0; i < ptN[k]; ++i) pt[k][i].set (pc[k].c[i]);
            for (int i = prevN; i < ptN[k]; ++i) pt[k][i].reset();      // sektioner, der lige er taget i brug
            for (int i = 0; i < postN[k]; ++i) post[k][i].set (inverseCoef (pc[k].c[cutN[k] + i]));
            for (int i = prevPost; i < postN[k]; ++i) post[k][i].reset();
            if (bp.pts[k].on && isFixedPoint (bp.pts[k].type) && bp.pts[k].gainDb > 0.1)   // ogsaa ved 0 % baseline
                maxPosDb = std::max (maxPosDb, bp.pts[k].gainDb);
        }

        const double gain     = driveToGain (clampd (bp.drive, 0.0, 400.0));
        const double effDrive = bp.drive + maxPosDb / kDbPerPct;       // "hvor meget drive er der i alt"
        const bool   sat      = ! bypass && bp.on && effDrive > 0.01 && bp.mix > 0.001;   // slukket baand fader ud

        if (sat && ! active)
        {
            g.snap (gain); mk.snap (1.0 / std::sqrt (gain));
            mix.snap (0.0);
            resetSat();
        }
        g.target (gain);
        mk.target (1.0 / std::sqrt (gain));
        mix.target (sat ? clampd (bp.mix, 0.0, 1.0) * clampd (effDrive / 2.0, 0.0, 1.0) : 0.0);
        type   = bp.type;
        warmA  = std::exp (-2.0 * kPi * 9000.0 / fs);
        active = sat || mix.v > 1.0e-5;
        return sat;
    }

    // u = baandets signal (i fase med den toerre lyd). Returnerer deltaen efter DC-blokering.
    inline __m128d process (__m128d u, double dcR)
    {
        const double gg = g.next(), m = mk.next(), mx = mix.next();
        for (int k = 0; k < kPoints; ++k)                                   // snittene -> x1 (referencen)
            for (int i = 0; i < cutN[k]; ++i) u = pt[k][i].process (u);
        __m128d x = u;
        for (int k = 0; k < kPoints; ++k)                                   // loeftene B
            for (int i = cutN[k]; i < ptN[k]; ++i) x = pt[k][i].process (x);

        alignas (16) double uu[2], ww[2];
        _mm_store_pd (uu, x);
        ww[0] = shapeFn (type, gg * uu[0]) * m;
        ww[1] = shapeFn (type, gg * uu[1]) * m;
        __m128d wet = _mm_load_pd (ww);
        if (type == 4)
        {
            warmZ = _mm_add_pd (mul (wet, 1.0 - warmA), mul (warmZ, warmA));
            wet = warmZ;
        }
        for (int k = 0; k < kPoints; ++k)                                   // 1 / S (halvdelen af loeftet tilbage)
            for (int i = 0; i < postN[k]; ++i) wet = post[k][i].process (wet);

        const __m128d d = mul (_mm_sub_pd (wet, u), mx);
        const __m128d y = _mm_add_pd (_mm_sub_pd (d, dcX), mul (dcY, dcR));
        dcX = d;
        dcY = y;
        return y;
    }
};

// ---------------------------------------------------------------------------
//  Solo af et frekvenspunkt (3.8): kun de frekvenser, punktet virker paa
// ---------------------------------------------------------------------------
// Brugeren (26-10): "lytter til det frequency point (dvs praecis de frekvenser, som frequency pointet er i
// beroering med), man p.t. har valgt". Filteret laegges oven paa baandets solo (kun lyden mellem baandets
// kanter), saa man hoerer punktets omraade i det baand, det hoerer til:
//   Bell, Notch, Band pass: baandpas om punktets frekvens med punktets Q
//   Low shelf, High pass:   lavpas ved frekvensen (punktet virker paa det, der ligger under)
//   High shelf, Low pass:   hoejpas ved frekvensen (punktet virker paa det, der ligger over)
// Lavpas/hoejpas er 24 dB/okt (to Butterworth = Linkwitz-Riley, -6 dB paa frekvensen).
struct PointSoloCoefs { Coef c[2]; int n = 0; };

inline PointSoloCoefs designPointSolo (int type, double fc, double q, double fs)
{
    PointSoloCoefs s;
    const double f = safeFreq (fc, fs);
    switch (type)
    {
        case 1: case 3:  s.c[0] = s.c[1] = designLowpass  (f, 0.70710678118654752, fs); s.n = 2; break;
        case 2: case 4:  s.c[0] = s.c[1] = designHighpass (f, 0.70710678118654752, fs); s.n = 2; break;
        default:         s.c[0] = designBandpass (f, std::max (q, 0.1), fs);           s.n = 1; break;
    }
    return s;
}

inline double pointSoloGain (const PointSoloCoefs& s, double f, double fs)
{
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < s.n; ++i) h *= coefResponse (s.c[i], f, fs);
    return std::abs (h);
}

// Punktets solo-filter for de aktuelle parametre (punktets frekvens holdes indenfor baandets kanter)
inline PointSoloCoefs pointSoloFor (const EngineParams& p, double fs)
{
    const int b = std::clamp (p.soloBand, 0, kBands - 1), k = std::clamp (p.soloPoint, 0, kPoints - 1);
    const PointParams& pp = p.band[b].pts[k];
    const BandEdges e = bandEdges (p, p.baseRate);
    return designPointSolo (pp.type, clampd (pp.freq, e.lo[b], e.hi[b]), pp.q, fs);
}

// Koerer paa base-rate efter motoren (begge fasetilstande). Toning ind og ud paa 10 ms.
class PointSolo
{
public:
    void prepare (double fsBase)
    {
        fs = fsBase;
        mix.setTime (fs, 10.0);
        mix.snap (0.0);
        f[0].reset(); f[1].reset();
    }

    void setParams (const EngineParams& p)
    {
        const bool on = p.soloBand >= 0 && p.soloPoint >= 0 && ! p.bypass;
        if (on)
        {
            if (mix.v < 1.0e-6) { f[0].reset(); f[1].reset(); }       // starter fra stilhed: ingen gammel hale
            const PointSoloCoefs c = pointSoloFor (p, fs);
            f[0].set (c.c[0]);
            f[1].set (c.c[1]);
            n = c.n;
        }
        mix.target (on ? 1.0 : 0.0);
    }

    void process (float* left, float* right, int len)
    {
        if (mix.v < 1.0e-9 && mix.t < 1.0e-9) return;              // fra: lyden er uroert
        for (int s = 0; s < len; ++s)
        {
            const double m = mix.next();
            const __m128d x = _mm_set_pd ((double) right[s], (double) left[s]);
            __m128d y = f[0].process (x);
            if (n > 1) y = f[1].process (y);
            alignas (16) double o[2];
            _mm_store_pd (o, _mm_add_pd (x, mul (_mm_sub_pd (y, x), m)));
            left[s]  = (float) o[0];
            right[s] = (float) o[1];
        }
    }

private:
    double  fs = 44100.0;
    Ramp    mix;
    Biquad2 f[2];
    int     n = 1;
};

// ---------------------------------------------------------------------------
//  0 ms-motoren. Koerer paa (evt. oversamplet) stereo-lyd. Ingen allokering i process.
// ---------------------------------------------------------------------------
// Opdelingen (alle crossovers er Linkwitz-Riley, hvor LP + HP = AP):
//   G_b = x gennem allpassene fra alle de ANDRE baands crossovers
//   T   = x gennem allpassene fra alle crossovers            (den toerre sum, en ren allpass)
//   baand b, crossovers a < b < c < d (se Layout). Murene a og d deles FOERST, saa fade-graensernes
//   crossovers aldrig slipper noget forbi murene:
//     m = LPd(HPa(G)),  fade under = LPb(m),  r = HPb(m),  omraade = LPc(r),  fade over = HPc(r)
//     u = APc(tailLo * fade under) + omraade + tailHi * fade over
//   Alle tre dele ender med faserne fra a, b, c og d, saa sammen med G_b er de praecis i fase med T.
// Allpassene deles mellem baandene: H01 = baand 0+1's crossovers, H23 = baand 2+3's, og
// G_0 = H23 + baand 1, G_1 = H23 + baand 0, G_2 = H01 + baand 3, G_3 = H01 + baand 2, T = G_0 + baand 0.
class Engine
{
public:
    bool forceTree = false;                 // kun til test: koer altid opdelingen
                                            // (Doppler Saturator: altid til, saa Doppler-effekten er der ved 0 % drive)
    bool onlyOnBands = false;               // Doppler Saturator: spring de slukkede baands opdeling over (de taendes aldrig,
                                            // saa de behoever ikke vaere varme). Sparer ca. 3/4 af opdelingen.

    void prepare (double fsProcess)
    {
        fs = fsProcess;
        warmSamples = (int) (0.060 * fs);
        treeMix.setTime (fs, 15.0); treeMix.snap (0.0);
        soloMix.setTime (fs, 10.0); soloMix.snap (0.0);
        for (auto& r : bandG) { r.setTime (fs, 10.0); r.snap (1.0); }
        for (auto& b : bands) { b.sat.prepare (fs); b.resetSplit(); }
        haveCoefs = false;
        treeOn = treeWanted = false;
        warmLeft = 0;
        soloBand = -1;
        resetTree();
    }

    void reset()
    {
        resetTree();
        treeMix.snap (0.0);
        soloMix.snap (0.0);
        treeOn = treeWanted = false;
    }

    void setParams (const EngineParams& p)
    {
        // Edge Q's form glider i smaa skridt pr. blok (den flytter crossovers), saa intet klikker.
        // Et baand der staar stille hopper direkte til maalet.
        const ZlSpec sp = zlSpec (p.phaseMode);
        const double dt = std::max (samplesSince, 1) / fs;
        samplesSince = 0;
        FadeShape shapes[kBands];
        for (int b = 0; b < kBands; ++b)
        {
            const FadeShape tgt = fadeShape (p.band[b], sp);
            BandState& B = bands[b];
            B.fadeCur = (haveCoefs && B.sat.active) ? stepShape (B.fadeCur, tgt, dt) : tgt;
            shapes[b] = B.fadeCur;
        }
        const Layout L = makeLayout (p, fs, shapes);

        XoverCoefs xc[kEnds];
        for (int k = 0; k < kEnds; ++k) xc[k] = designEnd (k, L, fs, sp);

        static const int e01[] = { 0, 1, 2, 3, 4, 5, 6, 7 }, e23[] = { 8, 9, 10, 11, 12, 13, 14, 15 };
        static const int e0[] = { 0, 1, 2, 3 }, e1[] = { 4, 5, 6, 7 }, e2[] = { 8, 9, 10, 11 }, e3[] = { 12, 13, 14, 15 };
        h01.set (e01, 8, xc);
        h23.set (e23, 8, xc);
        gch[0].set (e1, 4, xc);
        gch[1].set (e0, 4, xc);
        gch[2].set (e3, 4, xc);
        gch[3].set (e2, 4, xc);
        tch.set (e0, 4, xc);
        for (int b = 0; b < kBands; ++b)
        {
            BandState& B = bands[b];
            const XoverCoefs* x = &xc[4 * b];
            B.lpA.set (x[0].lp);  B.apA.set (x[0].ap);
            B.lpB.set (x[1].lp);  B.apB.set (x[1].ap);
            B.lpC.set (x[2].lp);  B.apC.set (x[2].ap);  B.apC2.set (x[2].ap);
            B.lpD.set (x[3].lp);
        }
        haveCoefs = true;

        // Solo: kun lyden mellem det valgte baands kanter (fadet udenfor hoerer ikke med)
        const bool soloMode = p.soloBand >= 0;
        if (soloMode)
        {
            if (p.soloBand != soloBand || soloMix.v < 1.0e-6) { soloHp.reset(); soloLp.reset(); }
            soloBand = p.soloBand;
            soloHp.set (designXover (kSoloN6, L.edge.lo[soloBand], fs).hp);
            soloLp.set (designXover (kSoloN6, L.edge.hi[soloBand], fs).lp);
        }
        soloMix.target (soloMode ? 1.0 : 0.0);

        bool anySat = false;
        for (int b = 0; b < kBands; ++b)
        {
            BandState& B = bands[b];
            const bool sat = B.sat.setParams (p.band[b], L.edge.lo[b], L.edge.hi[b], fs, false);
            bandOn[b] = p.band[b].on;

            const FadeCoefs fcs = designFade (B.fadeCur, L.edge.lo[b], L.edge.hi[b], fs);
            B.fadeLo.set (fcs.lo);
            B.fadeHi.set (fcs.hi);
            B.swingLo.set (fcs.swLo);
            B.swingHi.set (fcs.swHi);
            B.kLo  = fcs.kLo;
            B.kHi  = fcs.kHi;
            B.beta = fcs.beta;

            const bool soloSel = (p.soloBand == b);
            bandG[b].target (soloMode ? (soloSel ? 1.0 : 0.0) : 1.0);
            if (sat && (! soloMode || soloSel)) anySat = true;
        }

        treeWanted = ! p.bypass && (forceTree || soloMode || anySat);

        if (treeWanted && ! treeOn)
        {
            treeOn = true;                       // koldstart: varm filtrene op mens dry stadig hoeres
            resetTree();
            warmLeft = warmSamples;
        }
    }

    void process (float* left, float* right, int n)
    {
        samplesSince += n;
        if (! treeOn) return;                    // intet at goere: lyden er uroert (bit-identisk)

        const double dcR = std::exp (-2.0 * kPi * 3.0 / fs);

        for (int s = 0; s < n; ++s)
        {
            treeMix.target ((treeWanted && warmLeft == 0) ? 1.0 : 0.0);
            const double tm = treeMix.next();
            if (warmLeft > 0) --warmLeft;
            const double sm = soloMix.next();

            const __m128d x = _mm_set_pd ((double) right[s], (double) left[s]);

            // ---- faelles allpass-faser og den toerre sum ----
            // (onlyOnBands: allpass-kaederne, som kun de slukkede baand bruger, springes over. G[0] skal altid
            // med, fordi den toerre sum T bygges af den)
            auto use = [&] (int b) { return ! onlyOnBands || bandOn[b]; };
            const bool    need01 = use (2) || use (3);
            const __m128d h23v = h23.process (x);
            const __m128d h01v = need01 ? h01.process (x) : x;
            const __m128d G[kBands] = { gch[0].process (h23v), use (1) ? gch[1].process (h23v) : h23v,
                                        use (2) ? gch[2].process (h01v) : h01v, use (3) ? gch[3].process (h01v) : h01v };
            const __m128d T = tch.process (G[0]);

            __m128d out = T;
            if (sm > 0.0)
            {
                const __m128d S = soloLp.process (soloHp.process (T));
                out = _mm_add_pd (T, mul (_mm_sub_pd (S, T), sm));
            }

            // ---- saturation pr. baand ----
            for (int b = 0; b < kBands; ++b)
            {
                BandState& B = bands[b];
                if (onlyOnBands && ! bandOn[b]) continue;

                // Opdelingen koerer altid, saa filtrene er varme naar baandet slaas til (HP = AP - LP)
                const __m128d la = B.lpA.process (G[b]);
                const __m128d m  = B.lpD.process (_mm_sub_pd (B.apA.process (G[b]), la));   // mellem murene
                const __m128d tl = B.lpB.process (m);                        // fade under (raa)
                const __m128d r  = _mm_sub_pd (B.apB.process (m), tl);
                const __m128d rg = B.lpC.process (r);                        // omraadet (raa)
                const __m128d th = _mm_sub_pd (B.apC.process (r), rg);       // fade over (raa)
                const double  gb = bandG[b].next();

                // Fade-vaegtene: (1 - beta) + beta * K * 1. ordens filter * svungets Bell
                const double bt = B.beta;
                const __m128d tlw = _mm_add_pd (mul (tl, 1.0 - bt), mul (B.swingLo.process (B.fadeLo.process (tl)), bt * B.kLo));
                const __m128d thw = _mm_add_pd (mul (th, 1.0 - bt), mul (B.swingHi.process (B.fadeHi.process (th)), bt * B.kHi));
                const __m128d u = _mm_add_pd (_mm_add_pd (B.apC2.process (tlw), rg), thw);
                if (! B.sat.active) continue;

                out = _mm_add_pd (out, mul (B.sat.process (u, dcR), gb));
            }

            alignas (16) double o[2];
            _mm_store_pd (o, _mm_add_pd (mul (x, 1.0 - tm), mul (out, tm)));
            left[s]  = (float) o[0];
            right[s] = (float) o[1];

            if (! treeWanted && tm <= 1.0e-6)
            {
                treeOn = false;          // faerdig: resten af blokken er uroert (dry)
                return;
            }
        }
    }

private:
    struct BandState
    {
        Chain2  lpA, apA, lpB, apB, lpC, apC, apC2, lpD;          // baandets fire crossovers (se Engine)
        Biquad2 fadeLo, fadeHi;                                   // fadets 1. ordens filtre
        Biquad2 swingLo, swingHi;                                 // svungets Bell-daempning (3.10)
        double  beta = 0.0, kLo = kTailK, kHi = kTailK;
        FadeShape fadeCur;
        BandSat sat;

        void resetSplit()
        {
            lpA.reset(); apA.reset(); lpB.reset(); apB.reset(); lpC.reset(); apC.reset(); apC2.reset();
            lpD.reset(); fadeLo.reset(); fadeHi.reset(); swingLo.reset(); swingHi.reset();
        }
    };

    void resetTree()
    {
        h01.reset(); h23.reset(); tch.reset();
        for (auto& c : gch) c.reset();
        soloHp.reset(); soloLp.reset();
        for (auto& B : bands) { B.resetSplit(); B.sat.resetSat(); }
    }

    double   fs = 44100.0;
    int      warmSamples = 2646, warmLeft = 0;
    bool     treeOn = false, treeWanted = false, haveCoefs = false;
    int      soloBand = -1;
    int      samplesSince = 0;                        // samples siden sidste setParams (til glidning i tid)
    bool     bandOn[kBands] = { true, true, true, true };
    Ramp     treeMix, soloMix, bandG[kBands];         // bandG: solo af baandets saturation
    ApChain2 h01, h23, gch[kBands], tch;
    Chain2   soloHp, soloLp;
    BandState bands[kBands];
};

// ---------------------------------------------------------------------------
//  FFT (radix 2, komplex, double). n skal vaere en potens af 2.
// ---------------------------------------------------------------------------
class Fft
{
public:
    void init (int size)
    {
        n = size;
        cs.resize ((size_t) n / 2);
        sn.resize ((size_t) n / 2);
        for (int m = 0; m < n / 2; ++m) { cs[(size_t) m] = std::cos (2.0 * kPi * m / n); sn[(size_t) m] = std::sin (2.0 * kPi * m / n); }
        rev.resize ((size_t) n);
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    // Fremad: X[k] = sum x[t] e^{-2 pi i k t / n}. Baglaens uden 1/n.
    void run (double* re, double* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        const double sg = inverse ? 1.0 : -1.0;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cs[(size_t) (k * step)], wi = sg * sn[(size_t) (k * step)];
                    const int a = i + k, b = a + half;
                    const double tr = wr * re[b] - wi * im[b], ti = wr * im[b] + wi * re[b];
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
        }
    }

private:
    int n = 0;
    std::vector<double> cs, sn;
    std::vector<int> rev;
};

// ---------------------------------------------------------------------------
//  Lineaer fase: opdelingen (base-rate, stereo)
// ---------------------------------------------------------------------------
// Hvert baand er x foldet med en symmetrisk FIR (nul fase), saa baandene og den toerre lyd har PRAECIS samme
// forsinkelse og fase, ogsaa mens baandene flyttes: ingen Doppler, ingen pitch-aendring. Foldningen er
// partitioneret (K blokke af P samples, FFT af 2P, overlap-save; K = 32 / 64 / 128 efter skarphed). Venstre
// og hoejre regnes i een kompleks FFT (x = L + i R; filteret er reelt).
// Regnearbejdet spredes jaevnt over blokken: partitionerne 1..K-1 (de aeldre inputblokke) laegges sammen lidt
// ad gangen, mens den naeste blok samles, og ved blokgraensen mangler kun den nyeste blok og IFFT'en. Ellers
// ville hele foldningen ligge i een lydblok fra vaerten (med den lange FIR risiko for udfald i Ableton).
// Nye filtre designes i en baggrundstraad (et maal -> FIR via FFT og Kaiser-vindue) og blendes ind over een
// blok. Begge filtre er lineaer fase med samme forsinkelse, saa blendingen kun aendrer styrken, ikke fasen.
// Skiftes skarpheden, aendres forsinkelsen: det nye saet tages i brug paa een gang (uden blending), og den
// toerre lyd laeses med den nye forsinkelse fra samme blok. Alt er allokeret til den laengste FIR i prepare,
// og forsinkelseslinjen gemmer altid de sidste 128 blokke, saa et skift kraever ingen allokering.
// Udgange (12 kanaler): 0/1 = toer lyd, 2+2b/3+2b = baand b, 10/11 = solo (kun mellem det valgte baands kanter).
class LinSplit
{
public:
    static constexpr int kFilters  = kBands + 1;          // 4 baand + solo
    static constexpr int kChannels = 2 + 2 * kFilters;

    ~LinSplit() { stopDesigner(); }

    // asyncDesign = false: filtrene designes med det samme i setMasks (test). true: i baggrundstraaden.
    // initial: filtrene designes med det samme (foer traaden starter), saa baandene virker fra foerste sample.
    void prepare (double fsBase, bool asyncDesign, const LinMask* initial = nullptr)
    {
        stopDesigner();
        fs   = fsBase;
        P    = linBlockSize (fs);
        N    = 2 * P;
        Kmax = kLinPartitions << kLinSharpMax;
        async = asyncDesign;

        fftN.init (N);
        const double i0b = besselI0 (kLinKaiserBeta);
        for (int s = 0; s <= kLinSharpMax; ++s)
        {
            Geo& g = geo[s];
            g.L = linFirLength (fs, s);
            g.K = kLinPartitions << s;                            // K * P = L
            g.center = g.L / 2 - 1;
            g.fftD.init (2 * g.L);
            g.win.assign ((size_t) g.L, 0.0);
            for (int t = 0; t < g.L - 1; ++t)
            {
                const double r = (t - g.center) / (double) g.center;
                g.win[(size_t) t] = besselI0 (kLinKaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            }
        }

        const size_t setSize = (size_t) kFilters * Kmax * N;
        for (auto& s : sets) { s.re.assign (setSize, 0.f); s.im.assign (setSize, 0.f); s.sharp = 0; s.state.store (0); }
        masterRe.assign (setSize, 0.f); masterIm.assign (setSize, 0.f);
        for (auto& v : masterValid) v = false;
        fdlRe.assign ((size_t) Kmax * N, 0.f); fdlIm.assign ((size_t) Kmax * N, 0.f);
        frRe.assign ((size_t) N, 0.0); frIm.assign ((size_t) N, 0.0);
        for (int a = 0; a < 2; ++a) { accRe[a].assign ((size_t) kFilters * N, 0.f); accIm[a].assign ((size_t) kFilters * N, 0.f); }
        yRe.assign ((size_t) N, 0.0); yIm.assign ((size_t) N, 0.0);
        y2Re.assign ((size_t) N, 0.0); y2Im.assign ((size_t) N, 0.0);
        prevL.assign ((size_t) P, 0.0); prevR.assign ((size_t) P, 0.0);
        curL.assign ((size_t) P, 0.0);  curR.assign ((size_t) P, 0.0);
        for (int f = 0; f < kFilters; ++f) { outL[f].assign ((size_t) P, 0.f); outR[f].assign ((size_t) P, 0.f); }
        dryLen = linLatency (fs, kLinSharpMax) + 1;
        dryL.assign ((size_t) dryLen, 0.f); dryR.assign ((size_t) dryLen, 0.f);
        dRe.assign ((size_t) 2 * geo[kLinSharpMax].L, 0.0); dIm.assign ((size_t) 2 * geo[kLinSharpMax].L, 0.0);
        pbRe.assign ((size_t) N, 0.0); pbIm.assign ((size_t) N, 0.0);

        readyIdx.store (-1);
        active = -1; fadeFrom = -1; activeK = 0;
        curLat = linLatency (fs, initial != nullptr ? std::clamp (initial[0].sharp, 0, kLinSharpMax) : 0);
        for (auto& m : lastPosted) m = LinMask();
        havePosted = false;
        reset();
        if (initial != nullptr)
        {
            designAndPublish (initial);
            for (int f = 0; f < kFilters; ++f) lastPosted[f] = initial[f];
            havePosted = true;
            takeFirst();
        }

        if (async)
        {
            quit = false; reqSeq = doneSeq = 0;
            designer = std::thread ([this] { designerLoop(); });
        }
    }

    // Toemmer lyden (ikke filtrene)
    void reset()
    {
        std::fill (fdlRe.begin(), fdlRe.end(), 0.f); std::fill (fdlIm.begin(), fdlIm.end(), 0.f);
        std::fill (prevL.begin(), prevL.end(), 0.0); std::fill (prevR.begin(), prevR.end(), 0.0);
        for (int a = 0; a < 2; ++a) { std::fill (accRe[a].begin(), accRe[a].end(), 0.f); std::fill (accIm[a].begin(), accIm[a].end(), 0.f); }
        for (int f = 0; f < kFilters; ++f) { std::fill (outL[f].begin(), outL[f].end(), 0.f); std::fill (outR[f].begin(), outR[f].end(), 0.f); }
        std::fill (dryL.begin(), dryL.end(), 0.f); std::fill (dryR.begin(), dryR.end(), 0.f);
        pos = 0; slot = 0; dryPos = 0; accDone = 0;
    }

    // Forsinkelse (samples) fra input til alle udgange (afhaenger af skarpheden paa det filtersaet der spiller)
    int latency() const { return curLat; }

    // Kaldes fra lyd-traaden. Nye maal sendes til designeren (aldrig blokerende).
    void setMasks (const LinMask masks[kFilters])
    {
        bool same = havePosted;
        for (int f = 0; f < kFilters && same; ++f) same = masks[f] == lastPosted[f];
        if (same) return;

        if (! async)
        {
            designAndPublish (masks);
            for (int f = 0; f < kFilters; ++f) lastPosted[f] = masks[f];
            havePosted = true;
            if (active < 0) takeFirst();                          // det allerfoerste saet bruges med det samme
            return;
        }

        std::unique_lock<std::mutex> lk (mtx, std::try_to_lock);
        if (! lk.owns_lock()) return;                             // proev igen naeste blok
        for (int f = 0; f < kFilters; ++f) { pending[f] = masks[f]; lastPosted[f] = masks[f]; }
        havePosted = true;
        ++reqSeq;
        lk.unlock();
        cv.notify_one();
    }

    void process (const float* inL, const float* inR, int n, float* const* out)
    {
        for (int s = 0; s < n; ++s)
        {
            curL[(size_t) pos] = inL[s];
            curR[(size_t) pos] = inR[s];

            // toer lyd: samme forsinkelse som baandene
            dryL[(size_t) dryPos] = inL[s];
            dryR[(size_t) dryPos] = inR[s];
            int rd = dryPos - curLat; if (rd < 0) rd += dryLen;       // curLat < dryLen
            out[0][s] = dryL[(size_t) rd];
            out[1][s] = dryR[(size_t) rd];
            if (++dryPos == dryLen) dryPos = 0;

            for (int f = 0; f < kFilters; ++f)
            {
                out[2 + 2 * f][s] = outL[f][(size_t) pos];
                out[3 + 2 * f][s] = outR[f][(size_t) pos];
            }

            if (++pos == P) { runBlock(); pos = 0; }
            else if (accDone + 1 < activeK && pos * (activeK - 1) >= (accDone + 1) * P)
                accumulate (++accDone);                           // een partition ad gangen, jaevnt over blokken
        }
    }

private:
    struct FilterSet
    {
        std::vector<float> re, im;            // kFilters * Kmax * N (kun de foerste K partitioner bruges)
        int sharp = 0;                        // skarphed (= FIR-laengde og forsinkelse)
        std::atomic<int> state { 0 };         // 0 = fri, 1 = i brug (designes / klar / aktiv / blendes ud)
    };

    // FIR-laengde, antal partitioner, centrum og vindue for een skarphed (aendres kun i prepare)
    struct Geo
    {
        int L = 0, K = 0, center = 0;
        std::vector<double> win;
        Fft fftD;
    };

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 60; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; if (term < 1e-17 * sum) break; }
        return sum;
    }

    void takeFirst()
    {
        active = readyIdx.exchange (-1);
        if (active < 0) return;
        activeK = geo[sets[active].sharp].K;
        curLat  = geo[sets[active].sharp].center + P;
    }

    void runBlock()
    {
        // 1) resten af partitionerne 1..K-1 til blokken der nu skal ud (de fleste er lagt sammen undervejs)
        while (accDone + 1 < activeK) accumulate (++accDone);

        // 2) overlap-save: [forrige blok, denne blok] -> spektrum i forsinkelseslinjen
        for (int i = 0; i < P; ++i)
        {
            frRe[(size_t) i] = prevL[(size_t) i]; frRe[(size_t) (P + i)] = curL[(size_t) i];
            frIm[(size_t) i] = prevR[(size_t) i]; frIm[(size_t) (P + i)] = curR[(size_t) i];
        }
        std::swap (prevL, curL);
        std::swap (prevR, curR);
        fftN.run (frRe.data(), frIm.data(), false);
        slot = (slot + 1) % Kmax;
        float* xr = &fdlRe[(size_t) slot * N];
        float* xi = &fdlIm[(size_t) slot * N];
        for (int i = 0; i < N; ++i) { xr[i] = (float) frRe[(size_t) i]; xi[i] = (float) frIm[(size_t) i]; }

        // 3) den nyeste blok (partition 0) + IFFT -> udgangene for de naeste P samples
        if (active >= 0)
        {
            const double inv = 1.0 / N;
            for (int f = 0; f < kFilters; ++f)
            {
                finish (active, 0, f, yRe, yIm);
                if (fadeFrom >= 0)
                {
                    finish (fadeFrom, 1, f, y2Re, y2Im);
                    for (int i = 0; i < P; ++i)
                    {
                        const double a = (i + 1.0) / P;
                        outL[f][(size_t) i] = (float) (inv * (a * yRe[(size_t) (P + i)] + (1.0 - a) * y2Re[(size_t) (P + i)]));
                        outR[f][(size_t) i] = (float) (inv * (a * yIm[(size_t) (P + i)] + (1.0 - a) * y2Im[(size_t) (P + i)]));
                    }
                }
                else
                {
                    for (int i = 0; i < P; ++i)
                    {
                        outL[f][(size_t) i] = (float) (inv * yRe[(size_t) (P + i)]);
                        outR[f][(size_t) i] = (float) (inv * yIm[(size_t) (P + i)]);
                    }
                }
            }
            curLat = geo[sets[active].sharp].center + P;          // den toerre lyd foelger med fra samme sample
            if (fadeFrom >= 0) { sets[fadeFrom].state.store (0); fadeFrom = -1; }
        }

        // 4) filtrene til den naeste blok: nyt saet fra designeren?
        const int r = readyIdx.exchange (-1);
        if (r >= 0)
        {
            if (active < 0)                                   active = r;
            else if (sets[r].sharp == sets[active].sharp)   { fadeFrom = active; active = r; }
            else                                            { sets[active].state.store (0); active = r; }   // ny forsinkelse: intet blend
        }
        activeK = active >= 0 ? geo[sets[active].sharp].K : 0;
        std::fill (accRe[0].begin(), accRe[0].end(), 0.f); std::fill (accIm[0].begin(), accIm[0].end(), 0.f);
        if (fadeFrom >= 0) { std::fill (accRe[1].begin(), accRe[1].end(), 0.f); std::fill (accIm[1].begin(), accIm[1].end(), 0.f); }
        accDone = 0;
    }

    // acc += X * H_f[k] (komplekst, bin for bin). SSE med 4 bins ad gangen (N er et multiplum af 4): MSVC
    // vektoriserer ikke selv loekken, og den er det tungeste i hele den lineaere fase.
    void mac (const FilterSet& S, int f, int k, const float* xr, const float* xi, float* ar, float* ai) const
    {
        const float* hr = &S.re[((size_t) f * Kmax + k) * N];
        const float* hi = &S.im[((size_t) f * Kmax + k) * N];
        for (int i = 0, n = N; i < n; i += 4)
        {
            const __m128 a = _mm_loadu_ps (xr + i), b = _mm_loadu_ps (xi + i);
            const __m128 c = _mm_loadu_ps (hr + i), d = _mm_loadu_ps (hi + i);
            _mm_storeu_ps (ar + i, _mm_add_ps (_mm_loadu_ps (ar + i), _mm_sub_ps (_mm_mul_ps (a, c), _mm_mul_ps (b, d))));
            _mm_storeu_ps (ai + i, _mm_add_ps (_mm_loadu_ps (ai + i), _mm_add_ps (_mm_mul_ps (a, d), _mm_mul_ps (b, c))));
        }
    }

    // Partition k (>= 1) til blokken der er ved at blive samlet: den bruger inputblokken slot + 1 - k
    void accumulate (int k)
    {
        const int xs = (slot + 1 - k + Kmax) % Kmax;
        const float* xr = &fdlRe[(size_t) xs * N];
        const float* xi = &fdlIm[(size_t) xs * N];
        for (int f = 0; f < kFilters; ++f)
        {
            mac (sets[active], f, k, xr, xi, &accRe[0][(size_t) f * N], &accIm[0][(size_t) f * N]);
            if (fadeFrom >= 0) mac (sets[fadeFrom], f, k, xr, xi, &accRe[1][(size_t) f * N], &accIm[1][(size_t) f * N]);
        }
    }

    // Blokken er samlet: den nyeste inputblok (partition 0) laegges til, og y = IFFT (uden 1/N)
    void finish (int setIdx, int a, int f, std::vector<double>& outRe, std::vector<double>& outIm)
    {
        float* ar = &accRe[a][(size_t) f * N];
        float* ai = &accIm[a][(size_t) f * N];
        mac (sets[setIdx], f, 0, &fdlRe[(size_t) slot * N], &fdlIm[(size_t) slot * N], ar, ai);
        for (int i = 0; i < N; ++i) { outRe[(size_t) i] = ar[i]; outIm[(size_t) i] = ai[i]; }
        fftN.run (outRe.data(), outIm.data(), true);
    }

    // ---- design (baggrundstraaden, eller direkte i test/prepare) ----
    void designFilter (const LinMask& m, int sharp, float* dstRe, float* dstIm)
    {
        const Geo& g = geo[sharp];
        const int Nd = 2 * g.L;
        for (int i = 0; i < Nd; ++i)
        {
            const double f = (i <= Nd / 2 ? i : Nd - i) * fs / Nd;
            dRe[(size_t) i] = m.binValue (f, fs / Nd);
            dIm[(size_t) i] = 0.0;
        }
        g.fftD.run (dRe.data(), dIm.data(), true);                // nul-fase impulssvar (cirkulaert, centreret i 0)
        const double sc = 1.0 / Nd;
        for (int k = 0; k < g.K; ++k)
        {
            for (int i = 0; i < N; ++i) { pbRe[(size_t) i] = 0.0; pbIm[(size_t) i] = 0.0; }
            for (int i = 0; i < P; ++i)
            {
                const int t = k * P + i;
                if (t >= g.L - 1) continue;
                const int idx = ((t - g.center) % Nd + Nd) % Nd;
                pbRe[(size_t) i] = dRe[(size_t) idx] * sc * g.win[(size_t) t];
            }
            fftN.run (pbRe.data(), pbIm.data(), false);
            for (int i = 0; i < N; ++i)
            {
                dstRe[(size_t) k * N + i] = (float) pbRe[(size_t) i];
                dstIm[(size_t) k * N + i] = (float) pbIm[(size_t) i];
            }
        }
    }

    void designAndPublish (const LinMask masks[kFilters])
    {
        const int sharp = std::clamp (masks[0].sharp, 0, kLinSharpMax);    // alle filtre i et saet har samme laengde
        const size_t fsz = (size_t) Kmax * N, used = (size_t) geo[sharp].K * N;
        for (int f = 0; f < kFilters; ++f)
            if (! masterValid[f] || ! (masks[f] == masterSpec[f]))
            {
                designFilter (masks[f], sharp, &masterRe[f * fsz], &masterIm[f * fsz]);
                masterSpec[f] = masks[f];
                masterValid[f] = true;
            }

        int idx = -1;
        while (idx < 0)
        {
            for (int i = 0; i < 4 && idx < 0; ++i)
            {
                int expected = 0;
                if (sets[i].state.compare_exchange_strong (expected, 1)) idx = i;
            }
            if (idx < 0) std::this_thread::yield();               // sker ikke: hoejst 3 saet er i brug ad gangen
        }
        for (int f = 0; f < kFilters; ++f)
        {
            std::memcpy (&sets[idx].re[f * fsz], &masterRe[f * fsz], used * sizeof (float));
            std::memcpy (&sets[idx].im[f * fsz], &masterIm[f * fsz], used * sizeof (float));
        }
        sets[idx].sharp = sharp;
        const int old = readyIdx.exchange (idx);
        if (old >= 0) sets[old].state.store (0);                  // blev aldrig brugt
    }

    void designerLoop()
    {
        std::unique_lock<std::mutex> lk (mtx);
        while (! quit)
        {
            cv.wait (lk, [this] { return quit || reqSeq != doneSeq; });
            if (quit) break;
            LinMask req[kFilters];
            for (int f = 0; f < kFilters; ++f) req[f] = pending[f];
            const unsigned seq = reqSeq;
            lk.unlock();
            designAndPublish (req);
            lk.lock();
            doneSeq = seq;
        }
    }

    void stopDesigner()
    {
        if (! designer.joinable()) return;
        { std::lock_guard<std::mutex> lk (mtx); quit = true; }
        cv.notify_one();
        designer.join();
    }

    double fs = 44100;
    int P = 512, N = 1024, Kmax = kLinPartitions << kLinSharpMax;
    bool async = false;
    Fft fftN;
    Geo geo[kLinSharpMax + 1];

    FilterSet sets[4];
    std::atomic<int> readyIdx { -1 };
    int active = -1, fadeFrom = -1;     // filtrene til blokken der er ved at blive samlet
    int activeK = 0, accDone = 0;       // partitioner i det aktive saet, og hvor mange (1..) der er lagt sammen
    int curLat = 0;                     // forsinkelsen paa det der spiller nu

    std::vector<float> fdlRe, fdlIm, accRe[2], accIm[2];      // acc[0] = aktivt saet, acc[1] = det der blendes ud
    std::vector<double> frRe, frIm, yRe, yIm, y2Re, y2Im, prevL, prevR, curL, curR;
    std::vector<float> outL[kFilters], outR[kFilters];
    std::vector<float> dryL, dryR;
    int pos = 0, slot = 0, dryPos = 0, dryLen = 1;

    // designerens egne data
    std::vector<float> masterRe, masterIm;
    LinMask masterSpec[kFilters];
    bool masterValid[kFilters] = {};
    std::vector<double> dRe, dIm, pbRe, pbIm;

    // til designeren
    std::thread designer;
    std::mutex mtx;
    std::condition_variable cv;
    bool quit = false;
    unsigned reqSeq = 0, doneSeq = 0;
    LinMask pending[kFilters];
    LinMask lastPosted[kFilters];
    bool havePosted = false;
};

// Maalene til LinSplit ud fra parametrene (4 baand + solo-baandet)
inline void linMasks (const EngineParams& p, int soloShown, LinMask out[LinSplit::kFilters])
{
    for (int b = 0; b < kBands; ++b) out[b] = linMaskFor (p, b, false);
    out[kBands] = linMaskFor (p, std::max (0, soloShown), true);
}

// ---------------------------------------------------------------------------
//  Lineaer fase: saturationen (paa base-rate eller oversamplet). Laeser LinSplit's 12 kanaler og skriver
//  resultatet i kanal 0 og 1.
// ---------------------------------------------------------------------------
class LinShaper
{
public:
    void prepare (double fsShape)
    {
        fs = fsShape;
        soloMix.setTime (fs, 10.0); soloMix.snap (0.0);
        for (auto& r : bandG) { r.setTime (fs, 10.0); r.snap (1.0); }
        for (auto& b : bands) b.prepare (fs);
    }

    void reset()
    {
        soloMix.snap (soloMix.t);
        for (auto& b : bands) b.resetSat();
    }

    void setParams (const EngineParams& p)
    {
        const BandEdges edge = bandEdges (p, p.baseRate);
        const bool soloMode = p.soloBand >= 0 && ! p.bypass;
        for (int b = 0; b < kBands; ++b)
        {
            bands[b].setParams (p.band[b], edge.lo[b], edge.hi[b], fs, p.bypass);
            bandG[b].target (soloMode ? (p.soloBand == b ? 1.0 : 0.0) : 1.0);
        }
        soloMix.target (soloMode ? 1.0 : 0.0);
    }

    void process (float* const* ch, int n)
    {
        const double dcR = std::exp (-2.0 * kPi * 3.0 / fs);
        for (int s = 0; s < n; ++s)
        {
            const __m128d x = _mm_set_pd ((double) ch[1][s], (double) ch[0][s]);
            __m128d out = x;
            const double sm = soloMix.next();
            if (sm > 0.0)
            {
                const __m128d S = _mm_set_pd ((double) ch[11][s], (double) ch[10][s]);
                out = _mm_add_pd (x, mul (_mm_sub_pd (S, x), sm));
            }
            for (int b = 0; b < kBands; ++b)
            {
                const double gb = bandG[b].next();
                if (! bands[b].active) continue;
                const __m128d u = _mm_set_pd ((double) ch[3 + 2 * b][s], (double) ch[2 + 2 * b][s]);
                out = _mm_add_pd (out, mul (bands[b].process (u, dcR), gb));
            }
            alignas (16) double o[2];
            _mm_store_pd (o, out);
            ch[0][s] = (float) o[0];
            ch[1][s] = (float) o[1];
        }
    }

private:
    double  fs = 44100;
    BandSat bands[kBands];
    Ramp    soloMix, bandG[kBands];
};

// Lineaer fase uden oversampling (test, og naar oversampling er slaaet fra): opdeling + saturation
class LinEngine
{
public:
    void prepare (double fs, int maxBlockSize, bool asyncDesign, const EngineParams* initial = nullptr)
    {
        maxBlock = std::max (1, maxBlockSize);
        LinMask m[LinSplit::kFilters];
        if (initial != nullptr) linMasks (*initial, initial->soloBand >= 0 ? initial->soloBand : 0, m);
        split.prepare (fs, asyncDesign, initial != nullptr ? m : nullptr);
        shaper.prepare (fs);
        for (int c = 0; c < LinSplit::kChannels; ++c) { buf[c].assign ((size_t) maxBlock, 0.f); ptr[c] = buf[c].data(); }
    }

    void setParams (const EngineParams& p)
    {
        LinMask m[LinSplit::kFilters];
        linMasks (p, p.soloBand >= 0 ? p.soloBand : lastSolo, m);
        if (p.soloBand >= 0) lastSolo = p.soloBand;
        split.setMasks (m);
        shaper.setParams (p);
    }

    void process (float* left, float* right, int n)
    {
        for (int s = 0; s < n; s += maxBlock)
        {
            const int len = std::min (maxBlock, n - s);
            split.process (left + s, right + s, len, ptr);
            shaper.process (ptr, len);
            std::memcpy (left + s,  ptr[0], (size_t) len * sizeof (float));
            std::memcpy (right + s, ptr[1], (size_t) len * sizeof (float));
        }
    }

    int latency() const { return split.latency(); }

    LinSplit  split;
    LinShaper shaper;
    int lastSolo = 0;

private:
    int maxBlock = 512;
    std::vector<float> buf[LinSplit::kChannels];
    float* ptr[LinSplit::kChannels] = {};
};

} // namespace satdsp
