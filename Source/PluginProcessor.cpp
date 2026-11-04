#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::StringArray DopplerSatProcessor::modeNames()
{
    return { "0 ms - 24 dB/oct", "0 ms - 48 dB/oct" };
}

juce::StringArray DopplerSatProcessor::shapeNames()
{
    return { "Sine", "Triangle", "Square", "Random" };
}

// Rate i Sync (brugeren: "timet til bpm i forskellige rytmer (1/4, 1/3, 4/4, alle de relevante og alment brugte)"):
// takter, punkterede (.) og trioler (T)
juce::StringArray DopplerSatProcessor::syncNames()
{
    return { "8 bars", "4 bars", "2 bars", "1 bar", "1/2.", "1/2", "1/2T", "1/4.", "1/4", "1/4T",
             "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32" };
}

double DopplerSatProcessor::syncBeats (int index, double barBeats)
{
    // negative = takter (ganges med taktens laengde), ellers fjerdedele
    static const double b[] = { -8.0, -4.0, -2.0, -1.0, 3.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0,
                                0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    const double v = b[juce::jlimit (0, (int) std::size (b) - 1, index)];
    return v < 0.0 ? -v * barBeats : v;
}

double DopplerSatProcessor::lfoWave (int shape, double ph, double rndA, double rndB, double squareW)
{
    switch (shape)
    {
        case 1:                                                  // Triangle (i fase med sinus)
            return ph < 0.25 ? 4.0 * ph : ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0;
        case 2:                                                  // Square med bloede overgange (ingen klik)
        {
            const double h = 0.5 * squareW;
            auto ramp = [h] (double d)                           // -1 foer overgangen, +1 efter
            {
                if (d <= -h) return -1.0;
                if (d >=  h) return  1.0;
                return -std::cos (juce::MathConstants<double>::pi * (d + h) / (2.0 * h));
            };
            if (ph < 0.25 || ph >= 0.75) return ramp (ph < 0.5 ? ph : ph - 1.0);
            return -ramp (ph - 0.5);
        }
        case 3:                                                  // Random: bloed fra en tilfaeldig vaerdi til den naeste
        {
            const double t = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * ph);
            return rndA + (rndB - rndA) * t;
        }
        default:                                                 // Sine
            return std::sin (juce::MathConstants<double>::twoPi * ph);
    }
}

// Offset (brugeren: "Med mulighed for offset [...], så man blive længere tid på toppen eller bund af en kurve"):
// kurven flyttes op eller ned og klippes ved -1 / +1, som i Abletons LFO
double DopplerSatProcessor::lfoValue (int shape, double ph, double offset, double rndA, double rndB, double squareW)
{
    return juce::jlimit (-1.0, 1.0, lfoWave (shape, ph, rndA, rndB, squareW) + offset);
}

DopplerSatProcessor::DopplerSatProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    // Hele signalet gaar gennem 4x oversampling og motoren (som 0 ms-tilstandene i Multiband Saturator)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) 2, (size_t) 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);

    modeP     = apvts.getRawParameterValue ("mode");
    freqP     = apvts.getRawParameterValue ("freq");
    amountP   = apvts.getRawParameterValue ("amount");
    widthP    = apvts.getRawParameterValue ("width");
    rateP     = apvts.getRawParameterValue ("rate");
    syncP     = apvts.getRawParameterValue ("sync");
    syncRateP = apvts.getRawParameterValue ("syncrate");
    shapeP    = apvts.getRawParameterValue ("shape");
    offsetP   = apvts.getRawParameterValue ("offset");
    dryWetP   = apvts.getRawParameterValue ("drywet");
    outP      = apvts.getRawParameterValue ("outgain");
    bypassP   = apvts.getRawParameterValue ("bypass");
    newRandom();
    newRandom();
}

juce::AudioProcessorValueTreeState::ParameterLayout DopplerSatProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&] (auto p) { params.push_back (std::move (p)); };

    // Baandet: midten (Frequency), hvor langt LFO'en flytter det (Amount) og dets bredde (Width)
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "freq", 1 }, "Frequency",
         juce::NormalisableRange<float> (minFreq, maxFreq, 0.1f, 0.3f), 600.f));
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "Amount",
         juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f));
    juce::NormalisableRange<float> widthRange (0.1f, 6.f, 0.01f);
    widthRange.setSkewForCentre (1.f);
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "Width", widthRange, 1.f));

    // Kanternes stejlhed (de to 0 ms-tilstande). 1.1: Drive (fast 0 %) og Distortion er fjernet (brugeren 2026-11-04).
    add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", modeNames(), 1));

    // LFO: Rate i Hz (fra meget langsomt til meget hurtigt) eller i nodevaerdier (Sync), form og offset
    juce::NormalisableRange<float> rateRange (0.01f, 20.f, 0.01f);
    rateRange.setSkewForCentre (1.f);
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "rate", 1 }, "Rate", rateRange, 0.5f));
    add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "sync", 1 }, "Sync", false));
    add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "syncrate", 1 }, "Sync Rate", syncNames(), 3));
    add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "shape", 1 }, "LFO Shape", shapeNames(), 0));
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "offset", 1 }, "Offset",
         juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f));

    // Output: Dry/Wet blander det uroerte signal med det behandlede (halvt-halvt giver ogsaa phaser-huller)
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "drywet", 1 }, "Dry/Wet",
         juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "outgain", 1 }, "Output Gain",
         juce::NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f));
    add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

