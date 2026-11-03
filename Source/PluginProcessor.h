#pragma once
#include <JuceHeader.h>
#include "SatDsp.h"

// Doppler Saturator (brugeren 2026-11-03): "Dobbler-effekt (ptitch ændres, når man trækker båndet henover
// frekvenserne) i multiband saturation er faktisk super fedt. [...] Tag spektogrammet med et enkelt bånd med
// phase-indstilligerne "0 ms - 24 db/oct" og "0 ms - 48 db/oct" (ikke medtag linear phase), så vi kan lave en
// saturator, hvis eneste formål er at give brugeren mulighed for at bruge dobbler-effekten [...] kreativt."
//
// Lyden er 0 ms-motoren fra Multiband Saturator 3.17 (satdsp::Engine, SatDsp.h) med eet baand. En LFO flytter
// baandet hen over det valgte omraade, og naar dets delefiltre flytter sig, aendres pitchen (Doppler).
class DopplerSatProcessor : public juce::AudioProcessor
{
public:
    static constexpr float  minFreq     = 20.f;
    static constexpr float  maxFreq     = 20000.f;
    static constexpr float  minDb       = -80.f;
    static constexpr double kMaxSweepOct = 3.0;    // Amount 100 % = baandet flyttes +/- 3 oktaver
    static constexpr int    kBand       = 1;       // motorens baand, der flyttes (de andre er slukket)
    static constexpr int    kChunk      = 32;      // LFO'en og filtrene opdateres hver 32. sample (0.7 ms ved 44.1 kHz)
    // Kanterne flytter sig hoejst saa hurtigt (oktaver pr. sekund). Hurtigere gav niveauhop (maalt med eet baand,
    // tone 0.6 peak: 24 dB 80 okt/s 0.78, 48 dB 60 okt/s 1.25 og 100 okt/s 1.7). Ved grænsen hoejst ca. +1.4 dB.
    static constexpr double kMaxOctPerSec24 = 40.0, kMaxOctPerSec48 = 20.0;

    static juce::StringArray modeNames();         // index 0 = 0 ms 24 dB/oct, 1 = 0 ms 48 dB/oct
    static juce::StringArray shapeNames();        // Sine, Triangle, Square, Random
    static juce::StringArray syncNames();         // nodevaerdier til Rate i Sync
    static double            syncBeats (int index, double barBeats);   // laengden i fjerdedele
    static juce::StringArray distortionNames();

    // LFO'ens form ved fase 0..1 (foer offset), -1..1. rndA / rndB: tilfaeldige vaerdier ved denne og naeste periode.
    // squareW: Square's overgang som andel af perioden (bloed, saa den ikke klikker).
    static double lfoWave (int shape, double phase, double rndA, double rndB, double squareW);
    // Med offset: v = wave + offset, klippet til -1..1 (saa den bliver laengere paa toppen eller i bunden)
    static double lfoValue (int shape, double phase, double offset, double rndA, double rndB, double squareW);

    DopplerSatProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Spektrum-analysator: FFT af mono-summen. Audio-traaden skriver, editoren laeser.
    struct Analyzer
    {
        static constexpr int order = 11;
        static constexpr int size  = 1 << order;
        std::array<std::atomic<float>, size / 2 + 1> mags;

        Analyzer() { reset(); }
        void reset()
        {
            fifo.fill (0.f);
            idx = 0;
            for (auto& m : mags) m.store (-100.f);
        }
        void push (float s)
        {
            fifo[(size_t) idx] = s;
            if (++idx == size) { idx = 0; compute(); }
        }

    private:
        void compute()
        {
            std::copy (fifo.begin(), fifo.end(), data.begin());
            std::fill (data.begin() + size, data.end(), 0.f);
            window.multiplyWithWindowingTable (data.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (data.data());
            const float norm = 4.f / (float) size;      // Hann-vindue: en fuld sinus = 0 dB
            for (int i = 0; i <= size / 2; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (data[(size_t) i] * norm, -100.f);
                mags[(size_t) i].store (0.80f * mags[(size_t) i].load() + 0.20f * db);
            }
        }
        juce::dsp::FFT fft { order };
        juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
        std::array<float, size> fifo {};
        std::array<float, size * 2> data {};
        int idx = 0;
    };
    Analyzer inAnalyzer, outAnalyzer;

    // Baandets kanter for en LFO-vaerdi v (-1..1): midten flyttes v * Amount * 3 oktaver fra Frequency
    void bandEdgesFor (double v, double& lo, double& hi) const;
    // Motorens parametre med baandet mellem lo og hi (faelles for lyden og tegningen)
    satdsp::EngineParams buildEngineParams (double lo, double hi) const;
    double getProcessingRate() const;                 // 4x vaertens rate (oversampling er altid til)

    // Til editoren: LFO'ens fase og vaerdi lige nu, og hvor baandet er
    std::atomic<double> lfoPhaseNow { 0.0 }, lfoValueNow { 0.0 }, bandLoNow { 300.0 }, bandHiNow { 1200.0 };
    std::atomic<double> rndNowA { 0.0 }, rndNowB { 0.0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateLfoState (int numSamples);            // fasen ved blokkens start (Sync: fra vaertens position)
    void newRandom();

    std::atomic<float> *modeP = nullptr, *freqP = nullptr, *amountP = nullptr, *widthP = nullptr, *driveP = nullptr,
                       *typeP = nullptr, *rateP = nullptr, *syncP = nullptr, *syncRateP = nullptr, *shapeP = nullptr,
                       *offsetP = nullptr, *dryWetP = nullptr, *outP = nullptr, *bypassP = nullptr;

    satdsp::Engine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuf;

    double phase = 0.0;              // LFO'ens fase 0..1
    double cycle = 0.0;              // antal hele perioder (Sync: fra vaertens position), til Random
    double rndA = 0.0, rndB = 0.0;   // Random: vaerdien i denne og naeste periode
    juce::Random rng { 20261103 };
    double lfoRate = 0.5;            // Hz i den aktuelle blok
    bool   wasPlaying = false;

    int   maxBlock = 512;
    float lastOutGain = 1.f, lastDryWet = 1.f;
    double lastLo = 300.0, lastHi = 1200.0;
    double curLoOct = 0.0, curHiOct = 0.0;   // kanterne (log2 Hz) efter fartgrænsen
    bool   haveEdges = false;
    int    lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DopplerSatProcessor)
};
