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

// 1.7: naar baandet helt ud i Sweep-omraadets kanter ved hoej Rate? (brugeren 2026-11-10: "Uanset rate (speed), skal
// båndet bevæge sig ud helt ud i kanterne af den valgte range"). Baandets midte maales hvert stykke (32 samples).
static bool reachTest()
{
    bool ok = true;
    std::printf ("  raekkevidde (Frequency 600 Hz, Sweep 100 %% = 75 Hz - 4.80 kHz, Band Size 0.5 okt):\n");
    for (int mode = 0; mode < 3; ++mode)
        for (int shape = 0; shape < 4; ++shape)
            for (float rate : { 1.f, 20.f })
            {
                DopplerSatProcessor p;
                p.setPlayConfigDetails (2, 2, 44100.0, 32);
                p.prepareToPlay (44100.0, 32);
                setParam (p, "freq", 600.f); setParam (p, "amount", 100.f); setParam (p, "width", 0.5f);
                setParam (p, "mode", (float) mode); setParam (p, "shape", (float) shape); setParam (p, "rate", rate);
                juce::AudioBuffer<float> buf (2, 32);
                juce::MidiBuffer midi;
                double lo = 1.0e9, hi = 0.0;
                const int blocks = (int) ((rate < 5.f ? 6.0 : 2.0) * 44100.0 / 32.0);
                for (int b = 0; b < blocks; ++b)
                {
                    buf.clear();
                    p.processBlock (buf, midi);
                    if (b < blocks / 3) continue;                          // foerst en tredjedel opvarmning
                    const double c = std::sqrt (p.bandLoNow.load() * p.bandHiNow.load());   // midten (geometrisk)
                    lo = std::min (lo, c); hi = std::max (hi, c);
                }
                // Random naar sjaeldent helt ud (tilfaeldige maal); de andre skal naa inden for 0.1 oktav
                const double loOct = std::log2 (lo / 75.0), hiOct = std::log2 (4800.0 / hi);
                const bool pass = shape == 3 || (loOct < 0.1 && hiOct < 0.1);
                ok = ok && pass;
                static const char* sn[] = { "Sine", "Triangle", "Square", "Random" };
                std::printf ("    %2d dB/oct %-8s %4.0f Hz: midten %6.1f Hz - %6.0f Hz  %s\n", 24 << mode, sn[shape], rate, lo, hi,
                             pass ? "OK" : "NAAR IKKE UD");
            }
    return ok;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[2]) == "--reach") return reachTest() ? 0 : 1;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    DopplerSatProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    run (proc, 1.3);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler1.png"));

    // 1.6: Band Size 4 okt -> Sweep-displayet og det skraverede omraade skal vaere de samme som i doppler1
    setParam (proc, "width", 4.f);
    run (proc, 0.5);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler6_bandsize4.png"));
    setParam (proc, "width", 1.f);

    // 1.6: Pause -> LFO'ens fase og baandet staar stille
    setParam (proc, "pause", 1.f);
    run (proc, 0.2);
    const double ph0 = proc.lfoPhaseNow.load(), lo0 = proc.bandLoNow.load();
    run (proc, 1.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    const bool still = proc.lfoPhaseNow.load() == ph0 && proc.bandLoNow.load() == lo0;
    std::printf ("  pause: fase %.4f -> %.4f, baandets bund %.1f -> %.1f Hz  %s\n", ph0, proc.lfoPhaseNow.load(), lo0,
                 proc.bandLoNow.load(), still ? "staar stille OK" : "FEJL");
    save (*ed, dir.getChildFile ("doppler7_pause.png"));
    setParam (proc, "pause", 0.f);

    setParam (proc, "shape", 2.f);  setParam (proc, "offset", 40.f); setParam (proc, "sync", 1.f);
    setParam (proc, "mode", 0.f);   setParam (proc, "amount", 80.f);
    run (proc, 0.7);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    save (*ed, dir.getChildFile ("doppler2.png"));

    setParam (proc, "shape", 3.f);  setParam (proc, "offset", -30.f); setParam (proc, "sync", 0.f);
    setParam (proc, "mode", 2.f);                          // 1.2: 96 dB/oct
    setParam (proc, "freq", 2000.f);  setParam (proc, "width", 2.5f);
    setParam (proc, "rate", 2.f);
    for (int i = 0; i < 60; ++i)                          // Random-sporet fyldes af editorens timer
    {
        run (proc, 0.034);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    save (*ed, dir.getChildFile ("doppler3.png"));

    // 1.1: hoejreklik paa en knap / drejeknap -> automationsmenuen, og kontrollen selv reagerer IKKE
    auto rightClick = [] (juce::Component& c)
    {
        const auto pos = c.getLocalBounds().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), pos,
                                  juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), 0.f, 0.f, 0.f, 0.f, 0.f,
                                  &c, &c, now, pos, now, 1, false);
        c.mouseDown (e);
        c.mouseUp (e);
    };
    bool menuOk = true;
    for (auto* c : ed->getChildren())
    {
        auto* tb = dynamic_cast<juce::ToggleButton*> (c);
        auto* sl = dynamic_cast<juce::Slider*> (c);
        if ((tb == nullptr || tb->getButtonText() != "Bypass") && (sl == nullptr || ! sl->isVisible() || sl->getTextValueSuffix() != " Hz"
                                                                    || sl->getValue() < 100.0))
            continue;
        const auto& desk = juce::Desktop::getInstance();
        const int   before = desk.getNumComponents();
        const bool  state0 = tb != nullptr && tb->getToggleState();
        const double v0    = sl != nullptr ? sl->getValue() : 0.0;
        rightClick (*c);
        // Straks: JUCE lukker selv menuen igen, naar programmet ikke er i forgrunden (det er en konsol-test aldrig)
        const bool opened = desk.getNumComponents() > before;
        const bool still  = tb != nullptr ? tb->getToggleState() == state0 : sl->getValue() == v0;
        menuOk = menuOk && opened && still;
        std::printf ("  hoejreklik paa %s: menu %s, kontrollen %s\n", tb != nullptr ? "Bypass" : "Frequency",
                     opened ? "aabnet" : "IKKE aabnet", still ? "uaendret" : "AENDRET");
        if (opened && tb != nullptr)
            if (auto* menu = desk.getComponent (desk.getNumComponents() - 1))
                save (*menu, dir.getChildFile ("doppler5_menu.png"));
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    }
    std::printf (menuOk ? "  hoejreklik OK\n" : "  hoejreklik FEJL\n");

    // 1.1: "?" ved de to 0 ms-tilstande -> forklaringsboksen
    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getName() == "?") b->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    save (*ed, dir.getChildFile ("doppler4_help.png"));

    ed.reset();
    return 0;
}
