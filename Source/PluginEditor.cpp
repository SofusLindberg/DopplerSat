#include "PluginEditor.h"

using P = DopplerSatProcessor;

// =============================================================================
//  Maal (fast vindue 900 x 600). y i modulerne regnes fra modulets top.
// =============================================================================
namespace layout
{
    constexpr int   modH  = 234, gap = 8;
    constexpr float widths[] = { 0.33f, 0.25f, 0.24f, 0.18f };    // LFO, Band, Saturation, Output
    constexpr float title = 17;                                   // modultitlernes grundlinje
    constexpr float knobLabel = 131, knobSub = 142;               // knap-labels og deres undertekst
    constexpr float knob = 180, value = 210.5f;                   // drejeknappernes midte og tal-felternes top
}

namespace hw
{
    const juce::Colour silk (0xffd3d6dd), silkDim (0xff8d929d), amber (0xffffb340), orange (0xffffa94d),
                       cyan (0xff6ee7ff), green (0xff50fa7b), purple (0xffc77dff), red (0xffff4b4b),
                       selBlue (0xff3b9eff), lfo (0xff8be9fd);
}
static const juce::Colour driveColour (0xffffa94d), inputColour (0xff6ee7ff), outputColour (0xffc77dff),
                          bandColour (0xff50fa7b);

static juce::Font hwFont (const juce::String& name, float size, const juce::String& style = "Regular", float spacing = 0.f)
{
    juce::Font f (juce::FontOptions().withName (name).withStyle (style).withPointHeight (size));
    if (spacing != 0.f) f.setExtraKerningFactor (spacing / f.getHeight());
    return f;
}

static void silkText (juce::Graphics& g, const juce::String& t, float x, float baseline, float size, juce::Colour c,
                      juce::Justification j = juce::Justification::left, float spacing = 0.9f, bool upper = true)
{
    g.setFont (hwFont ("Bahnschrift", size, "Regular", spacing));
    g.setColour (c);
    g.drawSingleLineText (upper ? t.toUpperCase() : t, juce::roundToInt (x), juce::roundToInt (baseline), j);
}

static void drawLed (juce::Graphics& g, float cx, float cy, bool on, juce::Colour col, float r)
{
    if (on)
    {
        g.setColour (col.withAlpha (0.22f));
        g.fillEllipse (cx - r - 3.5f, cy - r - 3.5f, 2.f * (r + 3.5f), 2.f * (r + 3.5f));
    }
    g.setColour (on ? col : juce::Colour (0xff1a1b1e));
    g.fillEllipse (cx - r, cy - r, 2.f * r, 2.f * r);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (cx - r, cy - r, 2.f * r, 2.f * r, 1.f);
    if (on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (cx - 1.f - 0.35f * r, cy - 1.f - 0.35f * r, 0.7f * r, 0.7f * r);
    }
}

static void fillButton (juce::Graphics& g, juce::Rectangle<float> r, bool on, float rx, bool highlight)
{
    juce::ColourGradient grad (juce::Colour (on ? 0xff2e3036 : 0xff5a5e66), 0.f, r.getY(),
                               juce::Colour (on ? 0xff41444b : 0xff383b41), 0.f, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, rx);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawRoundedRectangle (r, rx, 1.f);
    if (highlight)
    {
        g.setColour (juce::Colours::white.withAlpha (on ? 0.05f : 0.18f));
        g.drawRoundedRectangle (r.reduced (1.f), rx - 1.f, 1.f);
    }
}

static void drawLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient grad (juce::Colour (0xff040605), 0.f, r.getY(), juce::Colour (0xff0d1210), 0.f, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 2.f, 1.f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.f, 1.f);
}

static juce::String hzText (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}

