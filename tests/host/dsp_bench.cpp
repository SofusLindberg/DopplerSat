// Hvad koster hvad i Doppler Saturator? DspBench.exe
// Maaler (uden vaert) hele processBlock og delene hver for sig: 4x oversampling, DopplerCore og setParams.
#include <JuceHeader.h>
#include "../../Source/PluginProcessor.h"

template <class F>
static double timeIt (F&& f, int reps)
{
    double best = 1.0e30;
    for (int r = 0; r < reps; ++r)                         // bedste af flere: mindst stoej fra resten af pc'en
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        f();
        best = std::min (best, juce::Time::getMillisecondCounterHiRes() - t0);
    }
    return best;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    constexpr double fs = 44100.0, seconds = 5.0;
    constexpr int block = 512, blocks = (int) (seconds * fs / block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    auto fill = [&] { for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) buf.setSample (c, i, 0.3f * (rng.nextFloat() * 2.f - 1.f)); };
    auto pct = [&] (double ms) { return 100.0 * ms / (1000.0 * seconds); };

    static const char* modeName[] = { "24 dB", "48 dB", "96 dB" };
    for (int mode = 0; mode < DopplerSatProcessor::kNumModes; ++mode)
    {
        DopplerSatProcessor proc;
        if (auto* p = proc.apvts.getParameter ("mode")) p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
        proc.setPlayConfigDetails (2, 2, fs, block);
        proc.prepareToPlay (fs, block);
        const double all = timeIt ([&] { for (int b = 0; b < blocks; ++b) { fill(); proc.processBlock (buf, midi); } }, 5);
        const double gen = timeIt ([&] { for (int b = 0; b < blocks; ++b) fill(); }, 5);
        std::printf ("%s: hele processBlock %.2f %% af een kerne\n", modeName[mode], pct (all - gen));

        // Delene hver for sig, i stykker paa 32 samples som i processBlock
        juce::dsp::Oversampling<float> os (2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
        os.initProcessing (block);
        const double tOs = timeIt ([&]
        {
            for (int b = 0; b < blocks; ++b)
            {
                fill();
                for (int s = 0; s < block; s += 32)
                {
                    juce::dsp::AudioBlock<float> ab (buf.getArrayOfWritePointers(), 2, (size_t) s, 32);
                    os.processSamplesUp (ab);
                    os.processSamplesDown (ab);
                }
            }
        }, 5) - gen;

        satdsp::DopplerCore core;
        core.prepare (fs * 4.0);
        std::vector<float> L (4 * 32), R (4 * 32);
        int k = 0;
        const double tSet = timeIt ([&]
        {
            for (int b = 0; b < blocks * block / 32; ++b, ++k)
                core.setParams (proc.buildEngineParams (300.0 * (1.0 + 0.001 * (k % 100)), 600.0));
        }, 5);
        const double tCore = timeIt ([&]
        {
            for (int b = 0; b < blocks * block / 32; ++b)
            {
                for (int i = 0; i < 128; ++i) { L[(size_t) i] = 0.3f * (rng.nextFloat() * 2.f - 1.f); R[(size_t) i] = L[(size_t) i]; }
                core.process (L.data(), R.data(), 128);
            }
        }, 5) - timeIt ([&]
        {
            for (int b = 0; b < blocks * block / 32; ++b)
                for (int i = 0; i < 128; ++i) { L[(size_t) i] = 0.3f * (rng.nextFloat() * 2.f - 1.f); R[(size_t) i] = L[(size_t) i]; }
        }, 5);
        std::printf ("   4x oversampling op + ned: %.2f %%,  DopplerCore (4x rate): %.2f %%,  setParams: %.2f %%\n",
                     pct (tOs), pct (tCore), pct (tSet));
    }

    // 1.9: hvor tit springes Smooth Peaks' reference-motor over (den er bit for bit som hovedmotoren)? Andelen maales
    // i de sidste `measure` sekunder; `jump` = Frequency 600 -> 2500 Hz efter 1 s (motorerne skilles og skal blive ens igen)
    auto skipShare = [&] (const char* what, int shape, float rate, float sweep, int mode, bool jump, bool pause, double measure,
                          bool silence = false, bool bypassBlip = false)
    {
        DopplerSatProcessor proc;
        auto set = [&] (const char* id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set ("shape", (float) shape); set ("rate", rate); set ("amount", sweep); set ("mode", (float) mode);
        set ("pause", pause ? 1.f : 0.f);
        proc.setPlayConfigDetails (2, 2, fs, block);
        proc.prepareToPlay (fs, block);
        const double total = 1.0 + 6.0 + measure;
        juce::int64 s0 = 0, r0 = 0;
        bool started = false;
        for (int b = 0; b < (int) (total * fs / block); ++b)
        {
            const double t = b * (double) block / fs;
            if (jump && t >= 1.0 && t < 1.0 + block / fs) set ("freq", 2500.f);
            if (t >= total - measure && ! started) { s0 = proc.refSkipped; r0 = proc.refRun; started = true; }
            fill();
            if (silence && t >= 2.0 && t < 5.0) buf.clear();             // stilhed 2-5 s (fx afspilningen stoppet)
            if (bypassBlip) set ("bypass", t >= 2.0 && t < 3.0 ? 1.f : 0.f);   // Bypass 2-3 s
            proc.processBlock (buf, midi);
        }
        const double sk = (double) (proc.refSkipped - s0), rn = (double) (proc.refRun - r0);
        std::printf ("  %-44s reference-motoren sprunget over i %5.1f %% af stykkerne\n", what, 100.0 * sk / std::max (1.0, sk + rn));
    };
    std::printf ("Smooth Peaks (1.9), de sidste 5 s:\n");
    skipShare ("Sine 0,5 Hz, Sweep 50 % (standard)",          0,  0.5f,  50.f, 1, false, false, 5.0);
    skipShare ("Sine 0,5 Hz, Sweep 100 %, 96 dB",             0,  0.5f, 100.f, 2, false, false, 5.0);
    skipShare ("Sine 2 Hz, Sweep 100 %",                      0,  2.f,  100.f, 1, false, false, 5.0);
    skipShare ("Sine 12 Hz, Sweep 100 %",                     0, 12.f,  100.f, 1, false, false, 5.0);
    skipShare ("Square 0,5 Hz, Sweep 50 %",                   2,  0.5f,  50.f, 1, false, false, 5.0);
    skipShare ("Sine 0,5 Hz, Frequency-spring efter 1 s",     0,  0.5f,  50.f, 1, true,  false, 5.0);
    skipShare ("Pause, Frequency-spring efter 1 s",           0,  0.5f,  50.f, 1, true,  true,  5.0);
    skipShare ("Frequency-spring efter 1 s, stilhed 2-5 s",   0,  0.5f,  50.f, 1, true,  false, 5.0, true);
    skipShare ("Frequency-spring efter 1 s, Bypass 2-3 s",    0,  0.5f,  50.f, 1, true,  false, 5.0, false, true);
    return 0;
}
