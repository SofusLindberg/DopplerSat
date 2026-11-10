#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::StringArray DopplerSatProcessor::modeNames()
{
    // 1.3: "0 ms - " er fjernet (brugeren 2026-11-08: "Hvis teksten ikke har nogen repræsentativ betydning, skal
    // '0 ms' fjernes"). Alle tre er 0 ms, saa det skilte dem ikke ad. Kun teksten er aendret, indeks er de samme.
    return { "24 dB/oct", "48 dB/oct", "96 dB/oct" };
}

int DopplerSatProcessor::modePhase (int index)
{
    return index <= 0 ? satdsp::kPhaseZl24 : index == 1 ? satdsp::kPhaseZl48 : satdsp::kPhaseZl96;
}

double DopplerSatProcessor::modeMaxSpeed (int index)
{
    return index <= 0 ? kMaxOctPerSec24 : index == 1 ? kMaxOctPerSec48 : kMaxOctPerSec96;
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
    pauseP    = apvts.getRawParameterValue ("pause");
    newRandom();
    newRandom();
}

juce::AudioProcessorValueTreeState::ParameterLayout DopplerSatProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&] (auto p) { params.push_back (std::move (p)); };

    // Baandet: midten (Frequency), hvor langt LFO'en flytter det (Sweep) og dets egen stoerrelse (Band Size).
    // 1.4 (brugeren 2026-11-09): "Width skal hedde noget andet. Det skal fremgå tydeligere, at det er selve kurven, der
    // forlænges eller forkortes" -> "Band Size"; Amount -> "Sweep" (kun omraadet, baandet bevaeger sig hen over).
    // Kun navnene er nye; ID'erne ("amount", "width") er de samme, saa automation og gemte projekter virker.
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "freq", 1 }, "Frequency",
         juce::NormalisableRange<float> (minFreq, maxFreq, 0.1f, 0.3f), 600.f));
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "Sweep",
         juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f));
    juce::NormalisableRange<float> widthRange (0.1f, 6.f, 0.01f);
    widthRange.setSkewForCentre (1.f);
    add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "Band Size", widthRange, 1.f));

    // Kanternes stejlhed (0 ms-tilstandene; 1.2: 96 dB/oct er tilfoejet sidst, standard er stadig 48).
    // 1.1: Drive (fast 0 %) og Distortion er fjernet (brugeren 2026-11-04).
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

    // 1.6 (brugeren 2026-11-10): "en pause-knap, hvor effekten stopper, bandet er nået til. Dette skal hjælpe til, at
    // brugeren kan aflæse det spektrum, som band size-displayet viser". LFO'en staar stille, baandet bliver, hvor det
    // er. Tilfoejet sidst, saa de andre parametres raekkefoelge er den samme.
    add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "pause", 1 }, "Pause", false));

    // 1.8 havde "smooth" (Smooth Peaks) her. 1.9 (brugeren 2026-11-10: "byg 1.9 med smooth peaks som default"): den
    // er altid til og ikke laengere en parameter. Hver version har sin egen plugin-kode, saa 1.8-projekter roeres ikke.

    return { params.begin(), params.end() };
}

// 1.4 (brugeren 2026-11-09: "Sørg for, at pluginnet virker til Alle DAW's"): ogsaa mono -> mono og mono -> stereo,
// saa pluginnet kan saettes paa mono-spor (fx Cubase, Studio One, Reaper, Bitwig). Stereo lyder som foer.
bool DopplerSatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    return out == juce::AudioChannelSet::mono() && in == juce::AudioChannelSet::mono();
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

double DopplerSatProcessor::bandCentreFor (double v) const
{
    const double centre = std::log2 ((double) freqP->load()) + v * (double) amountP->load() * 0.01 * kMaxSweepOct;
    return juce::jlimit ((double) minFreq, (double) maxFreq, std::exp2 (centre));
}

