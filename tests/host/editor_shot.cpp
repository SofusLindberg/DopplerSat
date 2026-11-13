// Billeder af Doppler Saturator's editor uden vaert: EditorShot.exe <mappe>
// Koerer lyd gennem processoren (saa spektrum og LFO bevaeger sig) og gemmer PNG'er i et par tilstande.
#include <JuceHeader.h>
#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"

static void setParam (DopplerSatProcessor& p, const char* id, float v)
{
    if (auto* par = p.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (v));
}

// 1.10: Sweep-displayets ender, som displayet viser dem (Offset regnet med)
static void sweepEnds (DopplerSatProcessor& p, double& lo, double& hi)
{
    const double off = (double) p.apvts.getRawParameterValue ("offset")->load() * 0.01;
    lo = p.bandCentreFor (juce::jlimit (-1.0, 1.0, -1.0 + off));
    hi = p.bandCentreFor (juce::jlimit (-1.0, 1.0,  1.0 + off));
}

// 1.10 (brugeren 2026-11-13): skriv bunden eller toppen i Sweep-displayet; den anden ende skal blive staaende, og den
// skrevne naas (1.11: Frequency og Sweep har ingen trin, saa begge skal ramme inden for 0.1 %). Bund over top skal
// afvises. 1.13: uden Offset kan alle omraader mellem 20 Hz og 20 kHz laves (sweepForEnds maa aldrig svare 2 = den anden
// ende maatte flyttes). Med Offset kan omraadet vaere for stort (Offset bruger kun en del af det); saa skal den skrevne
// ende stadig naas.
static bool sweepTypeTest (DopplerSatEditor& ed, DopplerSatProcessor& p)
{
    bool ok = true;
    int cases = 0, moved = 0, movedOffset = 0;
    struct S { float freq, sweep; };
    for (float off : { 0.f, 40.f, -60.f, 100.f, -100.f })
        for (S s : { S { 600.f, 50.f }, S { 100.f, 80.f }, S { 5000.f, 30.f }, S { 40.f, 100.f }, S { 600.f, 0.f }, S { 12000.f, 60.f },
                     S { 20.f, 70.f }, S { 20000.f, 40.f } })
            for (int top = 0; top < 2; ++top)
                for (double factor : { 0.7, 1.3, 0.25, 4.0, 0.001, 1000.0 })
                {
                    setParam (p, "offset", off); setParam (p, "freq", s.freq); setParam (p, "amount", s.sweep);
                    double lo, hi;
                    sweepEnds (p, lo, hi);
                    const bool single = lo == hi;
                    double typed = (top ? hi : lo) * factor;
                    const double t = juce::jlimit (20.0, 20000.0, typed);
                    if ((top && t <= lo) || (! top && t >= hi && ! single)) continue;   // bund over top: nedenfor
                    if (single && t == lo) continue;
                    if (t == (top ? hi : lo)) continue;                                   // uaendret (klippet)
                    const bool typedTop = single ? t > lo : top == 1;
                    double f2, s2;
                    const int code = DopplerSatProcessor::sweepForEnds (typedTop ? lo : t, typedTop ? t : hi, typedTop, off * 0.01,
                                                                        s.freq, f2, s2);
                    if (code == 2) ++(off == 0.f ? moved : movedOffset);
                    if (! ed.applySweepText (single ? -1 : top, juce::String (typed, 3) + " Hz")) { ok = false; std::printf ("    afvist?!\n"); continue; }
                    ++cases;
                    double nlo, nhi;
                    sweepEnds (p, nlo, nhi);
                    const double fixedEnd = typedTop ? lo : hi;
                    const double gotTyped = typedTop ? nhi : nlo, gotFixed = typedTop ? nlo : nhi;
                    // 1.14: den uroerte ende staar altid. Svar 1: den skrevne naas. Svar 2 (kun med Offset): omraadet er saa
                    // bredt, som Offset tillader ((a + b) * log2 1000 / 2 oktaver), og Sweep er 100 %.
                    const double a = -juce::jmax (-1.0, off * 0.01 - 1.0), b = juce::jmin (1.0, off * 0.01 + 1.0);
                    const double maxSpan = (a + b) * 0.5 * std::log2 (1000.0);
                    const bool pass = std::abs (gotFixed / fixedEnd - 1.0) < 0.001
                                      && (code == 2 ? off != 0.f && std::abs (std::log2 (nhi / nlo) - maxSpan) < 0.002
                                                    : std::abs (gotTyped / t - 1.0) < 0.001);
                    if (! pass)
                    {
                        ok = false;
                        std::printf ("    FEJL: offset %.0f, %.0f Hz, Sweep %.0f %%, %s %.1f Hz: %.1f - %.1f -> %.1f - %.1f (svar %d)\n", off,
                                     s.freq, s.sweep, typedTop ? "top" : "bund", t, lo, hi, nlo, nhi, code);
                    }
                }
    // Hele spektret: Frequency 600 Hz, Sweep 50 %, bund 20 Hz og top 20 kHz -> 20 Hz - 20 kHz (= Sweep 100 %)
    setParam (p, "offset", 0.f); setParam (p, "freq", 600.f); setParam (p, "amount", 50.f);
    double lo, hi, hi0;
    sweepEnds (p, lo, hi0);
    ed.applySweepText (0, "20");
    sweepEnds (p, lo, hi);
    const bool fullLo = std::abs (lo / 20.0 - 1.0) < 0.005 && std::abs (hi / hi0 - 1.0) < 0.003;
    ed.applySweepText (1, "20k");
    sweepEnds (p, lo, hi);
    const float sw = p.apvts.getRawParameterValue ("amount")->load();
    const bool wideOk = fullLo && std::abs (lo / 20.0 - 1.0) < 0.005 && std::abs (hi / 20000.0 - 1.0) < 0.005 && sw > 99.9f;
    std::printf ("  hele spektret (bund 20 Hz, saa top 20 kHz): %.1f Hz - %.1f Hz, Sweep %.1f %%  %s\n", lo, hi, sw, wideOk ? "OK" : "FEJL");
    // Bund over top, og noget der ikke er et tal: afvist, intet aendret
    setParam (p, "freq", 600.f); setParam (p, "amount", 50.f);
    const float f0 = p.apvts.getRawParameterValue ("freq")->load(), a0 = p.apvts.getRawParameterValue ("amount")->load();
    const bool rej = ! ed.applySweepText (0, "5k") && ! ed.applySweepText (1, "100") && ! ed.applySweepText (0, "abc")
                  && p.apvts.getRawParameterValue ("freq")->load() == f0 && p.apvts.getRawParameterValue ("amount")->load() == a0;
    std::printf ("  bund over top / ikke et tal: %s\n", rej ? "afvist OK" : "FEJL");
    // Skrivemaader: "1,2 kHz" og "2.5k" som top
    ed.applySweepText (1, "1,2 kHz");
    sweepEnds (p, lo, hi);
    const bool fmt1 = std::abs (hi / 1200.0 - 1.0) < 0.005;
    ed.applySweepText (1, "2.5k");
    sweepEnds (p, lo, hi);
    const bool fmt2 = std::abs (hi / 2500.0 - 1.0) < 0.005;
    std::printf ("  skrivemaader \"1,2 kHz\" og \"2.5k\": %s\n", fmt1 && fmt2 ? "OK" : "FEJL");
    std::printf ("  skriv bund / top: %d tilfaelde (5 Offset, 8 indstillinger, bund og top), den anden ende flyttet %d gange uden "
                 "Offset, %d med Offset  %s\n", cases, moved, movedOffset, ok && moved == 0 ? "OK" : "FEJL");
    setParam (p, "offset", 0.f); setParam (p, "freq", 600.f); setParam (p, "amount", 30.f);
    return ok && moved == 0 && wideOk && rej && fmt1 && fmt2;
}

