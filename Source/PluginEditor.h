#pragma once
#include "PluginProcessor.h"

// Doppler Saturator's editor: samme moerke hardware-look som Multiband Saturator (design 2A), med et display
// (spektrum + baandet, der bevaeger sig) og fire moduler: LFO, Band, Edges og Output.
// 1.1: Drive og Distortion er fjernet, kun output-spektret vises, "?" ved 0 ms-tilstandene, og hoejreklik paa
// enhver knap / vaelger giver en menu til automation (DAW'ens egen, hvis den har en).
class DopplerSatEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit DopplerSatEditor (DopplerSatProcessor&);
    ~DopplerSatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 900, kHeight = 600;

    // 1.10: et skrevet tal i Sweep-displayet (part 0 = bund, 1 = top, -1 = eet tal vist). Samme vej som naar brugeren
    // klikker og trykker Enter; offentlig, saa EditorShot kan teste den. false = afvist (intet tal, bund over top).
    bool applySweepText (int part, const juce::String& typed);
    // 1.14: faelles for skrivning og traek: ny vaerdi (Hz) for bund / top; gesture = een aendring i DAW'en
    bool applySweepHz (int part, double hz, bool gesture);
    void sweepEndsNow (double& lo, double& hi) const;     // Sweep-displayets ender, som de vises
    // Til EditorShot: aabn feltet over et tal med en bestemt tekst i (til billedet), og et traek (dy pixel, + = ned)
    void testBeginSweepEdit (int part, const juce::String& shownText);
    void testSweepDrag (int part, float dyPixels, bool fine);
    // Mockup (brugeren 2026-11-13, kun EditorShot): 0 = som nu, 1 = Sweep- og Band Size-displayet lige over deres knapper
    // (tre soejler i Band, displayene paa to linjer), Edges over Frequency
    void setMockupLayout (int variant);

    // Hoejreklik: en kontrol, der viser parameterens menu i stedet for at reagere paa hoejre museknap
    template <class Base>
    class ParamControl : public Base
    {
    public:
        std::function<void()> onRightClick;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
            Base::mouseDown (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Base::mouseDrag (e); }
        void mouseUp   (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Base::mouseUp (e); }
    };

private:
    class HardwareLook;
    class PlotArea;
    class LfoView;
    class LcdText;
    class HelpButton;                          // lille rundt "?" (som i Multiband Saturator)
    class HelpOverlay;                         // forklaringsboksen oven paa hele editoren

    void timerCallback() override;
    void updateSweepLcd();                     // 1.15: kun Sweep-displayets tekst (ved skrivning og traek)
    static int mockColX (int x, int w, int i);
    static int mockColW (int w);
    int mockup = 0;
    void renderPlate();
    void updateSync (bool sync);
    void updateMode (int mode);
    void showModeHelp();
    void showParamMenu (const juce::String& paramID);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using PSlider   = ParamControl<juce::Slider>;
    using PCombo    = ParamControl<juce::ComboBox>;
    using PButton   = ParamControl<juce::TextButton>;
    using PToggle   = ParamControl<juce::ToggleButton>;

    std::unique_ptr<HardwareLook> look;        // foerst: nedlaegges sidst
    DopplerSatProcessor& processor;
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::Image plate;

    std::unique_ptr<PlotArea>  plot;
    std::unique_ptr<LfoView>   lfoView;
    std::unique_ptr<LcdText>   rangeLcd, nowLcd;

    // LFO
    PSlider rateSlider, syncRateSlider, offsetSlider;
    PCombo  shapeBox;
    PButton hzButton, syncButton;
    PToggle pauseButton;                       // 1.6: LFO'en staar stille (i LFO-modulets titellinje)
    // Baand
    PSlider freqSlider, amountSlider, widthSlider;
    // Edges (de tre 0 ms-tilstande, 1.2: 96 dB/oct; 1.9: en soejle i Band-modulet)
    PButton modeButtons[DopplerSatProcessor::kNumModes];
    // Output
    PSlider dryWetSlider, outSlider;
    PToggle bypassButton;

    std::unique_ptr<HelpButton>  modeHelp;
    std::unique_ptr<HelpOverlay> helpOverlay;

    std::unique_ptr<SliderAtt> rateAtt, syncRateAtt, offsetAtt, freqAtt, amountAtt, widthAtt, dryWetAtt, outAtt;
    std::unique_ptr<ComboAtt>  shapeAtt;
    std::unique_ptr<ButtonAtt> bypassAtt, pauseAtt;
    std::unique_ptr<juce::ParameterAttachment> syncAtt, modeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DopplerSatEditor)
};
