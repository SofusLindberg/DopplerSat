#pragma once
#include "PluginProcessor.h"

// Doppler Saturator's editor: samme moerke hardware-look som Multiband Saturator (design 2A), med et display
// (spektrum + baandet, der bevaeger sig) og fire moduler: LFO, Band, Saturation og Output.
class DopplerSatEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit DopplerSatEditor (DopplerSatProcessor&);
    ~DopplerSatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 900, kHeight = 600;

private:
    class HardwareLook;
    class PlotArea;
    class LfoView;
    class LcdText;

    void timerCallback() override;
    void renderPlate();
    void updateSync (bool sync);
    void updateMode (int mode);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<HardwareLook> look;        // foerst: nedlaegges sidst
    DopplerSatProcessor& processor;
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::Image plate;

    std::unique_ptr<PlotArea> plot;
    std::unique_ptr<LfoView>  lfoView;
    std::unique_ptr<LcdText>  rangeLcd, nowLcd;

    // LFO
    juce::Slider     rateSlider, syncRateSlider, offsetSlider;
    juce::ComboBox   shapeBox;
    juce::TextButton hzButton, syncButton;
    // Baand
    juce::Slider     freqSlider, amountSlider, widthSlider;
    // Saturation
    juce::Slider     driveSlider;
    juce::ComboBox   typeBox;
    juce::TextButton modeButtons[2];
    // Output
    juce::Slider       dryWetSlider, outSlider;
    juce::ToggleButton bypassButton;

    std::unique_ptr<SliderAtt> rateAtt, syncRateAtt, offsetAtt, freqAtt, amountAtt, widthAtt, driveAtt, dryWetAtt, outAtt;
    std::unique_ptr<ComboAtt>  shapeAtt, typeAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;
    std::unique_ptr<juce::ParameterAttachment> syncAtt, modeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DopplerSatEditor)
};
