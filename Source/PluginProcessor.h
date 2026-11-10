#pragma once
#include <JuceHeader.h>
#include "DopplerCore.h"

// Doppler Saturator (brugeren 2026-11-03): "Dobbler-effekt (ptitch ændres, når man trækker båndet henover
// frekvenserne) i multiband saturation er faktisk super fedt. [...] Tag spektogrammet med et enkelt bånd med
// phase-indstilligerne "0 ms - 24 db/oct" og "0 ms - 48 db/oct" (ikke medtag linear phase), så vi kan lave en
// saturator, hvis eneste formål er at give brugeren mulighed for at bruge dobbler-effekten [...] kreativt."
//
// Lyden er 0 ms-motoren fra Multiband Saturator 3.17 (satdsp::Engine, SatDsp.h) med eet baand. En LFO flytter
// baandet hen over det valgte omraade, og naar dets delefiltre flytter sig, aendres pitchen (Doppler).
//
// 1.1 (brugeren 2026-11-04): "Brugeren skal ikke kunne ændre drive [...] Selve drive-effekten skal være så lidt
// lydbar som overhovedet muligt, gerne helt væk". Drive er fast 0 % (ingen saturation overhovedet), og Distortion
// er fjernet, fordi den intet goer ved 0 %. Lyden er kun delefiltrenes allpass (satdsp::DopplerCore), bit for bit
// som 1.0 med Drive paa 0 %.
//
// 1.2 (brugeren 2026-11-05): "yderligere en 0 - ms, som er endnu stejlere og pitcher lyden endnu mere". Tredje valg
// "0 ms - 96 dB/oct"; 24 og 48 lyder bit for bit som i 1.1.
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
    // 96 dB (1.2): dobbelt saa stejl, halv fart. Square 20 Hz med to toner (peak 0.6) giver da 0.68 / 0.75 / 0.79 for
    // 24 / 48 / 96 (maaleproeve, verste af fire baandplaceringer); ved 20 okt/s gav 96 1.01 og ved 160 okt/s 4.8.
    static constexpr double kMaxOctPerSec24 = 40.0, kMaxOctPerSec48 = 20.0, kMaxOctPerSec96 = 10.0;
    static constexpr int    kNumModes = 3;

    static juce::StringArray modeNames();         // index 0 = 0 ms 24 dB/oct, 1 = 48 dB/oct, 2 = 96 dB/oct (1.2)
    static int    modePhase (int index);          // valg nr. -> satdsp::kPhaseZl24 / 48 / 96
    static double modeMaxSpeed (int index);       // fartgraensen for valg nr.
    static juce::StringArray shapeNames();        // Sine, Triangle, Square, Random
    static juce::StringArray syncNames();         // nodevaerdier til Rate i Sync
    static double            syncBeats (int index, double barBeats);   // laengden i fjerdedele

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
    // 1.4: kanterne ringer efter (allpass). Laengst maalt 1.32 s til -80 dB (20 Hz, 0.1 okt, 96 dB/oct; HostSmoke).
    // Siger pluginnet 0 s, klipper DAW'er, der stopper plugins ved stilhed eller bouncer, efterklangen af.
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Pluginnets Bypass er ogsaa DAW'ens bypass (VST3 kIsBypass), saa der kun er een Bypass i automationslisten
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

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
    Analyzer outAnalyzer;            // 1.1: kun output vises (brugeren: "Vis kun output på grafen")

    // Baandets kanter for en LFO-vaerdi v (-1..1): midten flyttes v * Amount * 3 oktaver fra Frequency
    void bandEdgesFor (double v, double& lo, double& hi) const;
    // 1.6: baandets midte for en LFO-vaerdi v (Hz, 20 Hz - 20 kHz). Sweep-displayet og det skraverede omraade viser
    // midtens vej (Frequency +/- Sweep), saa Band Size ikke aendrer dem (brugeren 2026-11-10)
    double bandCentreFor (double v) const;
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

    std::atomic<float> *modeP = nullptr, *freqP = nullptr, *amountP = nullptr, *widthP = nullptr,
                       *rateP = nullptr, *syncP = nullptr, *syncRateP = nullptr, *shapeP = nullptr,
                       *offsetP = nullptr, *dryWetP = nullptr, *outP = nullptr, *bypassP = nullptr,
                       *pauseP = nullptr, *smoothP = nullptr;

    // 1.8 Smooth Peaks: det vaade signal maa hoejst naa kSmoothCeilDb over topniveauet af en reference-motor, hvor
    // baandet flytter sig med den gamle fartgraense (som 1.6). Reference-motoren koerer kun, naar knappen er taendt.
    static constexpr float kSmoothCeilDb = 3.f;
    satdsp::DopplerCore refCore;
    bool   refOn = false, haveRefEdges = false;
    double refLoOct = 0.0, refHiOct = 0.0;
    float  refEnv = 0.f, smoothGain = 1.f;                // reference-topniveau og forstaerkningen lige nu
    float  aRefDown = 1.f, aGainDown = 1.f, aGainUp = 1.f;

    satdsp::DopplerCore core;        // = satdsp::Engine ved 0 % drive, uden alt det, der ikke kan hoeres
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
