// Billeder af Doppler Saturator's editor uden vaert: EditorShot.exe <mappe>
// Koerer lyd gennem processoren (saa spektrum og LFO bevaeger sig) og gemmer PNG'er i et par tilstande.
#include <JuceHeader.h>
#include "../../Source/PluginProcessor.h"

static void setParam (DopplerSatProcessor& p, const char* id, float v)
{
    if (auto* par = p.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (v));
}

static void run (DopplerSatProcessor& p, double seconds)
{
    static double t = 0.0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random r (1);
    for (int blk = 0; blk < (int) (seconds * 44100.0 / 512.0); ++blk)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / 44100.0)
        {
            const float s = (float) (0.25 * std::sin (2 * 3.14159265 * 110 * t) + 0.15 * std::sin (2 * 3.14159265 * 440 * t)
                                     + 0.05 * (r.nextFloat() * 2.f - 1.f));
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
    }
}

static void save (juce::Component& c, const juce::File& f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds());
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("  %s\n", f.getFileName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    DopplerSatProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    run (proc, 1.3);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler1.png"));

    setParam (proc, "shape", 2.f);  setParam (proc, "offset", 40.f); setParam (proc, "sync", 1.f);
    setParam (proc, "drive", 250.f); setParam (proc, "mode", 0.f);   setParam (proc, "amount", 80.f);
    run (proc, 0.7);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler2.png"));

    setParam (proc, "shape", 3.f);  setParam (proc, "offset", -30.f); setParam (proc, "sync", 0.f);
    setParam (proc, "drive", 30.f); setParam (proc, "freq", 2000.f);  setParam (proc, "width", 2.5f);
    setParam (proc, "rate", 2.f);
    for (int i = 0; i < 60; ++i)                          // Random-sporet fyldes af editorens timer
    {
        run (proc, 0.034);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    save (*ed, dir.getChildFile ("doppler3.png"));

    ed.reset();
    return 0;
}