// =============================================================================
//  HardwareLook (fra Multiband Saturator 3.17): drejeknapper, LED-knapper, LCD-felter
// =============================================================================
class DopplerSatEditor::HardwareLook : public juce::LookAndFeel_V4
{
public:
    HardwareLook()
    {
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff1c1e21));
        setColour (juce::PopupMenu::textColourId,                  hw::silk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3d44));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId,        juce::Colour (0xff202226));
        setColour (juce::TooltipWindow::textColourId,              hw::silk);
        setColour (juce::TooltipWindow::outlineColourId,           juce::Colour (0xff5a5e66));
        setColour (juce::TextEditor::backgroundColourId,           juce::Colour (0xff040605));
        setColour (juce::TextEditor::textColourId,                 hw::silk);
        setColour (juce::TextEditor::highlightColourId,            hw::selBlue.withAlpha (0.4f));
        setColour (juce::TextEditor::outlineColourId,              juce::Colours::black);
        setColour (juce::TextEditor::focusedOutlineColourId,       hw::selBlue);
        setColour (juce::CaretComponent::caretColourId,            hw::silk);
    }

    static juce::Colour ledOf (const juce::Component& c)
    {
        return juce::Colour ((juce::uint32) (juce::int64) c.getProperties().getWithDefault ("led", (juce::int64) 0xffffffff));
    }

    void drawRotarySlider (juce::Graphics& g, int, int, int, int, float pos, float, float, juce::Slider& s) override
    {
        const float r   = (float) (int) s.getProperties().getWithDefault ("knobR", 20);
        const float cx  = (float) s.getWidth() * 0.5f;
        const float cy  = (float) s.getProperties().getWithDefault ("knobCy", (double) s.getHeight() * 0.5);
        const auto  col = s.findColour (juce::Slider::rotarySliderFillColourId);
        const bool  bi  = (bool) s.getProperties().getWithDefault ("bipolar", false);   // Offset: ringen fra midten
        auto pol = [&] (float rad, float deg)
        {
            const float a = juce::degreesToRadians (deg);
            return juce::Point<float> (cx + rad * std::sin (a), cy - rad * std::cos (a));
        };
        auto arc = [&] (float d0, float d1)
        {
            juce::Path p;
            p.addCentredArc (cx, cy, r + 4.5f, r + 4.5f, 0.f, juce::degreesToRadians (juce::jmin (d0, d1)),
                             juce::degreesToRadians (juce::jmax (d0, d1)), true);
            return p;
        };

        for (int i = 0; i <= 10; ++i)
        {
            const float a = -135.f + 27.f * (float) i;
            g.setColour (hw::silk.withAlpha (0.55f));
            g.drawLine (juce::Line<float> (pol (r + 8.f, a), pol (r + 11.f, a)), i % 5 == 0 ? 1.6f : 1.f);
        }
        const juce::PathStrokeType ring (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colour (0xff0f1012));
        g.strokePath (arc (-135.f, 135.f), ring);
        const float a0 = bi ? 0.f : -135.f;
        const float a1 = -135.f + 270.f * juce::jmax (pos, 0.004f);
        if (std::abs (a1 - a0) > 0.5f)
        {
            g.setColour (col.withAlpha (0.35f));
            g.strokePath (arc (a0, a1), juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (arc (a0, a1), ring);
        }

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (cx + 1.5f - (r + 1.f), cy + 3.f - (r + 1.f), 2.f * (r + 1.f), 2.f * (r + 1.f));
        juce::ColourGradient body (juce::Colour (0xff7a7e88), cx - 0.3f * r, cy - 0.4f * r,
                                   juce::Colour (0xff1c1d21), cx - 0.3f * r + 1.8f * r, cy - 0.4f * r, true);
        g.setGradientFill (body);
        g.fillEllipse (cx - r, cy - r, 2.f * r, 2.f * r);
        g.setColour (juce::Colour (0xff0a0a0b));
        g.drawEllipse (cx - r, cy - r, 2.f * r, 2.f * r, 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 0; i < 28; ++i)
            g.drawLine (juce::Line<float> (pol (r - 2.5f, (float) i * 360.f / 28.f), pol (r - 0.5f, (float) i * 360.f / 28.f)), 1.f);
        const float rc = r * 0.74f;
        juce::ColourGradient cap (juce::Colour (0xff5d616b), cx - 0.2f * rc, cy - 0.4f * rc,
                                  juce::Colour (0xff2a2c31), cx - 0.2f * rc + 1.7f * rc, cy - 0.4f * rc, true);
        g.setGradientFill (cap);
        g.fillEllipse (cx - rc, cy - rc, 2.f * rc, 2.f * rc);
        g.setColour (juce::Colour (0xff101114));
        g.drawEllipse (cx - rc, cy - rc, 2.f * rc, 2.f * rc, 1.f);

        const float ia = -135.f + 270.f * pos;
        g.setColour (juce::Colours::white);
        juce::Path ind;
        ind.startNewSubPath (pol (r * 0.22f, ia));
        ind.lineTo (pol (r * 0.70f, ia));
        g.strokePath (ind, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        juce::Slider::SliderLayout l;
        const auto b = s.getLocalBounds();
        l.textBoxBounds = { b.getCentreX() - 33, b.getBottom() - 17, 66, 17 };
        l.sliderBounds  = b.withTrimmedBottom (17);
        return l;
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->getProperties().set ("lcd", true);
        l->setFont (hwFont ("Consolas", 11.5f));
        l->setJustificationType (juce::Justification::centred);
        l->setBorderSize ({ 0, 0, 0, 0 });
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (! (bool) l.getProperties().getWithDefault ("lcd", false))
        {
            juce::LookAndFeel_V4::drawLabel (g, l);
            return;
        }
        const auto r = l.getLocalBounds().toFloat();
        drawLcd (g, r.reduced (0.5f));
        if (! l.isBeingEdited())
        {
            g.setColour (l.findColour (juce::Label::textColourId));
            g.setFont (hwFont ("Consolas", 11.5f));
            g.drawSingleLineText (l.getText(), juce::roundToInt (r.getCentreX()), juce::roundToInt (r.getY() + 12.5f),
                                  juce::Justification::horizontallyCentred);
        }
    }

    // lille knap (26 x 18) med LED, trykt tekst til hoejre
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        const bool  on = b.getToggleState();
        const float cy = (float) b.getHeight() * 0.5f;
        fillButton (g, { 0.5f, cy - 8.5f, 25.f, 17.f }, on, 3.f, false);
        drawLed (g, 13.f, cy, on, ledOf (b), 3.5f);
        silkText (g, b.getButtonText(), 34.f, cy + 3.5f, 9.5f, hw::silk.withAlpha (0.85f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool, bool) override
    {
        fillButton (g, b.getLocalBounds().toFloat().reduced (0.5f), b.getToggleState(), 3.f, true);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        drawLed (g, 13.f, (float) b.getHeight() * 0.5f, b.getToggleState(), ledOf (b), 3.5f);
        g.setFont (hwFont ("Bahnschrift", 12.f, "Regular", 0.3f));
        g.setColour (juce::Colour (0xffeceef3));
        g.drawSingleLineText (b.getButtonText(), 26, juce::roundToInt ((float) b.getHeight() * 0.5f + 4.f));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        drawLcd (g, juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f));
        const float ax = (float) w - 14.f, my = (float) h * 0.5f;
        juce::Path p;
        p.addTriangle (ax, my - 2.5f, ax + 3.5f, my - 6.5f, ax + 7.f, my - 2.5f);
        p.addTriangle (ax, my + 2.5f, ax + 3.5f, my + 6.5f, ax + 7.f, my + 2.5f);
        g.setColour (box.findColour (juce::ComboBox::textColourId).withAlpha (0.85f));
        g.fillPath (p);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 0, box.getWidth() - 26, box.getHeight());
        label.setBorderSize ({ 0, 5, 0, 5 });
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return hwFont ("Consolas", 12.f); }
    juce::Font getPopupMenuFont() override                 { return hwFont ("Bahnschrift", 13.f); }
};