// 1.14 (brugeren 2026-11-13): "Når man skriver 20 hz i laveste kant, så forsvinder tallet for øverste kant, og bandet
// bevæger sig ikke" (Offset 19.1 %). Og traek: "Ved at trække lodret op og ned ud for hver tal i sweepdisplayet, kan
// man justere tallet for det tal, man har fat i" (0.02 oktav pr. pixel, Shift 0.002; den anden ende staar).
static bool bugAndDragTest (DopplerSatEditor& ed, DopplerSatProcessor& p)
{
    bool ok = true;
    auto check = [&] (const char* what, bool pass, double lo, double hi)
    {
        std::printf ("  %s: %.1f Hz - %.1f Hz  %s\n", what, lo, hi, pass ? "OK" : "FEJL");
        ok = ok && pass;
    };
    auto near = [] (double x, double want) { return std::abs (x / want - 1.0) < 0.001; };
    double lo, hi, lo0, hi0;

    // Fejlen: Offset 19.1 %, bund 20 Hz -> bunden bliver 20 Hz, toppen staar, Sweep > 0
    setParam (p, "offset", 19.1f); setParam (p, "freq", 600.f); setParam (p, "amount", 30.f);
    sweepEnds (p, lo0, hi0);
    ed.applySweepText (0, "20");
    sweepEnds (p, lo, hi);
    check ("Offset 19.1 %, skriv bund 20 Hz", near (lo, 20.0) && near (hi, hi0) && p.apvts.getRawParameterValue ("amount")->load() > 1.f, lo, hi);
    // Tilstanden fra brugerens billede (20 Hz, Sweep 0 %, Offset 19.1 %): skriv toppen 1k -> 20 Hz - 1 kHz
    setParam (p, "freq", 20.f); setParam (p, "amount", 0.f);
    ed.applySweepText (1, "1k");
    sweepEnds (p, lo, hi);
    check ("Offset 19.1 %, 20 Hz og Sweep 0 %, skriv top 1 kHz", near (lo, 20.0) && near (hi, 1000.0), lo, hi);
    // For bredt med Offset: 20 Hz - 20 kHz kan ikke vises med Offset 19.1 % -> bunden (uroert) staar, toppen saa hoejt som muligt
    sweepEnds (p, lo0, hi0);
    ed.applySweepText (1, "20k");
    sweepEnds (p, lo, hi);
    const double maxSpan = (0.809 + 1.0) * 0.5 * std::log2 (1000.0);
    check ("Offset 19.1 %, skriv top 20 kHz (for bredt: bunden staar)", near (lo, lo0) && std::abs (std::log2 (hi / lo) - maxSpan) < 0.002, lo, hi);

    // Traek uden Offset: bund 50 px ned = halv frekvens, top 50 px op = dobbelt, Shift 50 px = 2^0.1; den anden staar
    setParam (p, "offset", 0.f); setParam (p, "freq", 600.f); setParam (p, "amount", 30.f);
    sweepEnds (p, lo0, hi0);
    ed.testSweepDrag (0, 50.f, false);
    sweepEnds (p, lo, hi);
    check ("traek bund 50 px ned (halv)", near (lo, lo0 * 0.5) && near (hi, hi0), lo, hi);
    sweepEnds (p, lo0, hi0);
    ed.testSweepDrag (1, -50.f, false);
    sweepEnds (p, lo, hi);
    check ("traek top 50 px op (dobbelt)", near (hi, hi0 * 2.0) && near (lo, lo0), lo, hi);
    sweepEnds (p, lo0, hi0);
    ed.testSweepDrag (1, -50.f, true);
    sweepEnds (p, lo, hi);
    check ("traek top 50 px op med Shift (2^0.1)", near (hi, hi0 * std::exp2 (0.1)) && near (lo, lo0), lo, hi);
    // Traek forbi den anden ende: bunden kan ikke komme over toppen (intet sker)
    sweepEnds (p, lo0, hi0);
    ed.testSweepDrag (0, -500.f, false);
    sweepEnds (p, lo, hi);
    check ("traek bund forbi toppen (intet sker)", near (lo, lo0) && near (hi, hi0), lo, hi);
    // Eet tal (Sweep 0): traek op = toppen
    setParam (p, "freq", 600.f); setParam (p, "amount", 0.f);
    ed.testSweepDrag (1, -50.f, false);
    sweepEnds (p, lo, hi);
    check ("Sweep 0 %, traek op (toppen)", near (lo, 600.0) && near (hi, 1200.0), lo, hi);

    setParam (p, "offset", 0.f); setParam (p, "freq", 600.f); setParam (p, "amount", 30.f);
    return ok;
}