bool DopplerSatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

double DopplerSatProcessor::getProcessingRate() const
{
    return (getSampleRate() > 0.0 ? getSampleRate() : 44100.0) * 4.0;
}

void DopplerSatProcessor::bandEdgesFor (double v, double& lo, double& hi) const
{
    const double centre = std::log2 ((double) freqP->load()) + v * (double) amountP->load() * 0.01 * kMaxSweepOct;
    const double half   = 0.5 * (double) widthP->load();
    lo = juce::jlimit ((double) minFreq, (double) maxFreq, std::exp2 (centre - half));
    hi = juce::jlimit ((double) minFreq, (double) maxFreq, std::exp2 (centre + half));
    if (hi < lo * 1.05) { if (lo * 1.05 <= maxFreq) hi = lo * 1.05; else lo = hi / 1.05; }
}

satdsp::EngineParams DopplerSatProcessor::buildEngineParams (double lo, double hi) const
{
    satdsp::EngineParams p;
    const double fsProc = getProcessingRate();
    p.phaseMode = (int) std::lround (modeP->load()) == 0 ? satdsp::kPhaseZl24 : satdsp::kPhaseZl48;
    p.baseRate  = fsProc / 4.0;
    p.bypass    = bypassP->load() > 0.5f;
    p.soloBand  = -1;

    // Motoren har fire baand. Kun baand kBand bruges; de andre er slukket og ligger langt udenfor det hoerbare
    // (deres delefiltre staar stille, saa de giver ingen pitch-aendring)
    for (int b = 0; b < satdsp::kBands; ++b) p.band[b].on = false;
    p.band[0].lo = 10.0;          p.band[0].hi = 12.0;
    p.band[2].lo = 0.40 * fsProc; p.band[2].hi = 0.42 * fsProc;
    p.band[3].lo = 0.43 * fsProc; p.band[3].hi = 0.45 * fsProc;

    auto& bp = p.band[kBand];
    bp.on     = true;
    bp.lo     = lo;
    bp.hi     = hi;
    // 1.1: ingen saturation (brugeren: "Selve drive-effekten skal være så lidt lydbar som overhovedet muligt, gerne
    // helt væk, hvis doppler-effekten så stadig virker"). Doppler-effekten er delefiltrene, ikke driven.
    bp.type   = 0;
    bp.drive  = 0.0;
    bp.mix    = 1.0;
    // Edge Q er bestemt paa forhaand (brugeren: "Edge Q er forudbestemt for brugeren og kan ikke ændres"): fra,
    // saa saturationen er et tydeligt baand med 0 ms-tilstandens skraa kanter (24 / 48 dB/oct). Med Edge Q 50 %
    // bredte fadet saturationen ud over det meste af spektret. Doppler-effekten er den samme (delefiltrene).
    bp.edgeOn = false;
    bp.edgeQ  = 50.0;
    return p;
}

void DopplerSatProcessor::newRandom()
{
    rndA = rndB;
    rndB = 2.0 * rng.nextDouble() - 1.0;
}

void DopplerSatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampler->initProcessing ((size_t) juce::jmax (maxBlock, kChunk));
    oversampler->reset();

    core.prepare (sampleRate * 4.0);

    const float lat = oversampler->getLatencyInSamples();
    dryDelay.prepare ({ sampleRate, (juce::uint32) juce::jmax (maxBlock, kChunk), 2 });
    dryDelay.reset();
    dryDelay.setDelay (lat);
    setLatencySamples ((int) std::lround (lat));

    lastOutGain = juce::Decibels::decibelsToGain (outP->load());
    lastDryWet  = dryWetP->load() * 0.01f;
    haveEdges = false;
    lastMode  = (int) std::lround (modeP->load());
    phase = 0.0;
    cycle = 0.0;
    wasPlaying = false;
    outAnalyzer.reset();
}

