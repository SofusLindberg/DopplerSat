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
    return 0;
}