// 1.13 (brugeren 2026-11-13): "Pilen, der indikerer Frequency Middle skal være i midten af de to yderkanter i alle
// tilfælde, hvor det er muligt". Uden Offset: er der plads, ligger Frequency midt i Sweep-omraadet; ellers er omraadet
// skubbet ind til en ende med samme bredde (Sweep * log2 1000 oktaver). Og baandet klippes ikke ved 20 Hz / 20 kHz
// ("som hvis ikke den ende kant rører ved en ende af spektrummet"): ved Sweep 100 % og Band Size 1 okt naar kanterne
// 14.1 Hz og 28.3 kHz.
static bool centreTest (DopplerSatProcessor& p)
{
    bool ok = true;
    int centred = 0, pushed = 0;
    const double full = std::log2 (1000.0);
    setParam (p, "offset", 0.f);
    for (float f : { 20.f, 40.f, 100.f, 300.f, 600.f, 2000.f, 8000.f, 15000.f, 20000.f })
        for (float s : { 0.f, 10.f, 30.f, 50.f, 80.f, 100.f })
        {
            setParam (p, "freq", f); setParam (p, "amount", s);
            const double fr = p.apvts.getRawParameterValue ("freq")->load(), sr = p.apvts.getRawParameterValue ("amount")->load();
            double lo, hi;
            sweepEnds (p, lo, hi);
            const double W = sr * 0.01 * full;
            const bool fits = std::log2 (fr / 20.0) >= 0.5 * W - 1.0e-6 && std::log2 (20000.0 / fr) >= 0.5 * W - 1.0e-6;
            bool pass = std::abs (std::log2 (hi / lo) - W) < 0.002;
            if (fits) { pass = pass && std::abs (std::log2 (std::sqrt (lo * hi) / fr)) < 0.002; ++centred; }
            else      { pass = pass && (std::abs (lo / 20.0 - 1.0) < 0.001 || std::abs (hi / 20000.0 - 1.0) < 0.001); ++pushed; }
            if (! pass)
            {
                ok = false;
                std::printf ("    FEJL: %.0f Hz, Sweep %.0f %%: %.1f - %.1f Hz\n", f, s, lo, hi);
            }
        }
    std::printf ("  pilen i midten: %d tilfaelde i midten, %d skubbet ind med samme bredde  %s\n", centred, pushed, ok ? "OK" : "FEJL");

    setParam (p, "freq", 600.f); setParam (p, "amount", 100.f); setParam (p, "width", 1.f);
    double a, b, c, d;
    p.bandEdgesFor (-1.0, a, b);
    p.bandEdgesFor ( 1.0, c, d);
    const bool noClip = std::abs (a / 14.142 - 1.0) < 0.005 && std::abs (d / 28284.0 - 1.0) < 0.005;
    std::printf ("  baandet ved Sweep 100 %%, Band Size 1 okt: %.1f Hz - %.1f Hz og %.0f Hz - %.0f Hz (glider ud over enderne)  %s\n",
                 a, b, c, d, noClip ? "OK" : "FEJL");
    setParam (p, "freq", 600.f); setParam (p, "amount", 30.f);
    return ok && noClip;
}