void DopplerSatProcessor::updateLfoState (int)
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    double bpm = 120.0, ppq = 0.0, barBeats = 4.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0) barBeats = ts->numerator * 4.0 / ts->denominator;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
        }

    if (syncP->load() > 0.5f)
    {
        const double beats = syncBeats ((int) std::lround (syncRateP->load()), barBeats);
        lfoRate = bpm / 60.0 / beats;
        if (playing && havePpq)
        {
            // Fasen foelger vaertens position, saa LFO'en ligger fast i takten (samme sted hver gang)
            const double c = ppq / beats, whole = std::floor (c);
            if (whole != cycle) { newRandom(); cycle = whole; }
            phase = c - whole;
        }
    }
    else
    {
        lfoRate = juce::jlimit (0.001, 50.0, (double) rateP->load());
    }
    juce::ignoreUnused (fs);
    wasPlaying = playing;
}

void DopplerSatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2) return;

    updateLfoState (numSamples);
    const double fs      = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    // Skift mellem 24 og 48 dB/oct: delefiltrene faar en anden opbygning, saa motoren startes forfra (den toerre
    // lyd hoeres, mens filtrene varmes op, og saa fades der ind - som fasetilstandene i Multiband Saturator)
    const int mode = (int) std::lround (modeP->load());
    if (mode != lastMode) { core.prepare (fs * 4.0); lastMode = mode; }
    const double maxSpeed = mode == 0 ? kMaxOctPerSec24 : kMaxOctPerSec48;
    const int    shape   = (int) std::lround (shapeP->load());
    const double offset  = (double) offsetP->load() * 0.01;
    const double squareW = juce::jlimit (1.0e-4, 0.25, 0.008 * lfoRate);   // Square: overgang paa ca. 8 ms
    const float  newWet  = dryWetP->load() * 0.01f;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    double v = 0.0;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = juce::jmin (kChunk, numSamples - start);

        // LFO -> baandets kanter -> motoren (hvert stykke paa 32 samples, saa baandet flytter sig glat)
        v = lfoValue (shape, phase, offset, rndA, rndB, squareW);
        double lo, hi;
        bandEdgesFor (v, lo, hi);
        // Fartgraensen: kanterne gaar hoejst maxStep oktaver mod maalet pr. stykke (hurtig Rate, Square, et hurtigt
        // traek i Frequency). Ved meget hurtige LFO'er bliver bevaegelsen derfor mindre.
        const double tLo = std::log2 (lo), tHi = std::log2 (hi), maxStep = maxSpeed * (double) len / fs;
        if (! haveEdges) { curLoOct = tLo; curHiOct = tHi; haveEdges = true; }
        curLoOct += juce::jlimit (-maxStep, maxStep, tLo - curLoOct);
        curHiOct += juce::jlimit (-maxStep, maxStep, tHi - curHiOct);
        lastLo = std::exp2 (curLoOct);
        lastHi = std::exp2 (curHiOct);
        core.setParams (buildEngineParams (lastLo, lastHi));

        // Det uroerte signal (til Dry/Wet), forsinket lige saa meget som oversamplingen
        float dryL[kChunk], dryR[kChunk];
        for (int i = 0; i < len; ++i)
        {
            dryDelay.pushSample (0, L[start + i]);
            dryDelay.pushSample (1, R[start + i]);
            dryL[i] = dryDelay.popSample (0);
            dryR[i] = dryDelay.popSample (1);
        }

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) len);
        auto up = oversampler->processSamplesUp (block);
        core.process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
        oversampler->processSamplesDown (block);

        for (int i = 0; i < len; ++i)
        {
            const float t = (float) (start + i) / (float) numSamples;
            const float w = lastDryWet + (newWet - lastDryWet) * t;
            L[start + i] = dryL[i] + (L[start + i] - dryL[i]) * w;
            R[start + i] = dryR[i] + (R[start + i] - dryR[i]) * w;
        }

        phase += lfoRate * (double) len / fs;
        while (phase >= 1.0) { phase -= 1.0; cycle += 1.0; newRandom(); }
    }
    lastDryWet = newWet;

    lfoPhaseNow.store (phase);
    lfoValueNow.store (v);
    rndNowA.store (rndA);
    rndNowB.store (rndB);
    bandLoNow.store (lastLo);
    bandHiNow.store (lastHi);

    const float newGain = juce::Decibels::decibelsToGain (outP->load());
    buffer.applyGainRamp (0, numSamples, lastOutGain, newGain);
    lastOutGain = newGain;

    for (int i = 0; i < numSamples; ++i)
        outAnalyzer.push (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
}

juce::AudioProcessorEditor* DopplerSatProcessor::createEditor()
{
    return new DopplerSatEditor (*this);
}

void DopplerSatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DopplerSatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DopplerSatProcessor();
}
