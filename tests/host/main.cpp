// Roeg-test for Doppler Saturator: indlaes VST3'en som en vaert, koer lyd gennem alle former og begge
// tilstande, og maal at pitchen aendres (Doppler), naar baandet flyttes - og ikke, naar Amount er 0.
//
//   HostSmoke.exe "<ny.vst3>"                             roeg-test, pitch-tabel og CPU
//   HostSmoke.exe "<ny.vst3>" --compare "<gammel.vst3>"   samme lyd? (den gamle med Drive 0 %)
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>

static juce::AudioProcessorParameter* find (juce::AudioPluginInstance& inst, const char* name)
{
    for (auto* p : inst.getParameters())
        if (p->getName (64) == name) return p;
    return nullptr;
}

// Saet en parameter til en RIGTIG vaerdi (Hz, %, valg nr.). En VST3-vaerts parametre er ikke RangedAudioParameter,
// saa vaerdien skal regnes om til 0..1. (Foer 1.1 blev den raa vaerdi sendt som 0..1: Frequency 300 blev 20 kHz,
// Width 1 blev 6 okt og Rate 1 blev 20 Hz - se FREMDRIFT 1.1.)
//   valg / til-fra (diskrete): nr. / (antal - 1)
//   kontinuerlige: soeg den 0..1-vaerdi, hvis tekst (pluginnets egen) er v (vaerdierne stiger med 0..1)
static float normFor (juce::AudioProcessorParameter* p, float v)
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) return r->convertTo0to1 (v);
    if (p->isDiscrete() || p->isBoolean())
    {
        const int n = juce::jmax (2, p->getNumSteps());
        return juce::jlimit (0.f, 1.f, v / (float) (n - 1));
    }
    float lo = 0.f, hi = 1.f;
    for (int i = 0; i < 40; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        if (p->getText (mid, 64).getFloatValue() < v) lo = mid; else hi = mid;
    }
    return 0.5f * (lo + hi);
}

static void setReal (juce::AudioProcessorParameter* p, float v)
{
    if (p == nullptr) return;
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) r->setValueNotifyingHost (r->convertTo0to1 (v));
    else p->setValueNotifyingHost (normFor (p, v));
}

static std::unique_ptr<juce::AudioPluginInstance> load (const char* path)
{
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, juce::String (path));
    if (descs.isEmpty()) { std::printf ("FEJL: fandt intet plugin i %s\n", path); return nullptr; }
    juce::String err;
    auto inst = fmt.createInstanceFromDescription (*descs[0], 44100.0, 512, err);
    if (inst == nullptr) std::printf ("FEJL: %s\n", err.toRawUTF8());
    return inst;
}

// Koer en sinustone gennem pluginnet og maal frekvensen (opadgaaende nulgennemgange, gennemsnit over `win` perioder).
// Giver den stoerste afvigelse fra tonen i cent og om lyden er endelig.
static double pitchDeviation (juce::AudioPluginInstance& inst, double f0, double seconds, bool& bad, double& peak, int win = 8)
{
    const double fs = 44100.0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::vector<double> crossings;
    double prev = 0.0, t = 0.0;
    long n = 0;
    const long total = (long) (seconds * fs), skip = (long) (0.5 * fs);
    while (n < total)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / fs)
        {
            const float s = (float) (0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * f0 * t));
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        inst.processBlock (buf, midi);
        for (int i = 0; i < 512; ++i, ++n)
        {
            const double v = buf.getSample (0, i);
            if (! std::isfinite (v)) bad = true;
            peak = std::max (peak, std::abs (v));
            if (n > skip && prev < 0.0 && v >= 0.0) crossings.push_back ((double) n - 1.0 + prev / (prev - v));
            prev = v;
        }
    }
    double worst = 0.0;
    for (size_t i = (size_t) win; i < crossings.size(); ++i)
    {
        const double f = win * fs / (crossings[i] - crossings[i - (size_t) win]);
        worst = std::max (worst, std::abs (1200.0 * std::log2 (f / f0)));
    }
    return worst;
}