// =============================================================================
//  LcdText: et LCD-felt med en linje tekst (opdateres af editorens timer)
// =============================================================================
class DopplerSatEditor::LcdText : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LcdText (juce::Colour c) : col (c) {}
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        drawLcd (g, r.reduced (0.5f));
        g.setColour (col);
        g.setFont (hwFont ("Consolas", 12.f));
        g.drawText (text, r, juce::Justification::centred, false);
    }
private:
    juce::Colour col;
    juce::String text;
};

// =============================================================================
//  LfoView: LFO'ens kurve (med offset) og en prik, der viser, hvor den er lige nu (som i Abletons LFO)
// =============================================================================
class DopplerSatEditor::LfoView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LfoView (DopplerSatProcessor& p) : proc (p)
    {
        setTooltip ("The LFO moves the band from side to side. Top = the band is highest in frequency, bottom = lowest. "
                    "Offset shifts the curve, so it stays longer at the top or the bottom.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto R = getLocalBounds().toFloat();
        drawLcd (g, R.reduced (0.5f));
        const auto r = R.reduced (8.f, 7.f);
        auto getP = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const int    shape  = (int) std::lround (getP ("shape"));
        const double offset = (double) getP ("offset") * 0.01;
        const double rate   = getP ("sync") > 0.5f ? 1.0 : (double) getP ("rate");
        const double sqW    = juce::jlimit (1.0e-4, 0.25, 0.008 * rate);
        const double a = proc.rndNowA.load(), b = proc.rndNowB.load();

        g.setColour (hw::lfo.withAlpha (0.12f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        auto yOf = [&] (double v) { return r.getCentreY() - (float) v * r.getHeight() * 0.5f; };

        juce::Path path;
        float dotX, dotY = yOf (proc.lfoValueNow.load());
        if (shape == 3)
        {
            // Random: et rullende spor af de vaerdier, LFO'en faktisk har haft (nyeste til hoejre)
            for (int i = 0; i < kTrace; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (kTrace - 1);
                const float yy = yOf (trace[(size_t) ((traceIdx + i) % kTrace)]);
                if (i == 0) path.startNewSubPath (x, yy); else path.lineTo (x, yy);
            }
            dotX = r.getRight();
        }
        else
        {
            // to perioder af kurven (med offset), prikken i den foerste
            for (int i = 0; i <= 400; ++i)
            {
                const double t  = 2.0 * i / 400.0;
                const double ph = t - std::floor (t);
                const double v  = P::lfoValue (shape, i == 400 ? 0.999999 : ph, offset, a, b, sqW);
                const float  x  = r.getX() + r.getWidth() * (float) i / 400.f;
                if (i == 0) path.startNewSubPath (x, yOf (v)); else path.lineTo (x, yOf (v));
            }
            dotX = r.getX() + r.getWidth() * 0.5f * (float) proc.lfoPhaseNow.load();
        }
        g.setColour (hw::lfo.withAlpha (0.18f));
        g.strokePath (path, juce::PathStrokeType (4.f));
        g.setColour (hw::lfo);
        g.strokePath (path, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colours::white);
        g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.f, 7.f);
    }

    // Kaldes af editorens timer (30 Hz): gem LFO'ens vaerdi til Random-sporet
    void sample()
    {
        trace[(size_t) traceIdx] = proc.lfoValueNow.load();
        traceIdx = (traceIdx + 1) % kTrace;
    }

private:
    static constexpr int kTrace = 150;           // 5 sekunder ved 30 Hz
    DopplerSatProcessor& proc;
    std::array<double, kTrace> trace {};
    int traceIdx = 0;
};

// =============================================================================
//  PlotArea: spektrum, omraadet LFO'en flytter baandet hen over, og baandet lige nu (med saturationskurven)
// =============================================================================
class DopplerSatEditor::PlotArea : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PlotArea (DopplerSatProcessor& p) : proc (p)
    {
        setOpaque (true);
        setTooltip ("Drag left / right: Frequency (the middle of the band's sweep). Drag up / down: Drive.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = plotRect();
        g.fillAll (juce::Colour (0xff030404));
        juce::ColourGradient bg (juce::Colour (0xff060807), r.getX(), r.getY(), juce::Colour (0xff0a0e0c), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 2.f);

        drawGrid (g, r);
        drawRange (g, r);
        drawSpectrum (g, r, proc.outAnalyzer, outputColour, 0.40f, 1.f);
        drawInputLine (g, r);
        drawBand (g, r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        startFreq  = getP ("freq");
        startDrive = getP ("drive");
        startPos   = e.position;
        for (auto* id : { "freq", "drive" })
            if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = plotRect();
        const float octPerPx = std::log2 (P::maxFreq / P::minFreq) / r.getWidth();
        const float f = juce::jlimit (P::minFreq, P::maxFreq, startFreq * std::exp2 ((e.position.x - startPos.x) * octPerPx));
        const float n0 = std::pow (startDrive / 400.f, satdsp::kDriveAxisExp);
        const float n  = juce::jlimit (0.f, 1.f, n0 - (e.position.y - startPos.y) / (r.getHeight() - driveTop));
        setP ("freq", f);
        setP ("drive", 400.f * std::pow (n, 1.f / (float) satdsp::kDriveAxisExp));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto* id : { "freq", "drive" })
            if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override      // nulstil Frequency og Drive
    {
        for (auto* id : { "freq", "drive" })
            if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue());
    }

private:
    static constexpr float driveTop = 14.f;

    float getP (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void  setP (const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    juce::Rectangle<float> plotRect() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (46.f).withTrimmedRight (46.f).withTrimmedBottom (24.f).withTrimmedTop (4.f);
    }

    float freqToX (double f) const
    {
        const auto r = plotRect();
        return r.getX() + r.getWidth() * (float) (std::log (f / P::minFreq) / std::log (P::maxFreq / P::minFreq));
    }
    float xToFreq (float x) const
    {
        const auto r = plotRect();
        return P::minFreq * std::pow (P::maxFreq / P::minFreq, juce::jlimit (0.f, 1.f, (x - r.getX()) / r.getWidth()));
    }
    float dbToY (float db, juce::Rectangle<float> r) const { return r.getY() + r.getHeight() * (1.f - (db - P::minDb) / (-P::minDb)); }
    float driveToY (float pct, juce::Rectangle<float> r) const
    {
        const float n = std::pow (juce::jlimit (0.f, 400.f, pct) / 400.f, (float) satdsp::kDriveAxisExp);
        return r.getBottom() - (r.getHeight() - driveTop) * n;
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        static const float freqs[] = { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 16000.f };
        const auto grid = juce::Colour (0xff141d19), axis = juce::Colour (0xff76837c);
        g.setFont (hwFont ("Consolas", 10.f));
        for (auto f : freqs)
        {
            const float x = freqToX (f);
            g.setColour (grid);
            g.drawVerticalLine ((int) std::round (x), r.getY(), r.getBottom());
            g.setColour (axis);
            g.drawSingleLineText (f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f),
                                  juce::roundToInt (x), juce::roundToInt (r.getBottom() + 16.f), juce::Justification::horizontallyCentred);
        }
        for (int db = 0; db >= (int) P::minDb; db -= 20)
        {
            const float y = dbToY ((float) db, r);
            g.setColour (grid);
            g.drawHorizontalLine ((int) std::round (y), r.getX(), r.getRight());
            g.setColour (axis);
            g.drawSingleLineText (juce::String (db) + " dB", juce::roundToInt (r.getX() - 8.f), juce::roundToInt (y + 3.5f), juce::Justification::right);
        }
        for (auto d : { 0.f, 25.f, 100.f, 400.f })
        {
            const float y = driveToY (d, r);
            g.setColour (driveColour.withAlpha (0.8f));
            g.drawSingleLineText (juce::String ((int) d) + "%", juce::roundToInt (r.getRight() + 6.f), juce::roundToInt (y + 3.5f));
        }
    }

    // Hvor LFO'en kan flytte baandet hen (med offset kan den ikke naa begge ender)
    void drawRange (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const double off = (double) getP ("offset") * 0.01;
        double lo1, hi1, lo2, hi2;
        proc.bandEdgesFor (juce::jlimit (-1.0, 1.0, -1.0 + off), lo1, hi1);
        proc.bandEdgesFor (juce::jlimit (-1.0, 1.0,  1.0 + off), lo2, hi2);
        const float x1 = freqToX (lo1), x2 = freqToX (hi2);
        g.setColour (bandColour.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (x1, r.getY(), x2 - x1, r.getHeight()));
        g.setColour (bandColour.withAlpha (0.35f));
        const float dash[] = { 4.f, 4.f };
        for (float x : { x1, x2 })
            g.drawDashedLine (juce::Line<float> (x, r.getY(), x, r.getBottom()), dash, 2, 1.f);
        // Frequency (midten af omraadet) som en lille trekant nederst
        const float xc = freqToX (getP ("freq"));
        juce::Path tri;
        tri.addTriangle (xc - 5.f, r.getBottom(), xc + 5.f, r.getBottom(), xc, r.getBottom() - 8.f);
        g.setColour (bandColour.withAlpha (0.8f));
        g.fillPath (tri);
    }

    juce::Path spectrumPath (const P::Analyzer& an, juce::Rectangle<float> r) const
    {
        juce::Path path;
        const double fsBase = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
        const double binHz  = fsBase / (double) P::Analyzer::size;
        const int    last   = P::Analyzer::size / 2;
        bool started = false;
        for (float px = r.getX(); px <= r.getRight(); px += 1.5f)
        {
            const double b0 = (double) xToFreq (px - 0.75f) / binHz, b1 = (double) xToFreq (px + 0.75f) / binHz;
            if (b0 >= (double) last) break;
            float db;
            if (b1 - b0 < 1.0)
            {
                const double pos = 0.5 * (b0 + b1);
                const int    i   = juce::jlimit (1, last - 1, (int) pos);
                const float  fr  = (float) juce::jlimit (0.0, 1.0, pos - (double) i);
                db = an.mags[(size_t) i].load() * (1.f - fr) + an.mags[(size_t) (i + 1)].load() * fr;
            }
            else
            {
                const int i0 = juce::jlimit (1, last, (int) std::ceil (b0)), i1 = juce::jlimit (1, last, (int) std::floor (b1));
                db = -100.f;
                for (int i = i0; i <= i1; ++i) db = juce::jmax (db, an.mags[(size_t) i].load());
            }
            const float y = juce::jlimit (r.getY(), r.getBottom(), dbToY (db, r));
            if (! started) { path.startNewSubPath (px, y); started = true; } else path.lineTo (px, y);
        }
        return path;
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r, const P::Analyzer& an, juce::Colour col, float fillA, float lineA) const
    {
        juce::Path line = spectrumPath (an, r);
        if (line.isEmpty()) return;
        juce::Path filled (line);
        filled.lineTo (r.getRight(), r.getBottom());
        filled.lineTo (r.getX(), r.getBottom());
        filled.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col.withAlpha (fillA), r.getX(), r.getY(), col.withAlpha (0.f), r.getX(), r.getBottom(), false));
        g.fillPath (filled);
        g.setColour (col.withAlpha (0.15f));
        g.strokePath (line, juce::PathStrokeType (4.f, juce::PathStrokeType::curved));
        g.setColour (col.withAlpha (lineA));
        g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
    }

    void drawInputLine (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        juce::Path line = spectrumPath (proc.inAnalyzer, r);
        g.setColour (inputColour.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (1.f, juce::PathStrokeType::curved));

        g.setFont (hwFont ("Bahnschrift", 10.f, "Bold"));
        float lx = r.getRight() - 8.f;
        const char*        texts[] = { "Output", "Input" };
        const juce::Colour cols[]  = { outputColour, inputColour };
        for (int k = 0; k < 2; ++k)
        {
            const char* text = texts[k];
            const auto  col  = cols[k];
            const float w = 6.f * (float) juce::String (text).length() + 14.f;
            lx -= w;
            g.setColour (col);
            g.fillEllipse (lx, r.getY() + 8.f, 7.f, 7.f);
            g.drawText (text, juce::Rectangle<float> (lx + 10.f, r.getY() + 4.f, w, 15.f), juce::Justification::left);
        }
    }

    // Baandet lige nu: kurven er den samme model, som lyden bruger (CurveModel, 0 ms-tilstanden)
    void drawBand (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const double lo = proc.bandLoNow.load(), hi = proc.bandHiNow.load();
        const auto ep = proc.buildEngineParams (lo, hi);
        const auto cm = satdsp::makeCurveModel (ep, proc.getProcessingRate());

        juce::Path fill, line;
        bool started = false;
        for (float x = r.getX(); x <= r.getRight() + 0.01f; x += 1.f)
        {
            double contrib[satdsp::kBands];
            cm.contribPct ((double) xToFreq (x), contrib);
            const float y = driveToY ((float) contrib[P::kBand], r);
            if (! started) { line.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); started = true; }
            else           line.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (r.getRight(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (bandColour.withAlpha (0.35f), 0.f, r.getY(), bandColour.withAlpha (0.08f), 0.f, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (bandColour);
        g.strokePath (line, juce::PathStrokeType (1.6f));

        // kanterne og drive-stregen
        const float x1 = freqToX (lo), x2 = freqToX (hi);
        g.setColour (bandColour.withAlpha (0.9f));
        g.drawVerticalLine ((int) std::round (x1), r.getY(), r.getBottom());
        g.drawVerticalLine ((int) std::round (x2), r.getY(), r.getBottom());
        const float yd = driveToY (getP ("drive"), r);
        const float dash[] = { 5.f, 3.f };
        g.setColour (driveColour);
        g.drawDashedLine (juce::Line<float> (x1, yd, x2, yd), dash, 2, 1.5f);
        g.setFont (hwFont ("Bahnschrift", 10.f, "Bold"));
        g.drawSingleLineText ("Drive " + juce::String (juce::roundToInt (getP ("drive"))) + "%", juce::roundToInt (x1 + 4.f),
                              juce::roundToInt (juce::jmax (r.getY() + 12.f, yd - 5.f)));
    }

    DopplerSatProcessor& proc;
    float startFreq = 600.f, startDrive = 100.f;
    juce::Point<float> startPos;
};

// =============================================================================
//  Editor
// =============================================================================
static void styleKnob (juce::Slider& s, const juce::String& suffix, juce::Colour col, int radius = 20)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 17);
    s.setTextValueSuffix (suffix);
    s.setMouseDragSensitivity (220);
    s.setColour (juce::Slider::rotarySliderFillColourId,  col);
    s.setColour (juce::Slider::textBoxTextColourId,       col);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.getProperties().set ("knobR", radius);
}

