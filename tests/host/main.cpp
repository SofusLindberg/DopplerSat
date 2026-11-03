// Roeg-test for Doppler Saturator: indlaes VST3'en som en vaert, koer lyd gennem alle former og begge
// tilstande, og maal at pitchen aendres (Doppler), naar baandet flyttes - og ikke, naar Amount er 0.
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>

static juce::AudioProcessorParameter* find (juce::AudioPluginInstance& inst, const char* name)
{
    for (auto* p : inst.getParameters())
        if (p->getName (64) == name) return p;
    return nullptr;
}

static void setReal (juce::AudioProcessorParameter* p, float v)
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) r->setValueNotifyingHost (r->convertTo0to1 (v));
    else p->setValueNotifyingHost (v);
}

// Koer en sinustone gennem pluginnet og maal frekvensen (opadgaaende nulgennemgange, gennemsnit over 8 perioder).
// Giver den stoerste afvigelse fra tonen i cent og om lyden er endelig.
static double pitchDeviation (juce::AudioPluginInstance& inst, double f0, double seconds, bool& bad, double& peak)
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
    for (size_t i = 8; i < crossings.size(); ++i)
    {
        const double f = 8.0 * fs / (crossings[i] - crossings[i - 8]);
        worst = std::max (worst, std::abs (1200.0 * std::log2 (f / f0)));
    }
    return worst;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("brug: HostSmoke <sti til .vst3>\n"); return 2; }
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, juce::String (argv[1]));
    if (descs.isEmpty()) { std::printf ("FEJL: fandt intet plugin\n"); return 1; }
    juce::String err;
    auto inst = fmt.createInstanceFromDescription (*descs[0], 44100.0, 512, err);
    if (inst == nullptr) { std::printf ("FEJL: %s\n", err.toRawUTF8()); return 1; }
    std::printf ("Indlaest: %s, %d parametre\n", inst->getName().toRawUTF8(), inst->getParameters().size());

    auto* mode = find (*inst, "Mode");     auto* freq = find (*inst, "Frequency"); auto* amount = find (*inst, "Amount");
    auto* width = find (*inst, "Width");   auto* drive = find (*inst, "Drive");    auto* rate = find (*inst, "Rate");
    auto* shape = find (*inst, "LFO Shape"); auto* offset = find (*inst, "Offset"); auto* sync = find (*inst, "Sync");
    auto* dryWet = find (*inst, "Dry/Wet"); auto* bypass = find (*inst, "Bypass");
    if (! mode || ! freq || ! amount || ! width || ! drive || ! rate || ! shape || ! offset || ! sync || ! dryWet || ! bypass)
    {
        std::printf ("FEJL: parametre mangler\n");
        return 1;
    }

    inst->setPlayConfigDetails (2, 2, 44100.0, 512);
    inst->prepareToPlay (44100.0, 512);
    std::printf ("  latens %d samples\n", inst->getLatencySamples());
    bool bad = false, ok = true;
    double peak = 0.0;

    // 1) Pitch: tone 300 Hz, baandet om 300 Hz (1 oktav bredt), LFO 1 Hz sinus, 0 % drive (ren Doppler)
    setReal (freq, 300.f); setReal (width, 1.f); setReal (rate, 1.f); setReal (shape, 0.f); setReal (offset, 0.f);
    setReal (drive, 0.f); setReal (dryWet, 100.f);
    for (int m = 0; m < 2; ++m)
    {
        setReal (mode, (float) m);
        setReal (amount, 0.f);
        const double still = pitchDeviation (*inst, 300.0, 3.0, bad, peak);
        setReal (amount, 100.f);
        const double moving = pitchDeviation (*inst, 300.0, 4.0, bad, peak);
        setReal (drive, 100.f);
        const double withDrive = pitchDeviation (*inst, 300.0, 4.0, bad, peak);
        setReal (drive, 0.f);
        const bool pass = still < 1.0 && moving > 20.0;
        ok = ok && pass;
        std::printf ("  %s: pitch-afvigelse Amount 0 %%: %.2f cent, Amount 100 %% (1 Hz): %.0f cent, med 100 %% drive: %.0f cent  %s\n",
                     m == 0 ? "0 ms 24" : "0 ms 48", still, moving, withDrive, pass ? "OK" : "FEJL");
    }

    // 2) Alle former, offset, sync, hurtig rate, dry/wet og bypass: ingen nedbrud eller NaN
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double t = 0.0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    long samples = 0;
    for (int step = 0; step < 16; ++step)
    {
        setReal (shape, (float) (step % 4));
        setReal (mode, (float) ((step / 4) % 2));
        setReal (offset, step % 3 == 0 ? 60.f : step % 3 == 1 ? -60.f : 0.f);
        setReal (sync, step >= 8 ? 1.f : 0.f);
        setReal (rate, step % 2 ? 20.f : 0.05f);
        setReal (drive, (float) (step * 25));
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
        std::printf ("  form %d, tilstand %d, offset %+d, %s %s, drive %d %%: peak %.2f  %s\n", step % 4, (step / 4) % 2,
                     step % 3 == 0 ? 60 : step % 3 == 1 ? -60 : 0, step >= 8 ? "sync" : "Hz", step % 2 ? "hurtig" : "langsom",
                     step * 25, stepPeak, levelOk ? "OK" : "FEJL");
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - start;
    std::printf ("  CPU: %.1f %% af een kerne (%.1f s lyd paa %.0f ms)\n", 100.0 * ms / (1000.0 * samples / 44100.0),
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