// ---------------------------------------------------------------------------
//  --compare: samme indstillinger i begge, samme lyd ind; hvor stor er forskellen ud?
// ---------------------------------------------------------------------------
static int compare (juce::AudioPluginInstance& a, juce::AudioPluginInstance& b)
{
    juce::AudioPluginInstance* both[] = { &a, &b };
    auto setBoth = [&] (const char* name, float v) { for (auto* i : both) setReal (find (*i, name), v); };
    setReal (find (b, "Drive"), 0.f);                       // den gamle (1.0): Drive 0 %
    for (auto* i : both)
    {
        i->setPlayConfigDetails (2, 2, 44100.0, 512);
        i->prepareToPlay (44100.0, 512);
    }
    std::printf ("  latens %d / %d samples\n", a.getLatencySamples(), b.getLatencySamples());

    struct Step { const char* what; float freq, amount, width, rate, shape, offset, mode, sync, dryWet, out, bypass; };
    const Step steps[] = {
        { "standard (Sine 0,5 Hz, 48)",      600.f,  50.f, 1.f,  0.5f, 0.f,   0.f, 1.f, 0.f, 100.f, 0.f, 0.f },
        { "Sine 1 Hz, Amount 100 %",         300.f, 100.f, 1.f,  1.f,  0.f,   0.f, 1.f, 0.f, 100.f, 0.f, 0.f },
        { "skift til 24 dB",                 300.f, 100.f, 1.f,  1.f,  0.f,   0.f, 0.f, 0.f, 100.f, 0.f, 0.f },
        { "Triangle 3 Hz, offset +60",      1500.f,  80.f, 0.5f, 3.f,  1.f,  60.f, 0.f, 0.f, 100.f, 0.f, 0.f },
        { "Square 20 Hz (fartgraensen)",     800.f, 100.f, 0.3f, 20.f, 2.f,   0.f, 0.f, 0.f, 100.f, 0.f, 0.f },
        { "Square 20 Hz, 48",                800.f, 100.f, 0.3f, 20.f, 2.f,   0.f, 1.f, 0.f, 100.f, 0.f, 0.f },
        { "Random 2 Hz, offset -40",        5000.f, 100.f, 2.5f, 2.f,  3.f, -40.f, 1.f, 0.f, 100.f, 0.f, 0.f },
        { "Sync 1/8T",                       400.f,  70.f, 1.f,  1.f,  0.f,   0.f, 1.f, 1.f, 100.f, 0.f, 0.f },
        { "Dry/Wet 50 % (phaser)",           400.f, 100.f, 1.5f, 0.3f, 0.f,   0.f, 1.f, 0.f,  50.f, 0.f, 0.f },
        { "Output +6 dB",                    400.f, 100.f, 1.5f, 0.3f, 0.f,   0.f, 1.f, 0.f, 100.f, 6.f, 0.f },
        { "Bypass til",                      400.f, 100.f, 1.5f, 0.3f, 0.f,   0.f, 1.f, 0.f, 100.f, 0.f, 1.f },
        { "Bypass fra igen (opvarmning)",    400.f, 100.f, 1.5f, 0.3f, 0.f,   0.f, 1.f, 0.f, 100.f, 0.f, 0.f },
        { "hoejt baand (12 kHz, 6 okt)",   12000.f, 100.f, 6.f,  0.7f, 0.f,   0.f, 0.f, 0.f, 100.f, 0.f, 0.f },
        { "lavt baand (40 Hz)",               40.f, 100.f, 0.1f, 0.2f, 0.f,   0.f, 1.f, 0.f, 100.f, 0.f, 0.f },
    };

    juce::AudioBuffer<float> ba (2, 512), bb (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (7);
    double t = 0.0, worstAll = 0.0;
    for (const auto& s : steps)
    {
        setBoth ("Frequency", s.freq); setBoth ("Amount", s.amount); setBoth ("Width", s.width);
        setBoth ("Rate", s.rate);      setBoth ("LFO Shape", s.shape); setBoth ("Offset", s.offset);
        setBoth ("Mode", s.mode);      setBoth ("Sync", s.sync);      setBoth ("Sync Rate", 12.f);
        setBoth ("Dry/Wet", s.dryWet); setBoth ("Output Gain", s.out); setBoth ("Bypass", s.bypass);
        double worst = 0.0, peak = 0.0;
        for (int blk = 0; blk < 90; ++blk)
        {
            for (int i = 0; i < 512; ++i, t += 1.0 / 44100.0)
            {
                const float n = 0.05f * (rng.nextFloat() * 2.f - 1.f);
                const float l = (float) (0.3 * std::sin (2 * 3.14159265 * 200 * t) + 0.2 * std::sin (2 * 3.14159265 * 3000 * t)) + n;
                const float r = (float) (0.3 * std::sin (2 * 3.14159265 * 130 * t) + 0.1 * std::sin (2 * 3.14159265 * 9000 * t)) - n;
                ba.setSample (0, i, l); ba.setSample (1, i, r);
                bb.setSample (0, i, l); bb.setSample (1, i, r);
            }
            a.processBlock (ba, midi);
            b.processBlock (bb, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                {
                    worst = std::max (worst, (double) std::abs (ba.getSample (c, i) - bb.getSample (c, i)));
                    peak  = std::max (peak, (double) std::abs (ba.getSample (c, i)));
                }
        }
        worstAll = std::max (worstAll, worst);
        std::printf ("  %-32s peak %.3f, stoerste forskel %.3g%s\n", s.what, peak, worst, worst == 0.0 ? " (bit-identisk)" : "");
    }
    const bool same = worstAll < 1.0e-6;
    std::printf (same ? "SAMME LYD\n" : "FORSKELLIG LYD\n");
    return same ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("brug: HostSmoke <sti til .vst3> [--compare <gammel .vst3>]\n"); return 2; }
    auto inst = load (argv[1]);
    if (inst == nullptr) return 1;
    std::printf ("Indlaest: %s, %d parametre\n", inst->getName().toRawUTF8(), inst->getParameters().size());

    // --bench: CPU ved standardindstillingerne, med Bypass og med 1024-blokke (hvad koster hvad?)
    if (argc >= 3 && juce::String (argv[2]) == "--bench")
    {
        for (int block : { 512, 64, 1024 })
            for (int byp = 0; byp < 2; ++byp)
            {
                inst->releaseResources();
                inst->setPlayConfigDetails (2, 2, 44100.0, block);
                inst->prepareToPlay (44100.0, block);
                setReal (find (*inst, "Bypass"), (float) byp);
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer midi;
                juce::Random rng (3);
                const int blocks = (int) (20.0 * 44100.0 / block);
                double ms = 0.0;
                for (int b = 0; b < blocks; ++b)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < block; ++i) buf.setSample (c, i, 0.3f * (rng.nextFloat() * 2.f - 1.f));
                    const auto t0 = juce::Time::getMillisecondCounterHiRes();
                    inst->processBlock (buf, midi);
                    ms += juce::Time::getMillisecondCounterHiRes() - t0;
                }
                std::printf ("  blok %4d, bypass %d: CPU %.2f %% af een kerne\n", block, byp, 100.0 * ms / 20000.0);
            }
        return 0;
    }

    if (argc >= 4 && juce::String (argv[2]) == "--compare")
    {
        auto old = load (argv[3]);
        if (old == nullptr) return 1;
        std::printf ("Sammenligner med: %s (Drive 0 %%)\n", old->getName().toRawUTF8());
        const int r = compare (*inst, *old);
        inst->releaseResources(); old->releaseResources();
        return r;
    }

    // Alle parametre skal kunne automatiseres (brugeren 2026-11-04)
    bool ok = true;
    for (auto* p : inst->getParameters())
    {
        std::printf ("  parameter %-12s automatiserbar: %s\n", p->getName (64).toRawUTF8(), p->isAutomatable() ? "ja" : "NEJ");
        ok = ok && p->isAutomatable();
    }
    if (auto* bp = inst->getBypassParameter()) std::printf ("  DAW'ens bypass: %s\n", bp->getName (64).toRawUTF8());
    // Omregningen virker? (Frequency 300 -> "300.0", Rate 1 -> "1.00", Output -6 -> "-6.00")
    const std::pair<const char*, float> checks[] = { { "Frequency", 300.f }, { "Rate", 1.f }, { "Width", 1.f },
                                                     { "Output Gain", -6.f }, { "Offset", -60.f } };
    for (auto [name, v] : checks)
        if (auto* p = find (*inst, name))
        {
            const float got = p->getText (normFor (p, v), 64).getFloatValue();
            const bool good = std::abs (got - v) <= 0.02f * std::max (1.f, std::abs (v));
            ok = ok && good;
            std::printf ("  %s %.2f -> plugin viser %s  %s\n", name, v, p->getText (normFor (p, v), 64).toRawUTF8(), good ? "OK" : "FEJL");
        }

    auto* mode = find (*inst, "Mode");     auto* freq = find (*inst, "Frequency"); auto* amount = find (*inst, "Amount");
    auto* width = find (*inst, "Width");   auto* drive = find (*inst, "Drive");    auto* rate = find (*inst, "Rate");
    auto* shape = find (*inst, "LFO Shape"); auto* offset = find (*inst, "Offset"); auto* sync = find (*inst, "Sync");
    auto* dryWet = find (*inst, "Dry/Wet"); auto* bypass = find (*inst, "Bypass");
    if (! mode || ! freq || ! amount || ! width || ! rate || ! shape || ! offset || ! sync || ! dryWet || ! bypass)
    {
        std::printf ("FEJL: parametre mangler\n");
        return 1;
    }
    if (drive != nullptr || find (*inst, "Distortion") != nullptr)
        std::printf ("  (Drive / Distortion findes: gammel version)\n");

    inst->setPlayConfigDetails (2, 2, 44100.0, 512);
    inst->prepareToPlay (44100.0, 512);
    std::printf ("  latens %d samples\n", inst->getLatencySamples());
    bool bad = false;
    double peak = 0.0;

    // 1) Pitch: tone 300 Hz, baandet om 300 Hz (1 oktav bredt), LFO 1 Hz sinus (ren Doppler)
    setReal (freq, 300.f); setReal (width, 1.f); setReal (rate, 1.f); setReal (shape, 0.f); setReal (offset, 0.f);
    setReal (drive, 0.f); setReal (dryWet, 100.f);
    // 1.2: tre tilstande (24 / 48 / 96); 1.0 og 1.1 har to
    const int nModes = juce::jlimit (2, 3, mode->getNumSteps());
    static const char* modeName[] = { "0 ms 24", "0 ms 48", "0 ms 96" };
    for (int m = 0; m < nModes; ++m)
    {
        setReal (mode, (float) m);
        setReal (amount, 0.f);
        const double still = pitchDeviation (*inst, 300.0, 3.0, bad, peak);
        setReal (amount, 100.f);
        const double moving = pitchDeviation (*inst, 300.0, 4.0, bad, peak);
        const bool pass = still < 1.0 && moving > 20.0;
        ok = ok && pass;
        std::printf ("  %s: pitch-afvigelse Amount 0 %%: %.2f cent, Amount 100 %% (1 Hz): %.0f cent  %s\n",
                     modeName[m], still, moving, pass ? "OK" : "FEJL");
    }

    // 2) Pitch-tabel til "?"-teksten: 24 / 48 / 96 ved forskellige hastigheder og toner (Amount 100 %, Width 1 okt,
    //    baandet om tonen, gennemsnit over 4 perioder)
    std::printf ("  Pitch-tabel (cent, Amount 100 %%, Width 1 okt, baandet om tonen):\n");
    std::printf ("    %-22s %8s %8s %8s %7s %7s\n", "", "24 dB", "48 dB", "96 dB", "48/24", "96/48");
    struct Row { double tone; float rate; };
    const Row rows[] = { { 300.0, 0.25f }, { 300.0, 1.f }, { 300.0, 3.f }, { 300.0, 6.f }, { 300.0, 12.f },
                         { 100.0, 1.f }, { 1000.0, 1.f }, { 3000.0, 1.f }, { 8000.0, 1.f } };
    for (const auto& row : rows)
    {
        setReal (freq, (float) row.tone); setReal (rate, row.rate); setReal (amount, 100.f);
        double c[3] = { 0.0, 0.0, 0.0 };
        for (int m = 0; m < nModes; ++m)
        {
            setReal (mode, (float) m);
            c[m] = pitchDeviation (*inst, row.tone, row.rate < 0.5f ? 9.0 : 4.5, bad, peak, 4);
        }
        std::printf ("    tone %5.0f Hz, %5.2f Hz %8.0f %8.0f %8.0f %7.2f %7.2f\n", row.tone, row.rate, c[0], c[1], c[2],
                     c[1] / std::max (c[0], 1.0e-9), c[2] / std::max (c[1], 1.0e-9));
    }
    setReal (freq, 300.f); setReal (rate, 1.f);

    // 3) Alle former, offset, sync, hurtig rate, dry/wet og bypass: ingen nedbrud eller NaN
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double t = 0.0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    long samples = 0;
    // alle former x alle tilstande x Hz / Sync (to tilstande: 16 trin som foer 1.2)
    for (int step = 0; step < 8 * nModes; ++step)
    {
        const bool stepSync = (step / (4 * nModes)) % 2 == 1;
        setReal (shape, (float) (step % 4));
        setReal (mode, (float) ((step / 4) % nModes));
        setReal (offset, step % 3 == 0 ? 60.f : step % 3 == 1 ? -60.f : 0.f);
        setReal (sync, stepSync ? 1.f : 0.f);
        setReal (rate, step % 2 ? 20.f : 0.05f);
        setReal (amount, 100.f);
        setReal (dryWet, step == 5 ? 50.f : 100.f);
        setReal (bypass, step == 6 ? 1.f : 0.f);
        double stepPeak = 0.0;
        for (int blk = 0; blk < 120; ++blk)
        {
            for (int i = 0; i < 512; ++i, t += 1.0 / 44100.0)
            {
                const float s = (float) (0.4 * std::sin (2 * 3.14159265 * 200 * t) + 0.2 * std::sin (2 * 3.14159265 * 3000 * t));
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            inst->processBlock (buf, midi);
            samples += 512;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) bad = true;
                    stepPeak = std::max (stepPeak, (double) std::abs (v));
                }
        }
        // Niveauet maa ikke hoppe (indgangen har peak 0.6): foer fartgraensen paa kanterne gav Square og 20 Hz op til 8.8
        const bool levelOk = stepPeak < 1.25;
        ok = ok && levelOk;
        std::printf ("  form %d, tilstand %d, offset %+d, %s %s: peak %.2f  %s\n", step % 4, (step / 4) % nModes,
                     step % 3 == 0 ? 60 : step % 3 == 1 ? -60 : 0, stepSync ? "sync" : "Hz", step % 2 ? "hurtig" : "langsom",
                     stepPeak, levelOk ? "OK" : "FEJL");
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - start;
    std::printf ("  CPU: %.2f %% af een kerne (%.1f s lyd paa %.0f ms)\n", 100.0 * ms / (1000.0 * samples / 44100.0),
                 samples / 44100.0, ms);

    juce::MemoryBlock mb;
    inst->getStateInformation (mb);
    inst->setStateInformation (mb.getData(), (int) mb.getSize());
    if (auto* ed = inst->createEditorIfNeeded()) { delete ed; std::printf ("  editor oprettet og lukket\n"); }
    inst->releaseResources();
    inst.reset();
    if (bad) std::printf ("FEJL: NaN/inf i lyden\n");
    std::printf (ok && ! bad ? "OK\n" : "DER ER FEJL\n");
    return ok && ! bad ? 0 : 1;
}