static void run (DopplerSatProcessor& p, double seconds)
{
    static double t = 0.0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random r (1);
    for (int blk = 0; blk < (int) (seconds * 44100.0 / 512.0); ++blk)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / 44100.0)
        {
            const float s = (float) (0.25 * std::sin (2 * 3.14159265 * 110 * t) + 0.15 * std::sin (2 * 3.14159265 * 440 * t)
                                     + 0.05 * (r.nextFloat() * 2.f - 1.f));
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
    }
}

static void save (juce::Component& c, const juce::File& f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds());
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("  %s\n", f.getFileName().toRawUTF8());
}

// 1.7: naar baandet helt ud i Sweep-omraadets kanter ved hoej Rate? (brugeren 2026-11-10: "Uanset rate (speed), skal
// båndet bevæge sig ud helt ud i kanterne af den valgte range"). Baandets midte maales hvert stykke (32 samples).
static bool reachTest()
{
    bool ok = true;
    // 1.11: Sweep 100 % = hele spektret. Maalet er midten af baandets kanter ved LFO -1 / +1 (kanterne klippes ved
    // 20 Hz og 20 kHz, saa den geometriske midte af de klippede kanter ligger lidt indenfor)
    std::printf ("  raekkevidde (Frequency 600 Hz, Sweep 100 %% = 20 Hz - 20 kHz, Band Size 0.5 okt):\n");
    for (int mode = 0; mode < 3; ++mode)
        for (int shape = 0; shape < 4; ++shape)
            for (float rate : { 1.f, 20.f })
            {
                DopplerSatProcessor p;
                p.setPlayConfigDetails (2, 2, 44100.0, 32);
                p.prepareToPlay (44100.0, 32);
                setParam (p, "freq", 600.f); setParam (p, "amount", 100.f); setParam (p, "width", 0.5f);
                setParam (p, "mode", (float) mode); setParam (p, "shape", (float) shape); setParam (p, "rate", rate);
                juce::AudioBuffer<float> buf (2, 32);
                juce::MidiBuffer midi;
                double lo = 1.0e9, hi = 0.0;
                const int blocks = (int) ((rate < 5.f ? 6.0 : 2.0) * 44100.0 / 32.0);
                for (int b = 0; b < blocks; ++b)
                {
                    buf.clear();
                    p.processBlock (buf, midi);
                    if (b < blocks / 3) continue;                          // foerst en tredjedel opvarmning
                    const double c = std::sqrt (p.bandLoNow.load() * p.bandHiNow.load());   // midten (geometrisk)
                    lo = std::min (lo, c); hi = std::max (hi, c);
                }
                // Random naar sjaeldent helt ud (tilfaeldige maal); de andre skal naa inden for 0.1 oktav
                double e1, e2, e3, e4;
                p.bandEdgesFor (-1.0, e1, e2);
                p.bandEdgesFor ( 1.0, e3, e4);
                const double loOct = std::log2 (lo / std::sqrt (e1 * e2)), hiOct = std::log2 (std::sqrt (e3 * e4) / hi);
                const bool pass = shape == 3 || (loOct < 0.1 && hiOct < 0.1);
                ok = ok && pass;
                static const char* sn[] = { "Sine", "Triangle", "Square", "Random" };
                std::printf ("    %2d dB/oct %-8s %4.0f Hz: midten %6.1f Hz - %6.0f Hz  %s\n", 24 << mode, sn[shape], rate, lo, hi,
                             pass ? "OK" : "NAAR IKKE UD");
            }
    return ok;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[2]) == "--reach") return reachTest() ? 0 : 1;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    DopplerSatProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    run (proc, 1.3);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler1.png"));

    // 1.6: Band Size 4 okt -> Sweep-displayet og det skraverede omraade skal vaere de samme som i doppler1
    setParam (proc, "width", 4.f);
    run (proc, 0.5);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler6_bandsize4.png"));
    setParam (proc, "width", 1.f);

    // 1.6: Pause -> LFO'ens fase og baandet staar stille
    setParam (proc, "pause", 1.f);
    run (proc, 0.2);
    const double ph0 = proc.lfoPhaseNow.load(), lo0 = proc.bandLoNow.load();
    run (proc, 1.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    const bool still = proc.lfoPhaseNow.load() == ph0 && proc.bandLoNow.load() == lo0;
    std::printf ("  pause: fase %.4f -> %.4f, baandets bund %.1f -> %.1f Hz  %s\n", ph0, proc.lfoPhaseNow.load(), lo0,
                 proc.bandLoNow.load(), still ? "staar stille OK" : "FEJL");
    save (*ed, dir.getChildFile ("doppler7_pause.png"));
    setParam (proc, "pause", 0.f);

    // 1.8's mockups (doppler8-10) er vaek: 1.9 har layoutet fra doppler9_mockup_balanced (doppler1 viser det)

    // 1.10: skriv i Sweep-displayet. Foerst regnestykket mange steder, saa den rigtige vej gennem feltet (Enter) med
    // billeder: feltet aabent over bunden med "180" (doppler8_sweep_edit) og bagefter (doppler9_sweep_typed)
    bool typeOk = true;
    if (auto* de = dynamic_cast<DopplerSatEditor*> (ed.get()))
    {
        typeOk = sweepTypeTest (*de, proc);
        typeOk = centreTest (proc) && typeOk;                   // 1.13
        typeOk = bugAndDragTest (*de, proc) && typeOk;          // 1.14
        double lo0, hi0;
        sweepEnds (proc, lo0, hi0);
        de->testBeginSweepEdit (0, "180");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        save (*ed, dir.getChildFile ("doppler8_sweep_edit.png"));
        juce::TextEditor* te = nullptr;
        for (auto* c : ed->getChildren())
            if (c->getName() == "Sweep display" && c->getNumChildComponents() > 0)
                te = dynamic_cast<juce::TextEditor*> (c->getChildComponent (0));
        if (te != nullptr && te->onReturnKey) te->onReturnKey();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        double lo1, hi1;
        sweepEnds (proc, lo1, hi1);
        const bool viaField = te != nullptr && std::abs (lo1 / 180.0 - 1.0) < 0.005 && std::abs (hi1 / hi0 - 1.0) < 0.003;
        std::printf ("  feltet: bund %.1f -> %.1f Hz (skrev 180), top %.1f -> %.1f Hz  %s\n", lo0, lo1, hi0, hi1, viaField ? "OK" : "FEJL");
        typeOk = typeOk && viaField;
        save (*ed, dir.getChildFile ("doppler9_sweep_typed.png"));
        setParam (proc, "freq", 600.f); setParam (proc, "amount", 50.f);

        // Mockup (brugeren 2026-11-13): "elementerne i GUI er lidt mere parallelle med hinanden [...] Lav et mockup".
        // Foer og efter i samme tilstand (doppler10_align_before / doppler11_mockup_aligned)
        setParam (proc, "amount", 30.f);
        run (proc, 0.3);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        save (*ed, dir.getChildFile ("doppler10_align_before.png"));
        de->setMockupLayout (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        save (*ed, dir.getChildFile ("doppler11_mockup_aligned.png"));
        de->setMockupLayout (0);
        setParam (proc, "amount", 50.f);
    }

    setParam (proc, "shape", 2.f);  setParam (proc, "offset", 40.f); setParam (proc, "sync", 1.f);
    setParam (proc, "mode", 0.f);   setParam (proc, "amount", 80.f);
    run (proc, 0.7);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler2.png"));

    setParam (proc, "shape", 3.f);  setParam (proc, "offset", -30.f); setParam (proc, "sync", 0.f);
    setParam (proc, "mode", 2.f);                          // 1.2: 96 dB/oct
    setParam (proc, "freq", 2000.f);  setParam (proc, "width", 2.5f);
    setParam (proc, "rate", 2.f);
    for (int i = 0; i < 60; ++i)                          // Random-sporet fyldes af editorens timer
    {
        run (proc, 0.034);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    save (*ed, dir.getChildFile ("doppler3.png"));

    // 1.1: hoejreklik paa en knap / drejeknap -> automationsmenuen, og kontrollen selv reagerer IKKE
    auto rightClick = [] (juce::Component& c)
    {
        const auto pos = c.getLocalBounds().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), pos,
                                  juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), 0.f, 0.f, 0.f, 0.f, 0.f,
                                  &c, &c, now, pos, now, 1, false);
        c.mouseDown (e);
        c.mouseUp (e);
    };
    bool menuOk = true;
    for (auto* c : ed->getChildren())
    {
        auto* tb = dynamic_cast<juce::ToggleButton*> (c);
        auto* sl = dynamic_cast<juce::Slider*> (c);
        if ((tb == nullptr || tb->getButtonText() != "Bypass") && (sl == nullptr || ! sl->isVisible() || sl->getTextValueSuffix() != " Hz"
                                                                    || sl->getValue() < 100.0))
            continue;
        const auto& desk = juce::Desktop::getInstance();
        const int   before = desk.getNumComponents();
        const bool  state0 = tb != nullptr && tb->getToggleState();
        const double v0    = sl != nullptr ? sl->getValue() : 0.0;
        rightClick (*c);
        // Straks: JUCE lukker selv menuen igen, naar programmet ikke er i forgrunden (det er en konsol-test aldrig)
        const bool opened = desk.getNumComponents() > before;
        const bool still  = tb != nullptr ? tb->getToggleState() == state0 : sl->getValue() == v0;
        menuOk = menuOk && opened && still;
        std::printf ("  hoejreklik paa %s: menu %s, kontrollen %s\n", tb != nullptr ? "Bypass" : "Frequency",
                     opened ? "aabnet" : "IKKE aabnet", still ? "uaendret" : "AENDRET");
        if (opened && tb != nullptr)
            if (auto* menu = desk.getComponent (desk.getNumComponents() - 1))
                save (*menu, dir.getChildFile ("doppler5_menu.png"));
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    }
    std::printf (menuOk ? "  hoejreklik OK\n" : "  hoejreklik FEJL\n");

    // 1.1: "?" ved de to 0 ms-tilstande -> forklaringsboksen
    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getName() == "?") b->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    save (*ed, dir.getChildFile ("doppler4_help.png"));

    ed.reset();
    std::printf (typeOk ? "  skriv i Sweep-displayet OK\n" : "  skriv i Sweep-displayet FEJL\n");
    return typeOk ? 0 : 1;
}