static void placeKnob (juce::Slider& s, float cx, float cy, float valueTop, int r = 20)
{
    const int top = (int) std::floor (cy - (float) r - 12.f);
    const int w   = juce::jmax (70, 2 * (r + 12));
    s.getProperties().set ("knobCy", (double) (cy - (float) top));
    s.setBounds (juce::roundToInt (cx - (float) w * 0.5f), top, w, juce::roundToInt (valueTop) + 17 - top);
}

static void groove (juce::Graphics& g, float x1, float y1, float x2, float y2)
{
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (x1, y1, x2, y2, 1.f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawLine (x1 + (x1 == x2 ? 1.f : 0.f), y1 + (y1 == y2 ? 1.f : 0.f), x2 + (x1 == x2 ? 1.f : 0.f), y2 + (y1 == y2 ? 1.f : 0.f), 1.f);
}

namespace
{
struct Layout
{
    int mY = 0, hintY = 0;
    juce::Rectangle<int> plot;
    int mx[4] {}, mw[4] {};
};

Layout makeLayout()
{
    Layout L;
    const int W = DopplerSatEditor::kWidth, H = DopplerSatEditor::kHeight;
    L.mY    = H - 12 - layout::modH;
    L.hintY = L.mY - 18;
    L.plot  = { 28, 50, W - 56, L.hintY - 8 - 50 };
    const int avail = W - 32 - 3 * layout::gap;
    int x = 16;
    for (int i = 0; i < 4; ++i)
    {
        L.mw[i] = (int) std::floor ((float) avail * layout::widths[i]);
        L.mx[i] = x;
        x += L.mw[i] + layout::gap;
    }
    return L;
}
}

DopplerSatEditor::DopplerSatEditor (DopplerSatProcessor& p)
    : AudioProcessorEditor (&p), look (std::make_unique<HardwareLook>()), processor (p)
{
    plot = std::make_unique<PlotArea> (processor);
    plot->setComponentID ("plot");
    addAndMakeVisible (*plot);
    lfoView = std::make_unique<LfoView> (processor);
    addAndMakeVisible (*lfoView);
    rangeLcd = std::make_unique<LcdText> (hw::green);
    rangeLcd->setTooltip ("The frequencies the LFO moves the band across (Frequency +/- Amount, plus the band's width).");
    addAndMakeVisible (*rangeLcd);
    nowLcd = std::make_unique<LcdText> (hw::green);
    nowLcd->setTooltip ("Where the band is right now.");
    addAndMakeVisible (*nowLcd);

    // ---- LFO
    styleKnob (rateSlider, " Hz", hw::lfo);
    rateSlider.setTooltip ("How fast the band moves: from very slow (0.01 Hz = once every 100 seconds) to very fast (20 Hz). "
                           "The edges move at most 40 (24 dB) or 20 (48 dB) octaves per second, so at fast rates the sweep "
                           "gets smaller. Faster made the level jump.");
    styleKnob (syncRateSlider, "", hw::lfo);
    syncRateSlider.setTooltip ("How fast the band moves, in note values at the song's tempo (bars, . = dotted, T = triplet).");
    styleKnob (offsetSlider, " %", hw::lfo);
    offsetSlider.getProperties().set ("bipolar", true);
    offsetSlider.setTooltip ("Shifts the LFO curve up or down. It is clipped at the top and bottom, so the band stays longer "
                             "at the high or the low end of its sweep.");
    shapeBox.setColour (juce::ComboBox::textColourId, hw::lfo);
    shapeBox.addItemList (P::shapeNames(), 1);
    shapeBox.setTooltip ("The LFO's curve: how the band moves from side to side.");
    for (auto* b : { &hzButton, &syncButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (201);
        b->getProperties().set ("led", (juce::int64) hw::lfo.getARGB());
    }
    hzButton.setButtonText ("Hz");
    syncButton.setButtonText ("Sync");
    hzButton.setTooltip ("Rate in Hz (free)");
    syncButton.setTooltip ("Rate in note values, locked to the song's tempo and bar");
    hzButton.onClick   = [this] { if (hzButton.getToggleState()   && syncAtt) syncAtt->setValueAsCompleteGesture (0.f); };
    syncButton.onClick = [this] { if (syncButton.getToggleState() && syncAtt) syncAtt->setValueAsCompleteGesture (1.f); };

    // ---- Baand
    styleKnob (freqSlider,   " Hz", hw::green);
    freqSlider.setTooltip ("The middle of the band's sweep. You can also drag left / right in the display.");
    styleKnob (amountSlider, " %",  hw::green);
    amountSlider.setTooltip ("How far the LFO moves the band: 100 % = 3 octaves to each side. More movement = more pitch change.");
    styleKnob (widthSlider,  " oct", hw::green);
    widthSlider.setTooltip ("The band's width in octaves.");

    // ---- Saturation
    styleKnob (driveSlider, " %", hw::orange);
    driveSlider.setTooltip ("How hard the band is saturated. The pitch effect is there even at 0 %.");
    typeBox.setColour (juce::ComboBox::textColourId, hw::amber);
    typeBox.addItemList (P::distortionNames(), 1);
    static const char* modeTip[] = {
        "0 ms, soft edges (24 dB/oct): a gentle pitch bend when the band moves, and the saturation blends softly.",
        "0 ms, firmer edges (48 dB/oct): about three times more pitch bend when the band moves." };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = modeButtons[i];
        b.setButtonText (P::modeNames()[i]);
        b.setTooltip (modeTip[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (202);
        b.getProperties().set ("led", (juce::int64) hw::amber.getARGB());
        b.onClick = [this, i] { if (modeButtons[i].getToggleState() && modeAtt) modeAtt->setValueAsCompleteGesture ((float) i); };
    }

    // ---- Output
    styleKnob (dryWetSlider, " %", hw::purple);
    dryWetSlider.setTooltip ("Dry = the untouched sound, Wet = with the moving band. In between, the two mix like a phaser.");
    styleKnob (outSlider, " dB", hw::purple);
    bypassButton.setButtonText ("Bypass");
    bypassButton.getProperties().set ("led", (juce::int64) hw::red.getARGB());

    juce::Component* children[] = { &rateSlider, &syncRateSlider, &offsetSlider, &shapeBox, &hzButton, &syncButton,
                                    &freqSlider, &amountSlider, &widthSlider, &driveSlider, &typeBox,
                                    &modeButtons[0], &modeButtons[1], &dryWetSlider, &outSlider, &bypassButton };
    for (auto* c : children) addAndMakeVisible (*c);

    // Looket til sidst, naar alle boern er tilfoejet (saa faar tal-felterne og vaelgerne det nye look)
    setLookAndFeel (look.get());

    auto& s = processor.apvts;
    rateAtt     = std::make_unique<SliderAtt> (s, "rate",     rateSlider);
    syncRateAtt = std::make_unique<SliderAtt> (s, "syncrate", syncRateSlider);
    offsetAtt   = std::make_unique<SliderAtt> (s, "offset",   offsetSlider);
    freqAtt     = std::make_unique<SliderAtt> (s, "freq",     freqSlider);
    amountAtt   = std::make_unique<SliderAtt> (s, "amount",   amountSlider);
    widthAtt    = std::make_unique<SliderAtt> (s, "width",    widthSlider);
    driveAtt    = std::make_unique<SliderAtt> (s, "drive",    driveSlider);
    dryWetAtt   = std::make_unique<SliderAtt> (s, "drywet",   dryWetSlider);
    outAtt      = std::make_unique<SliderAtt> (s, "outgain",  outSlider);
    shapeAtt    = std::make_unique<ComboAtt>  (s, "shape",    shapeBox);
    typeAtt     = std::make_unique<ComboAtt>  (s, "type",     typeBox);
    bypassAtt   = std::make_unique<ButtonAtt> (s, "bypass",   bypassButton);
    syncAtt = std::make_unique<juce::ParameterAttachment> (*s.getParameter ("sync"), [this] (float v) { updateSync (v > 0.5f); });
    syncAtt->sendInitialUpdate();
    modeAtt = std::make_unique<juce::ParameterAttachment> (*s.getParameter ("mode"), [this] (float v) { updateMode ((int) std::lround (v)); });
    modeAtt->sendInitialUpdate();

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

DopplerSatEditor::~DopplerSatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DopplerSatEditor::updateSync (bool sync)
{
    hzButton.setToggleState (! sync, juce::dontSendNotification);
    syncButton.setToggleState (sync, juce::dontSendNotification);
    rateSlider.setVisible (! sync);
    syncRateSlider.setVisible (sync);
}

void DopplerSatEditor::updateMode (int mode)
{
    for (int i = 0; i < 2; ++i) modeButtons[i].setToggleState (i == mode, juce::dontSendNotification);
}

void DopplerSatEditor::timerCallback()
{
    plot->repaint();
    lfoView->sample();
    lfoView->repaint();
    const double off = (double) processor.apvts.getRawParameterValue ("offset")->load() * 0.01;
    double lo1, hi1, lo2, hi2;
    processor.bandEdgesFor (juce::jlimit (-1.0, 1.0, -1.0 + off), lo1, hi1);
    processor.bandEdgesFor (juce::jlimit (-1.0, 1.0,  1.0 + off), lo2, hi2);
    rangeLcd->setText (hzText (lo1) + " - " + hzText (hi2));
    nowLcd->setText (hzText (processor.bandLoNow.load()) + " - " + hzText (processor.bandHiNow.load()));
}

void DopplerSatEditor::resized()
{
    const auto L = makeLayout();
    const float mY = (float) L.mY;
    auto y = [&] (float v) { return juce::roundToInt (mY + v); };
    plot->setBounds (L.plot);

    // LFO: kurven, Shape + Hz / Sync, Rate og Offset
    {
        const int x = L.mx[0], w = L.mw[0];
        lfoView->setBounds (x + 12, y (28), w - 24, 58);
        shapeBox.setBounds (x + 60, y (94), 94, 22);
        hzButton.setBounds (x + w - 12 - 106, y (94), 48, 22);
        syncButton.setBounds (x + w - 12 - 54, y (94), 54, 22);
        placeKnob (rateSlider,     (float) x + (float) w * 0.3f, mY + layout::knob, mY + layout::value);
        placeKnob (syncRateSlider, (float) x + (float) w * 0.3f, mY + layout::knob, mY + layout::value);
        placeKnob (offsetSlider,   (float) x + (float) w * 0.7f, mY + layout::knob, mY + layout::value);
    }
    // Baand: omraadet og baandet lige nu, Frequency / Amount / Width
    {
        const int x = L.mx[1], w = L.mw[1];
        rangeLcd->setBounds (x + 12, y (42), w - 24, 22);
        nowLcd->setBounds (x + 12, y (92), w - 24, 22);
        for (int i = 0; i < 3; ++i)
            placeKnob (*std::array<juce::Slider*, 3> { &freqSlider, &amountSlider, &widthSlider }[(size_t) i],
                       (float) (x + 12) + (float) (w - 24) / 3.f * ((float) i + 0.5f), mY + layout::knob, mY + layout::value);
    }
    // Saturation: Distortion, de to 0 ms-tilstande, Drive
    {
        const int x = L.mx[2], w = L.mw[2];
        typeBox.setBounds (x + 12, y (42), w - 24, 22);
        modeButtons[0].setBounds (x + 12, y (72), w - 24, 22);
        modeButtons[1].setBounds (x + 12, y (99), w - 24, 22);
        placeKnob (driveSlider, (float) x + (float) w * 0.5f, mY + layout::knob, mY + layout::value);
    }
    // Output: Bypass, Dry / Wet og Output gain
    {
        const int x = L.mx[3], w = L.mw[3];
        bypassButton.setBounds (x + 12, y (42), w - 24, 22);
        placeKnob (dryWetSlider, (float) x + (float) w * 0.27f, mY + layout::knob, mY + layout::value);
        placeKnob (outSlider,    (float) x + (float) w * 0.73f, mY + layout::knob, mY + layout::value);
    }
}

void DopplerSatEditor::renderPlate()
{
    const int W = getWidth(), H = getHeight();
    plate = juce::Image (juce::Image::RGB, W, H, false);
    juce::Graphics g (plate);
    const auto  L  = makeLayout();
    const float mY = (float) L.mY;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d43), 0.f, 0.f, juce::Colour (0xff26282c), 0.f, (float) H, false));
    g.fillAll();
    for (int yy = 1; yy < H; yy += 3)
    {
        g.setColour (juce::Colours::white.withAlpha (yy % 9 == 1 ? 0.035f : 0.015f));
        g.drawHorizontalLine (yy, 0.f, (float) W);
    }
    for (auto c : { juce::Point<float> (10.f, 10.f), juce::Point<float> ((float) W - 10.f, 10.f),
                    juce::Point<float> (10.f, (float) H - 10.f), juce::Point<float> ((float) W - 10.f, (float) H - 10.f) })
    {
        constexpr float r = 5.5f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9ea8), c.x - 0.3f * r, c.y - 0.4f * r,
                                                 juce::Colour (0xff3a3c42), c.x - 0.3f * r + 1.6f * r, c.y - 0.4f * r, true));
        g.fillEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r, 1.f);
        g.setColour (juce::Colour (0xff1a1b1e));
        g.drawLine (c.x - 3.5f, c.y - 1.5f, c.x + 3.5f, c.y + 1.5f, 1.4f);
    }

    g.setFont (hwFont ("Bahnschrift", 15.f, "SemiBold", 3.f));
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawSingleLineText ("DOPPLER SATURATOR", 30, 28);
    silkText (g, "Moving band  -  pitch bends as its edges sweep across the sound", (float) W - 30.f, 27.f, 9.5f,
              hw::silkDim, juce::Justification::right, 0.6f);

    const auto R = L.plot.toFloat();
    g.setColour (juce::Colour (0xff111214));
    g.fillRoundedRectangle (R.expanded (7.f), 6.f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (R.expanded (7.f), 6.f, 1.5f);
    g.setColour (juce::Colour (0xff5a5e66).withAlpha (0.55f));
    g.drawRoundedRectangle (R.expanded (6.f), 5.f, 1.f);

    g.setFont (hwFont ("Bahnschrift", 10.f));
    g.setColour (hw::silkDim);
    g.drawSingleLineText ("Drag left / right: Frequency  |  Drag up / down: Drive  |  Double-click: reset  |  "
                          "Shaded: where the LFO moves the band", W / 2, L.hintY + 12, juce::Justification::horizontallyCentred);

    static const char* titles[] = { "LFO", "Band", "Saturation", "Output" };
    for (int i = 0; i < 4; ++i)
    {
        const juce::Rectangle<float> m ((float) L.mx[i], mY, (float) L.mw[i], (float) layout::modH);
        g.setColour (juce::Colour (0xff1c1e21).withAlpha (0.55f));
        g.fillRoundedRectangle (m, 5.f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (m, 5.f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (m.reduced (1.f), 4.f, 1.f);
        const juce::String t = titles[i];
        silkText (g, t, m.getX() + 12.f, mY + layout::title, 11.f, hw::silk);
        const float x1 = m.getX() + 12.f + (float) t.length() * 7.4f + 8.f, x2 = m.getRight() - 12.f;
        if (x2 - x1 > 8.f) groove (g, x1, mY + 13.f, x2, mY + 13.f);
    }

    // faste labels
    const auto silkC = hw::silk.withAlpha (0.85f);
    silkText (g, "Shape", (float) L.mx[0] + 12.f, mY + 109.f, 9.5f, silkC);
    silkText (g, "Sweep range", (float) L.mx[1] + 12.f, mY + 37.f, 9.5f, silkC);
    silkText (g, "Band now", (float) L.mx[1] + 12.f, mY + 87.f, 9.5f, silkC);
    silkText (g, "Distortion", (float) L.mx[2] + 12.f, mY + 37.f, 9.5f, silkC);
    auto knobText = [&] (int mod, float fx, const juce::String& main, const juce::String& sub)
    {
        const float x = (float) L.mx[mod] + fx;
        silkText (g, main, x, mY + layout::knobLabel, 10.5f, silkC, juce::Justification::horizontallyCentred);
        if (sub.isNotEmpty())
            silkText (g, sub, x, mY + layout::knobSub, 8.5f, hw::silkDim, juce::Justification::horizontallyCentred, 0.4f, false);
    };
    knobText (0, (float) L.mw[0] * 0.3f, "Rate", "speed");
    knobText (0, (float) L.mw[0] * 0.7f, "Offset", "stay at top / bottom");
    for (int i = 0; i < 3; ++i)
    {
        static const char* m[] = { "Frequency", "Amount", "Width" }, *sb[] = { "middle", "how far", "octaves" };
        knobText (1, 12.f + (float) (L.mw[1] - 24) / 3.f * ((float) i + 0.5f), m[i], sb[i]);
    }
    knobText (2, (float) L.mw[2] * 0.5f, "Drive", "saturation");
    knobText (3, (float) L.mw[3] * 0.27f, "Dry / Wet", "");
    knobText (3, (float) L.mw[3] * 0.73f, "Output", "gain");
}

void DopplerSatEditor::paint (juce::Graphics& g)
{
    if (plate.getWidth() != getWidth() || plate.getHeight() != getHeight())
        renderPlate();
    g.drawImageAt (plate, 0, 0);
}