satdsp::EngineParams DopplerSatProcessor::buildEngineParams (double lo, double hi) const
{
    satdsp::EngineParams p;
    const double fsProc = getProcessingRate();
    p.phaseMode = modePhase ((int) std::lround (modeP->load()));
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
    // saa saturationen er et tydeligt baand med 0 ms-tilstandens skraa kanter (24 / 48 / 96 dB/oct). Med Edge Q 50 %
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

    // Smooth Peaks (1.8, altid til fra 1.9): tidskonstanter (et-polede filtre). Reference-topniveauet falder paa 100 ms
    // (saa loftet holder mellem toppene); forstaerkningen gaar ned paa ca. 0.3 ms og op paa ca. 60 ms.
    auto coef = [sampleRate] (double ms) { return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * sampleRate))); };
    aRefDown = coef (100.0);
    aGainDown = coef (0.3);  aGainUp = coef (60.0);
    startRef();
    smoothGain = 1.f;
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
        // Pause (1.6): fasen foelger ikke vaerten, saa baandet bliver staaende. Naar pausen slippes, hopper fasen
        // tilbage til takten, og fartgraensen lader kanterne glide derhen.
        if (playing && havePpq && pauseP->load() < 0.5f)
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
    if (numSamples == 0 || buffer.getNumChannels() < 1) return;

    // 1.4: mono. Mono -> stereo: hoejre kanal kommer ikke fra vaerten, saa den faar venstres lyd. Mono -> mono:
    // motoren koerer stereo med samme lyd i begge kanaler (hoejre i en midlertidig buffer, der smides vaek).
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    const bool mono = buffer.getNumChannels() < 2;

    updateLfoState (numSamples);
    const double fs      = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    // Skift mellem 24, 48 og 96 dB/oct: delefiltrene faar en anden opbygning, saa motoren startes forfra (den toerre
    // lyd hoeres, mens filtrene varmes op, og saa fades der ind - som fasetilstandene i Multiband Saturator)
    const int mode = (int) std::lround (modeP->load());
    if (mode != lastMode) { core.prepare (fs * 4.0); lastMode = mode; startRef(); }   // reference-motoren med
    const int    shape   = (int) std::lround (shapeP->load());
    const double offset  = (double) offsetP->load() * 0.01;
    const double squareW = juce::jlimit (1.0e-4, 0.25, 0.008 * lfoRate);   // Square: overgang paa ca. 8 ms

    // 1.7 (brugeren 2026-11-10): "Uanset rate (speed), skal båndet bevæge sig ud helt ud i kanterne af den valgte
    // range" (som Abletons LFO Tool). Fartgraensen er derfor mindst LFO'ens egen topfart (+25 %), saa kanterne altid
    // kan foelge den. Den gamle graense (40 / 20 / 10 okt/s) er tilbage som bund, saa spring (Frequency, Pause,
    // automation) stadig glider. Det gaar, fordi baand 1's filtre nu er TPT-SVF (DopplerCore.h, ingen niveauspring).
    const double sweepOct = (double) amountP->load() * 0.01 * kMaxSweepOct;     // oktaver til hver side
    const double pi = juce::MathConstants<double>::pi;
    const double lfoTopSpeed = shape == 1 ? 4.0 * sweepOct * lfoRate                       // Triangle
                             : shape == 2 ? pi * sweepOct * lfoRate / squareW                // Square (overgangen)
                             : shape == 3 ? pi * sweepOct * lfoRate                          // Random (cosinus-glid)
                                          : 2.0 * pi * sweepOct * lfoRate;                   // Sine
    const double maxSpeed = modeMaxSpeed (mode) + 1.25 * lfoTopSpeed;
    const float  newWet  = dryWetP->load() * 0.01f;
    const bool   paused  = pauseP->load() > 0.5f;
    float* L = buffer.getWritePointer (0);
    float* R = mono ? nullptr : buffer.getWritePointer (1);
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

        // Smooth Peaks: reference-baandet foelger samme maal, men med den gamle fartgraense (som 1.6)
        const double refStep = modeMaxSpeed (mode) * (double) len / fs;
        if (! haveRefEdges) { refLoOct = curLoOct; refHiOct = curHiOct; haveRefEdges = true; }
        refLoOct += juce::jlimit (-refStep, refStep, tLo - refLoOct);
        refHiOct += juce::jlimit (-refStep, refStep, tHi - refHiOct);

        // 1.9 CPU: saa laenge de to motorer er ens bit for bit og faar de samme kanter (langsom Rate), er reference-
        // motorens lyd = hovedmotorens, og den regnes ikke. Skilles de (hurtig Rate, et spring), kopieres hovedmotoren
        // foerst, saa reference-motoren fortsaetter, praecis som hvis den havde koert hele tiden.
        const bool sameEdges = refLoOct == curLoOct && refHiOct == curHiOct;
        if (refSame && ! sameEdges) { refCore = core; refSame = false; }
        core.setParams (buildEngineParams (lastLo, lastHi));
        if (! refSame) refCore.setParams (buildEngineParams (std::exp2 (refLoOct), std::exp2 (refHiOct)));

        // Stykkets to kanaler (mono: hoejre er en kopi af venstre i monoR)
        float monoR[kChunk];
        float* cl = L + start;
        float* cr = mono ? monoR : R + start;
        if (mono) std::copy (cl, cl + len, monoR);

        // Det uroerte signal (til Dry/Wet), forsinket lige saa meget som oversamplingen
        float dryL[kChunk], dryR[kChunk];
        for (int i = 0; i < len; ++i)
        {
            dryDelay.pushSample (0, cl[i]);
            dryDelay.pushSample (1, cr[i]);
            dryL[i] = dryDelay.popSample (0);
            dryR[i] = dryDelay.popSample (1);
        }

        float* chans[2] = { cl, cr };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
        auto up = oversampler->processSamplesUp (block);
        const int nUp = (int) up.getNumSamples();

        // Smooth Peaks: reference-motoren faar samme oversamplede lyd; dens topniveau pr. vaerts-sample (stoerste af de
        // 4 oversamplede samples, begge kanaler) er loftets grundlag. Er motorerne ens, er det hovedmotorens lyd.
        float* u0 = up.getChannelPointer (0);
        float* u1 = up.getChannelPointer (1);
        float rl[kChunk * 4], rr[kChunk * 4];
        const float* pl = u0;
        const float* pr = u1;
        if (refSame)
        {
            core.process (u0, u1, nUp);
            ++refSkipped;
        }
        else
        {
            satdsp::DopplerCore::processPair (core, refCore, u0, u1, rl, rr, nUp);
            pl = rl; pr = rr;
            ++refRun;
            // Er de blevet ens igen (samme kanter, og forskellen i filtrene er doet helt ud)? Saa springes den over igen.
            if (sameEdges && refCore.sameStateAs (core)) refSame = true;
        }
        float refPeak[kChunk];
        const int os = nUp / len;
        for (int i = 0; i < len; ++i)
        {
            float pk = 0.f;
            for (int j = i * os; j < (i + 1) * os; ++j) pk = std::max (pk, std::max (std::abs (pl[j]), std::abs (pr[j])));
            refPeak[i] = pk;
        }
        oversampler->processSamplesDown (block);

        // Smooth Peaks (1.8, altid til fra 1.9): det vaade signal maa hoejst naa 3 dB over reference-motorens
        // topniveau (baandet med den gamle fartgraense, som 1.6). Ved langsom Rate er de ens, saa intet sker; ved hurtig
        // Rate daempes kun det, der stikker op over den rolige version. Proevet foerst (2026-11-10): kort RMS mod det
        // toerre signal (toppene faldt kun 0-2 dB) og top mod det toerre signal (roerte en savtak ved 0.25 Hz med op
        // til -19 dB, fordi filtrene flytter rundt paa overtonerne).
        {
            const float ceil = juce::Decibels::decibelsToGain (kSmoothCeilDb);
            for (int i = 0; i < len; ++i)
            {
                const float aw = std::max (std::abs (cl[i]), std::abs (cr[i]));
                float target = 1.f;
                refEnv = refPeak[i] > refEnv ? refPeak[i] : refEnv + aRefDown * (refPeak[i] - refEnv);
                const float allowed = ceil * refEnv + 1.0e-3f;                           // gulv: -60 dBFS
                if (aw > allowed) target = allowed / aw;
                smoothGain += (target < smoothGain ? aGainDown : aGainUp) * (target - smoothGain);
                cl[i] *= smoothGain;
                cr[i] *= smoothGain;
            }
        }

        for (int i = 0; i < len; ++i)
        {
            const float t = (float) (start + i) / (float) numSamples;
            const float w = lastDryWet + (newWet - lastDryWet) * t;
            cl[i] = dryL[i] + (cl[i] - dryL[i]) * w;
            cr[i] = dryR[i] + (cr[i] - dryR[i]) * w;
        }

        if (! paused)                                    // 1.6: Pause - LFO'en staar stille
        {
            phase += lfoRate * (double) len / fs;
            while (phase >= 1.0) { phase -= 1.0; cycle += 1.0; newRandom(); }
        }
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
        outAnalyzer.push (mono ? L[i] : 0.5f * (L[i] + R[i]));
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
