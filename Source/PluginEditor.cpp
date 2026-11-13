#include "PluginEditor.h"

using P = DopplerSatProcessor;

// =============================================================================
//  Maal (fast vindue 900 x 600). y i modulerne regnes fra modulets top.
// =============================================================================
namespace layout
{
    constexpr int   modH  = 234, gap = 8;
    // 1.9 (brugeren 2026-11-10, mockuppet): intet Edges-modul (bredde 0), Edges-knapperne ligger som en soejle i Band.
    // Saa ligger drejeknapperne naesten lige langt fra hinanden i alle tre moduler (LFO 112, Band 108, Output 102 px).
    // Til og med 1.8: 0.33 / 0.29 / 0.20 / 0.18 med Edges som eget modul og "Edge steepness"-tegningen.
    constexpr float widths[] = { 0.33f, 0.41f, 0.f, 0.26f };      // LFO, Band, (Edges), Output
    constexpr int   nMod = 3;
    constexpr int   edgeColW = 118;                               // Edges-soejlen i Band (hoejre side)
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
static const juce::Colour outputColour (0xffc77dff), bandColour (0xff50fa7b);

static juce::Font hwFont (const juce::String& name, float size, const juce::String& style = "Regular", float spacing = 0.f)
{
   #if JUCE_MAC
    // 1.5: Mac har hverken Bahnschrift eller Consolas (Windows-skrifter). Naermeste skrifter, der altid findes paa
    // macOS: Avenir Next (har Regular / Demi Bold / Bold) og Menlo (fast bredde til displayene).
    const juce::String macName  = name == "Bahnschrift" ? "Avenir Next" : name == "Consolas" ? "Menlo" : name;
    const juce::String macStyle = (name == "Bahnschrift" && style == "SemiBold") ? "Demi Bold" : style;
    juce::Font f (juce::FontOptions().withName (macName).withStyle (macStyle).withPointHeight (size));
   #else
    juce::Font f (juce::FontOptions().withName (name).withStyle (style).withPointHeight (size));
   #endif
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

    // "compact" (1.3, Hz / Sync): LED og tekst laengere til venstre, saa "Sync" kan ses helt i den smalle knap
    // (brugeren 2026-11-08: "Teksten (ikke boksen) skal rykkes lidt til venstre, så hele teksten er synlig").
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool compact = b.getProperties()["compact"];
        drawLed (g, compact ? 10.f : 13.f, (float) b.getHeight() * 0.5f, b.getToggleState(), ledOf (b), 3.5f);
        g.setFont (hwFont ("Bahnschrift", 12.f, "Regular", 0.3f));
        g.setColour (juce::Colour (0xffeceef3));
        g.drawSingleLineText (b.getButtonText(), compact ? 19 : 26, juce::roundToInt ((float) b.getHeight() * 0.5f + 4.f));
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
//  1.10: med onTyped kan tallene skrives om (Sweep-displayet, brugeren 2026-11-13: "Brugeren skal kunne skrive
//  grænserne af top og bund ind ved at klikke på displayet"). Teksten er "bund - top" (eller eet tal); klik paa et
//  tal aabner et felt over netop det tal. Enter = brug, Esc = fortryd, Tab = brug og skriv det andet tal, klik
//  udenfor feltet (paa displayet) = brug.
// =============================================================================
class DopplerSatEditor::LcdText : public juce::Component, public juce::SettableTooltipClient
{
public:
    // part: 0 = venstre tal (bund), 1 = hoejre tal (top), -1 = displayet viser kun eet tal
    std::function<void (int part, const juce::String& typed)> onTyped;
    // 1.14: traek op / ned i et tal. valueOf = tallets vaerdi nu (Hz), onDragged = ny vaerdi, onDragGesture = traek
    // begynder (true) / slutter (false), saa DAW'en optager det som een bevaegelse
    std::function<double (int part)> valueOf;
    std::function<void (int part, double hz)> onDragged;
    std::function<void (bool begin)> onDragGesture;

    explicit LcdText (juce::Colour c) : col (c) {}
    ~LcdText() override { if (field != nullptr) removeChildComponent (field.get()); }

    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setEditable (bool e) { setMouseCursor (e ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        drawLcd (g, r.reduced (0.5f));
        g.setColour (col);
        g.setFont (font());
        g.drawText (text, r, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! onTyped || e.mods.isPopupMenu()) return;
        if (field != nullptr) endEdit (true, false);
        downPart = partAt ((float) e.x);
        lastDragY = 0;
        dragging = false;
    }

    // 1.14 (brugeren 2026-11-13): "Ved at trække lodret op og ned ud for hver tal i sweepdisplayet, kan man justere
    // tallet for det tal, man har fat i." Op = hoejere frekvens. 0.02 oktav pr. pixel (100 px = 2 oktaver), med Shift
    // 0.002. Et klik uden traek aabner tekstfeltet som foer.
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! onTyped || ! onDragged || e.mods.isPopupMenu()) return;
        const int dy = e.getDistanceFromDragStartY();
        if (! dragging)
        {
            if (std::abs (dy) < 3) return;
            beginDrag (downPart >= 0 ? downPart : (dy < 0 ? 1 : 0));   // eet tal (Sweep 0): op = toppen, ned = bunden
            if (! dragging) return;
        }
        dragBy ((float) (dy - lastDragY), e.mods.isShiftDown());
        lastDragY = dy;
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! onTyped || e.mods.isPopupMenu()) return;
        if (dragging) { endDrag(); return; }
        beginEdit (downPart);
    }

    // Traek (ogsaa til EditorShot): part 0 = bund, 1 = top; dy i pixel (positiv = ned)
    void beginDrag (int part)
    {
        dragPart = part;
        dragHz = valueOf ? valueOf (part) : 0.0;
        dragging = dragHz > 0.0;
        if (dragging && onDragGesture) onDragGesture (true);
    }
    void dragBy (float dy, bool fine)
    {
        if (! dragging) return;
        dragHz = juce::jlimit (20.0, 20000.0, dragHz * std::exp2 (-(double) dy * (fine ? 0.002 : 0.02)));
        if (onDragged) onDragged (dragPart, dragHz);
    }
    void endDrag()
    {
        if (! dragging) return;
        dragging = false;
        if (onDragGesture) onDragGesture (false);
    }

    // Aabner feltet over et tal (ogsaa til EditorShot)
    void beginEdit (int part)
    {
        if (field != nullptr) endEdit (false, false);
        const int sep = text.indexOf (" - ");
        if (sep < 0) part = -1;
        else if (part < 0) part = 0;
        const auto f   = font();
        const float w  = (float) getWidth();
        const float x0 = 0.5f * (w - juce::GlyphArrangement::getStringWidth (f, text));
        juce::Rectangle<int> box;
        juce::String num = text;
        if (part == 0)
        {
            num = text.substring (0, sep);
            box = { 3, 2, juce::roundToInt (x0 + juce::GlyphArrangement::getStringWidth (f, text.substring (0, sep + 1))) - 3, getHeight() - 4 };
        }
        else if (part == 1)
        {
            num = text.substring (sep + 3);
            const int left = juce::roundToInt (x0 + juce::GlyphArrangement::getStringWidth (f, text.substring (0, sep + 2)));
            box = { left, 2, getWidth() - 3 - left, getHeight() - 4 };
        }
        else
        {
            box = getLocalBounds().reduced (3, 2);
        }
        editPart = part;
        startText = num;
        field = std::make_unique<Field> (*this);
        field->setFont (f);
        field->setJustification (juce::Justification::centred);
        field->setIndents (2, juce::jmax (0, (box.getHeight() - (int) f.getHeight()) / 2));
        field->setColour (juce::TextEditor::textColourId, col);
        field->setInputRestrictions (16, "0123456789.,kKhHzZ ");
        field->setText (num, false);
        field->setBounds (box);
        field->onReturnKey = [this] { endEdit (true, false); };
        field->onEscapeKey = [this] { endEdit (false, false); };
        field->onFocusLost = [this] { endEdit (true, false); };
        addAndMakeVisible (*field);
        field->grabKeyboardFocus();
        field->selectAll();
    }

    bool isEditing() const { return field != nullptr; }

private:
    // Tab = brug tallet og skriv det andet
    struct Field : public juce::TextEditor
    {
        explicit Field (LcdText& o) : owner (o) {}
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::tabKey) { owner.endEdit (true, true); return true; }
            return juce::TextEditor::keyPressed (k);
        }
        LcdText& owner;
    };

    static juce::Font font() { return hwFont ("Consolas", 12.f); }

    // Hvilket tal ligger x i? Venstre for midten af " - " = bunden.
    int partAt (float x) const
    {
        const int sep = text.indexOf (" - ");
        if (sep < 0) return -1;
        const auto f = font();
        const float x0 = 0.5f * ((float) getWidth() - juce::GlyphArrangement::getStringWidth (f, text));
        return x < x0 + juce::GlyphArrangement::getStringWidth (f, text.substring (0, sep + 2)) ? 0 : 1;
    }

    void endEdit (bool apply, bool thenOther)
    {
        if (field == nullptr) return;
        const juce::String typed = field->getText().trim();
        const int part = editPart;
        // Feltet slettes foerst bagefter (vi kan vaere inde i dets egen tastatur- eller fokus-kald)
        auto* old = field.release();
        removeChildComponent (old);
        juce::MessageManager::callAsync ([old] { delete old; });
        // Uaendret tekst = ingenting (ellers ville det afrundede tal flytte enden en smule)
        if (apply && typed.isNotEmpty() && typed != startText && onTyped) onTyped (part, typed);
        if (thenOther && part >= 0)
        {
            juce::Component::SafePointer<LcdText> safe (this);
            juce::MessageManager::callAsync ([safe, part] { if (safe != nullptr) safe->beginEdit (1 - part); });
        }
    }

    juce::Colour col;
    juce::String text, startText;
    std::unique_ptr<Field> field;
    int editPart = 0;
    int downPart = 0, dragPart = 0, lastDragY = 0;      // 1.14: traek
    bool dragging = false;
    double dragHz = 0.0;
};

// 1.10: et skrevet tal i Hz: "180", "180 Hz", "1.2k", "1,2 kHz" (komma = punktum, k = tusind). -1 = ikke et tal.
static double parseHz (juce::String s)
{
    s = s.toLowerCase().replaceCharacter (',', '.').removeCharacters (" \t");
    const auto num = s.initialSectionContainingOnly ("0123456789.");
    if (! num.containsAnyOf ("0123456789")) return -1.0;
    const double v = num.getDoubleValue() * (s.containsChar ('k') ? 1000.0 : 1.0);
    return v > 0.0 ? v : -1.0;
}

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
//  PlotArea: spektrum (output), omraadet LFO'en flytter baandet hen over, og baandet lige nu
// =============================================================================
class DopplerSatEditor::PlotArea : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PlotArea (DopplerSatProcessor& p) : proc (p)
    {
        setOpaque (true);
        setTooltip ("Drag left / right: Frequency (the middle of the band's sweep). Right-click: automation.");
    }

    std::function<void()> onRightClick;            // hoejreklik = Frequency's automationsmenu

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
        drawLegend (g, r);
        drawBand (g, r);
    }

    // 1.1: kun venstre / hoejre (Frequency). Op / ned var Drive, som er fjernet.
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        dragging  = true;
        startFreq = getP ("freq");
        startPos  = e.position;
        if (auto* p = proc.apvts.getParameter ("freq")) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto r = plotRect();
        const float octPerPx = std::log2 (P::maxFreq / P::minFreq) / r.getWidth();
        setP ("freq", juce::jlimit (P::minFreq, P::maxFreq, startFreq * std::exp2 ((e.position.x - startPos.x) * octPerPx)));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        if (auto* p = proc.apvts.getParameter ("freq")) p->endChangeGesture();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override      // nulstil Frequency
    {
        if (auto* p = proc.apvts.getParameter ("freq"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    }

private:
    static constexpr float driveTop = 14.f;      // baandets top (samme hoejde som i 1.0 ved 100 % drive)
    static constexpr double kShowDrive = 100.0;  // tegningens hoejde: som 1.0's standard (lyden har ingen drive)

    float getP (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void  setP (const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    juce::Rectangle<float> plotRect() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (46.f).withTrimmedRight (20.f).withTrimmedBottom (24.f).withTrimmedTop (4.f);
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
    }

    // Hvor LFO'en kan flytte baandet hen (med offset kan den ikke naa begge ender)
    void drawRange (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        // 1.6: midtens vej (Frequency +/- Sweep). Foer: fra laveste til hoejeste kant, saa Band Size gjorde den bredere.
        const double off = (double) getP ("offset") * 0.01;
        const float x1 = freqToX ((float) proc.bandCentreFor (juce::jlimit (-1.0, 1.0, -1.0 + off))),
                    x2 = freqToX ((float) proc.bandCentreFor (juce::jlimit (-1.0, 1.0,  1.0 + off)));
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

    // Forklaringen oeverst til hoejre: kun Output (1.1, brugeren: "Vis kun output på grafen og som forklaringstekst")
    void drawLegend (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        g.setFont (hwFont ("Bahnschrift", 10.f, "Bold"));
        const juce::String text ("Output");
        const float w  = 6.f * (float) text.length() + 14.f;
        const float lx = r.getRight() - 8.f - w;
        g.setColour (outputColour);
        g.fillEllipse (lx, r.getY() + 8.f, 7.f, 7.f);
        g.drawText (text, juce::Rectangle<float> (lx + 10.f, r.getY() + 4.f, w, 15.f), juce::Justification::left);
    }

    // Baandet lige nu: formen er motorens egen model (CurveModel, 0 ms-tilstanden) tegnet i samme hoejde som 1.0
    // ved 100 % drive. Lyden har ingen drive; tegningen viser bare, hvor baandet og dets kanter er.
    void drawBand (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const double lo = proc.bandLoNow.load(), hi = proc.bandHiNow.load();
        auto ep = proc.buildEngineParams (lo, hi);
        ep.band[P::kBand].drive = kShowDrive;
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

        // kanterne
        const float x1 = freqToX (lo), x2 = freqToX (hi);
        g.setColour (bandColour.withAlpha (0.9f));
        g.drawVerticalLine ((int) std::round (x1), r.getY(), r.getBottom());
        g.drawVerticalLine ((int) std::round (x2), r.getY(), r.getBottom());
    }

    DopplerSatProcessor& proc;
    float startFreq = 600.f;
    bool  dragging = false;
    juce::Point<float> startPos;
};

// =============================================================================
//  Hjaelp (som i Multiband Saturator): lille rundt "?" og en forklaringsboks, der ligger oven paa hele editoren.
//  Klik udenfor boksen, paa krydset eller Esc lukker den.
// =============================================================================
class DopplerSatEditor::HelpButton : public juce::Button
{
public:
    HelpButton() : juce::Button ("?") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    // Rund trykknap (16 px) med "?"
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        juce::ColourGradient grad (juce::Colour (down ? 0xff383b41 : (over ? 0xff676b74 : 0xff5a5e66)), 0.f, r.getY(),
                                   juce::Colour (down ? 0xff5a5e66 : 0xff383b41), 0.f, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xff0a0a0b));
        g.drawEllipse (r, 1.f);
        g.setFont (hwFont ("Bahnschrift", 11.f, "Bold"));
        g.setColour (juce::Colour (0xffeceef3));
        g.drawSingleLineText ("?", juce::roundToInt (r.getCentreX()), juce::roundToInt (r.getCentreY() + 4.f),
                              juce::Justification::horizontallyCentred);
    }
};

class DopplerSatEditor::HelpOverlay : public juce::Component
{
public:
    struct Section { juce::String heading, body; };     // overskrift (kan vaere tom) + tekst

    HelpOverlay() { setWantsKeyboardFocus (true); }

    void show (const juce::String& newTitle, std::vector<Section> newSections, juce::Colour newAccent)
    {
        title    = newTitle;
        sections = std::move (newSections);
        accent   = newAccent;
        setVisible (true);
        toFront (true);
        layoutText();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f));
        g.setColour (juce::Colour (0xff202226));
        g.fillRoundedRectangle (box, 4.f);
        g.setColour (accent.withAlpha (0.7f));
        g.drawRoundedRectangle (box, 4.f, 1.5f);

        g.setColour (accent);
        g.setFont (hwFont ("Bahnschrift", 15.f, "Bold"));
        g.drawText (title, titleRect, juce::Justification::centredLeft, true);

        g.setColour (juce::Colour (0xff9a9fab));
        g.setFont (hwFont ("Bahnschrift", 20.f));
        g.drawText (juce::String::fromUTF8 ("\xc3\x97"), closeRect, juce::Justification::centred);   // kryds

        text.draw (g, textRect);

        g.setColour (juce::Colour (0xff9a9fab).withAlpha (0.7f));
        g.setFont (hwFont ("Bahnschrift", 10.f));
        g.drawText ("Click outside the box or press Esc to close", footRect, juce::Justification::centredRight);
    }

    void resized() override { layoutText(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! box.contains (e.position) || closeRect.contains (e.position))
            setVisible (false);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k != juce::KeyPress::escapeKey) return false;
        setVisible (false);
        return true;
    }

private:
    juce::String         title;
    std::vector<Section> sections;
    juce::Colour         accent;
    juce::TextLayout     text;
    juce::Rectangle<float> box, titleRect, closeRect, textRect, footRect;

    // Boksen centreres. Er vinduet for lavt til hele teksten, bliver skriften mindre.
    void layoutText()
    {
        if (getWidth() <= 0 || getHeight() <= 0 || sections.empty()) return;
        constexpr float pad = 18.f, titleH = 22.f, footH = 14.f;
        const float w    = juce::jmin (680.f, (float) getWidth() - 32.f);
        const float maxH = (float) getHeight() - 24.f;

        for (float fh = 13.f; ; fh -= 0.5f)
        {
            juce::AttributedString as;
            as.setWordWrap (juce::AttributedString::byWord);
            as.setLineSpacing (1.5f);
            for (size_t i = 0; i < sections.size(); ++i)
            {
                if (i > 0) as.append ("\n\n", hwFont ("Bahnschrift", fh * 0.6f), juce::Colours::transparentBlack);   // halv linje luft
                if (sections[i].heading.isNotEmpty())
                    as.append (sections[i].heading + "\n", hwFont ("Bahnschrift", fh, "Bold"), accent);
                as.append (sections[i].body, hwFont ("Bahnschrift", fh), juce::Colour (0xffd3d6dd));
            }
            text.createLayout (as, w - 2.f * pad);

            const float h = pad + titleH + 10.f + text.getHeight() + 12.f + footH + 10.f;
            if (h <= maxH || fh <= 9.f)
            {
                box = getLocalBounds().toFloat().withSizeKeepingCentre (w, juce::jmin (h, maxH));
                auto r = box.reduced (pad, 0.f);
                r.removeFromTop (pad);
                titleRect = r.removeFromTop (titleH).withTrimmedRight (28.f);
                closeRect = juce::Rectangle<float> (box.getRight() - 34.f, box.getY() + 8.f, 26.f, 26.f);
                r.removeFromTop (10.f);
                textRect  = r.removeFromTop (text.getHeight());
                r.removeFromTop (12.f);
                footRect  = r.removeFromTop (footH);
                return;
            }
        }
    }
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
    const int avail = W - 32 - (layout::nMod - 1) * layout::gap;
    int x = 16;
    for (int i = 0; i < 4; ++i)
    {
        L.mw[i] = (int) std::floor ((float) avail * layout::widths[i]);
        L.mx[i] = x;
        if (L.mw[i] > 0) x += L.mw[i] + layout::gap;
    }
    return L;
}
}

DopplerSatEditor::DopplerSatEditor (DopplerSatProcessor& p)
    : AudioProcessorEditor (&p), look (std::make_unique<HardwareLook>()), processor (p)
{
    plot = std::make_unique<PlotArea> (processor);
    plot->setComponentID ("plot");
    plot->onRightClick = [this] { showParamMenu ("freq"); };
    addAndMakeVisible (*plot);
    lfoView = std::make_unique<LfoView> (processor);
    addAndMakeVisible (*lfoView);
    rangeLcd = std::make_unique<LcdText> (hw::green);
    rangeLcd->setTooltip ("Sweep: where the LFO moves the middle of the band (Frequency +/- Sweep). Band Size does not change it.\n"
                          "Drag the low or the high number up / down to change it (Shift = fine), or click it to type it "
                          "(e.g. 180, 1.2k). The other number stays where it is, and Frequency and Sweep follow.");
    rangeLcd->setName ("Sweep display");
    rangeLcd->setEditable (true);
    rangeLcd->onTyped = [this] (int part, const juce::String& typed) { applySweepText (part, typed); };
    // 1.14: traek op / ned i et tal (samme regning som skrivning; hele traekket er een aendring i DAW'en)
    rangeLcd->valueOf = [this] (int part) { double c1, c2; sweepEndsNow (c1, c2); return part == 1 ? c2 : c1; };
    rangeLcd->onDragged = [this] (int part, double hz) { applySweepHz (part, hz, false); };
    rangeLcd->onDragGesture = [this] (bool begin)
    {
        auto* fp = processor.apvts.getParameter ("freq");
        auto* ap = processor.apvts.getParameter ("amount");
        if (begin) { fp->beginChangeGesture(); ap->beginChangeGesture(); }
        else       { ap->endChangeGesture();   fp->endChangeGesture(); }
    };
    addAndMakeVisible (*rangeLcd);
    nowLcd = std::make_unique<LcdText> (hw::green);
    nowLcd->setName ("Band Size display");                 // 2.07: EditorShot tjekker, at raekkerne flugter
    nowLcd->setTooltip ("Band Size: where the band's edges are right now (they are Band Size apart, and the LFO moves them).");
    addAndMakeVisible (*nowLcd);

    // ---- LFO
    styleKnob (rateSlider, " Hz", hw::lfo);
    // 1.7: ingen fartgraense for LFO'en laengere: baandet naar altid helt ud i Sweep-omraadet
    rateSlider.setTooltip ("How fast the band moves: from very slow (0.01 Hz = once every 100 seconds) to very fast (20 Hz). "
                           "The band always travels the whole Sweep range, also at fast rates, so fast rates bend the pitch a lot "
                           "and give short level peaks.");
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
        b->getProperties().set ("compact", true);
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
    amountSlider.setTooltip ("Sweep: how wide the area is that the LFO moves the band across, centred on Frequency (100 % = the whole "
                             "spectrum, 20 Hz - 20 kHz). Near an end of the spectrum the area is pushed inwards, so it keeps its width. "
                             "It only changes the area the band travels across (the Sweep display), not the band itself. "
                             "More travel = more pitch change.");
    styleKnob (widthSlider,  " oct", hw::green);
    widthSlider.setTooltip ("Band Size: how wide the band itself is, from edge to edge (0.1 - 6 octaves). "
                            "It does not change how far the band travels.");

    // ---- Edges (1.1: Drive og Distortion er fjernet; lyden er kun kanternes Doppler-effekt)
    static const char* modeTip[] = {
        "Softer edges (24 dB/oct): a gentler, smoother pitch bend. Click ? for more.",
        "Steeper edges (48 dB/oct): a stronger, clearer pitch bend. Click ? for more.",
        "Steepest edges (96 dB/oct): the strongest pitch bend, most of all at slow rates. Click ? for more." };
    for (int i = 0; i < P::kNumModes; ++i)
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

    outSlider.setTooltip ("Output level.");
    bypassButton.setTooltip ("Bypass: the untouched sound. This is also the DAW's own bypass for the plugin.");

    // 1.6: Pause (brugeren 2026-11-10: "en pause-knap, hvor effekten stopper, bandet er nået til")
    pauseButton.setButtonText ("Pause");
    pauseButton.getProperties().set ("led", (juce::int64) hw::amber.getARGB());
    pauseButton.setTooltip ("Pause: the LFO stops and the band stays where it is, so you can read it in the display "
                            "and in Band Size. A band that stands still does not bend the pitch.");

    juce::Component* children[] = { &rateSlider, &syncRateSlider, &offsetSlider, &shapeBox, &hzButton, &syncButton, &pauseButton,
                                    &freqSlider, &amountSlider, &widthSlider,
                                    &modeButtons[0], &modeButtons[1], &modeButtons[2], &dryWetSlider, &outSlider, &bypassButton };
    for (auto* c : children) addAndMakeVisible (*c);

    // Hoejreklik paa enhver knap / vaelger: automationsmenuen for dens parameter (brugeren 2026-11-04: "Sørg for alle
    // parametre kan automatiseres [...] Ved at kunne højreklikke på en knob/knap inde i pluginnet")
    auto menuFor = [this] (auto& c, const char* id) { c.onRightClick = [this, id] { showParamMenu (id); }; };
    menuFor (rateSlider, "rate");    menuFor (syncRateSlider, "syncrate"); menuFor (offsetSlider, "offset");
    menuFor (shapeBox, "shape");     menuFor (hzButton, "sync");           menuFor (syncButton, "sync");
    menuFor (freqSlider, "freq");    menuFor (amountSlider, "amount");     menuFor (widthSlider, "width");
    for (auto& b : modeButtons) menuFor (b, "mode");
    menuFor (dryWetSlider, "drywet"); menuFor (outSlider, "outgain");      menuFor (bypassButton, "bypass");
    menuFor (pauseButton, "pause");

    // "?" ved 0 ms-tilstandene. Overlayet tilfoejes sidst, saa det ligger oeverst.
    modeHelp = std::make_unique<HelpButton>();
    modeHelp->setTooltip ("What is the difference between 24, 48 and 96 dB/oct?");
    modeHelp->onClick = [this] { showModeHelp(); };
    addAndMakeVisible (*modeHelp);
    helpOverlay = std::make_unique<HelpOverlay>();
    addChildComponent (*helpOverlay);

    // Looket til sidst, naar alle boern er tilfoejet (saa faar tal-felterne og vaelgerne det nye look)
    setLookAndFeel (look.get());

    auto& s = processor.apvts;
    rateAtt     = std::make_unique<SliderAtt> (s, "rate",     rateSlider);
    syncRateAtt = std::make_unique<SliderAtt> (s, "syncrate", syncRateSlider);
    offsetAtt   = std::make_unique<SliderAtt> (s, "offset",   offsetSlider);
    freqAtt     = std::make_unique<SliderAtt> (s, "freq",     freqSlider);
    amountAtt   = std::make_unique<SliderAtt> (s, "amount",   amountSlider);
    widthAtt    = std::make_unique<SliderAtt> (s, "width",    widthSlider);
    dryWetAtt   = std::make_unique<SliderAtt> (s, "drywet",   dryWetSlider);
    outAtt      = std::make_unique<SliderAtt> (s, "outgain",  outSlider);
    shapeAtt    = std::make_unique<ComboAtt>  (s, "shape",    shapeBox);
    bypassAtt   = std::make_unique<ButtonAtt> (s, "bypass",   bypassButton);
    pauseAtt    = std::make_unique<ButtonAtt> (s, "pause",    pauseButton);
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
    for (int i = 0; i < P::kNumModes; ++i) modeButtons[i].setToggleState (i == mode, juce::dontSendNotification);
}

// "?" ved de to 0 ms-tilstande (brugeren 2026-11-04): "forklar helt kort, hvad forskellen er. Ikke noget teknisk,
// men forklaring om den lydlige kvalitet: Laver den ene mere pitch end den anden? Og så ultra kort og på
// begynderniveau, hvad 24db vs 48 db rent teknisk betyder." Tallene er maalt med HostSmoke's pitch-tabel (1.1):
// 48 / 24 = 3.0-3.2 ved 0.25-1 Hz og 1.25-1.4 ved 3-12 Hz (Amount 100 %, fartgraensen 40 / 20 okt/s).
// 1.2 (96 dB/oct, fartgraense 10 okt/s): 96 / 48 = 2.7 ved 0.25 Hz og 1.1-1.5 ved 1-12 Hz.
// 1.7 (ingen fartgraense for LFO'en, HostSmoke's pitch-tabel ved 300 Hz, Sweep 100 %): 48 / 24 = 3.0-3.4 ved alle
// hastigheder; 96 / 48 = 2.7-2.8 ved 0.25-1 Hz, 2.3 ved 3 Hz, 1.3 ved 6 Hz og 0.9 ved 12 Hz (24 / 48 / 96: 789 /
// 2335 / 2174 cent ved 12 Hz). Toppe op til ca. +9 dB ved hurtig Rate, RMS hoejst ca. +2 dB.
void DopplerSatEditor::showModeHelp()
{
    using S = HelpOverlay::Section;
    helpOverlay->show ("24, 48 or 96 dB/oct", {
        S { "How they sound",
            "All three bend the pitch while the band moves. 24 dB/oct is the gentlest and smoothest. 48 dB/oct "
            "bends about 3 times as much: a stronger, clearer wobble. 96 dB/oct bends the most at slow and medium "
            "rates: up to almost 3 times as much as 48 dB/oct. At the fastest rates 48 and 96 bend about the same.\n"
            "The band always travels the whole Sweep range, also at fast rates, so fast rates bend the pitch a lot.\n"
            "The steeper the edges and the faster the rate, the more the level swells and dips along with the pitch "
            "(the short peaks are kept at most 3 dB above a calm version of the effect), and the more a kick or bass right "
            "at the band's lower edge is smeared." },
        S { "What 24, 48 and 96 dB/oct mean",
            "The band's edges are filters, and the number is how steep they are. One octave past the edge, a 24 dB/oct "
            "filter has turned the sound down by 24 dB, 48 dB/oct by 48 dB and 96 dB/oct by 96 dB: each is twice as "
            "steep as the one before.\n"
            "A filter also delays the sound a tiny bit around its edge, and a steeper filter delays it more. When the "
            "edge moves, that delay changes, and the pitch bends (the Doppler effect).\n"
            "None of the three adds latency (0 ms). The plugin as a whole delays the sound by 1.4 ms, which your DAW "
            "compensates for." } },
        hw::amber);
    helpOverlay->grabKeyboardFocus();
}

// Hoejreklik paa en kontrol. Har DAW'en sin egen menu til parameteren (VST3: fx Cubase, Bitwig, Reaper, Studio
// One), staar dens punkter (automation, MIDI learn ...) oeverst. Derunder vores egne punkter, der virker i alle DAW'er.
void DopplerSatEditor::showParamMenu (const juce::String& paramID)
{
    auto* param = processor.apvts.getParameter (paramID);
    if (param == nullptr) return;
    const juce::String name = param->getName (64);

    juce::PopupMenu m;
    bool hostItems = false;
    if (auto* ctx = getHostContext())
        if (auto hostMenu = ctx->getContextMenuForParameter (param))
        {
            m = hostMenu->getEquivalentPopupMenu();
            hostItems = m.getNumItems() > 0;
        }
    if (hostItems)
        m.addSeparator();
    m.addSectionHeader (name);

    // Saet parameteren som "sidst rørt" i DAW'en uden at aendre den (begin / samme vaerdi / end)
    m.addItem ("Select \"" + name + "\" for automation (last touched)", [param]
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue());
        param->endChangeGesture();
    });
    m.addItem ("Reset \"" + name + "\" to default", [param]
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    });

    if (! hostItems)
    {
        m.addSeparator();
        m.addSectionHeader ("How to automate it");
        m.addItem ("Ableton Live: pick " + juce::String (JucePlugin_Name) + " > " + name + " in the track's automation lane", false, false, nullptr);
        m.addItem ("FL Studio: Select ... (above), then Tools > Last tweaked > Create automation clip", false, false, nullptr);
        m.addItem ("Other DAWs: Select ... (above), then use the DAW's 'last touched parameter'", false, false, nullptr);
    }
    m.setLookAndFeel (look.get());                 // pluginnets moerke look (ikke JUCE's standard)
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void DopplerSatEditor::timerCallback()
{
    plot->repaint();
    lfoView->sample();
    lfoView->repaint();
    updateSweepLcd();
    nowLcd->setText (hzText (processor.bandLoNow.load()) + " - " + hzText (processor.bandHiNow.load()));
}

// 1.6: Sweep-displayet viser midtens vej (Frequency +/- Sweep); Band Size aendrer det ikke
void DopplerSatEditor::updateSweepLcd()
{
    double c1, c2;
    sweepEndsNow (c1, c2);
    rangeLcd->setText (hzText (c1) == hzText (c2) ? hzText (c1) : hzText (c1) + " - " + hzText (c2));
}

// 1.10 (brugeren 2026-11-13): et skrevet tal i Sweep-displayet. part 0 = bunden, 1 = toppen, -1 = displayet viser
// kun eet tal (Sweep 0): et lavere tal er saa en ny bund, et hoejere en ny top. Den anden ende bliver staaende (som
// displayet viser den nu), og Frequency og Sweep regnes ud (DopplerSatProcessor::sweepForEnds) og saettes som een
// aendring (begge kan automatiseres som foer).
bool DopplerSatEditor::applySweepText (int part, const juce::String& typed)
{
    const double hz = parseHz (typed);
    return hz > 0.0 && applySweepHz (part, hz, true);
}

// Sweep-displayets ender, som displayet viser dem (Offset regnet med)
void DopplerSatEditor::sweepEndsNow (double& c1, double& c2) const
{
    const double off = (double) processor.apvts.getRawParameterValue ("offset")->load() * 0.01;
    c1 = processor.bandCentreFor (juce::jlimit (-1.0, 1.0, -1.0 + off));
    c2 = processor.bandCentreFor (juce::jlimit (-1.0, 1.0,  1.0 + off));
}

// Faelles for skrivning (1.10) og traek (1.14): en ny vaerdi for bunden (part 0) eller toppen (1); -1 = eet tal vist.
// gesture = saet Frequency og Sweep som een aendring (ved traek styrer LcdText begyndelse og slutning).
bool DopplerSatEditor::applySweepHz (int part, double hz, bool gesture)
{
    auto& s = processor.apvts;
    const double off = (double) s.getRawParameterValue ("offset")->load() * 0.01;
    double c1, c2;
    sweepEndsNow (c1, c2);
    bool top = part == 1;
    if (part < 0)
    {
        if (hz == c1) return false;
        top = hz > c1;
    }
    double freq = 0.0, sweep = 0.0;
    if (P::sweepForEnds (top ? c1 : hz, top ? hz : c2, top, off, (double) s.getRawParameterValue ("freq")->load(),
                         freq, sweep) == 0)
        return false;

    auto* fp = s.getParameter ("freq");
    auto* ap = s.getParameter ("amount");
    if (gesture) { fp->beginChangeGesture(); ap->beginChangeGesture(); }
    fp->setValueNotifyingHost (fp->convertTo0to1 ((float) freq));
    ap->setValueNotifyingHost (ap->convertTo0to1 ((float) sweep));
    if (gesture) { ap->endChangeGesture(); fp->endChangeGesture(); }
    // Kun Sweep-displayets tal med det samme. 1.10-1.14 kaldte hele timerCallback, saa displayet blev tegnet ved hver
    // musebevaegelse under et traek (op til 60-125 gange i sekundet mod ellers 30), og Random-sporet i LFO-visningen
    // loeb hurtigere (brugeren 2026-11-13: "Hvorfor bliver frame rate højere, når jeg skruer på en af kanterne").
    updateSweepLcd();
    return true;
}

void DopplerSatEditor::testBeginSweepEdit (int part, const juce::String& shownText)
{
    rangeLcd->beginEdit (part);
    if (auto* te = dynamic_cast<juce::TextEditor*> (rangeLcd->getChildComponent (0))) te->setText (shownText, false);
}

void DopplerSatEditor::testSweepDrag (int part, float dyPixels, bool fine)
{
    rangeLcd->beginDrag (part);
    rangeLcd->dragBy (dyPixels, fine);
    rangeLcd->endDrag();
}

// 2.07 (brugeren 2026-11-13: "elementerne i GUI er lidt mere parallelle med hinanden. Det er få pixels"): de to
// raekker i modulerne har faelles top, bund og hoejde (20 px): raekke A top 43 (Sweep-display, 24 dB/oct, Bypass),
// raekke B top 93 (Shape, Hz, Sync, Band Size-display, 96 dB/oct). Til og med 2.06: Shape-raekken 94-116 og
// displayene 42-64 / 92-114 (22 hoeje), Edges-knapperne 20 hoeje.
namespace rows { constexpr float a = 43.f, b = 93.f; constexpr int h = 20; }

void DopplerSatEditor::resized()
{
    const auto L = makeLayout();
    // 1.9: Edges-soejlen i Band-modulet, til hoejre for de to displays
    const int edgeColX = L.mx[1] + L.mw[1] - 12 - layout::edgeColW;
    const float mY = (float) L.mY;
    auto y = [&] (float v) { return juce::roundToInt (mY + v); };
    plot->setBounds (L.plot);

    // LFO: kurven, Shape + Hz / Sync, Rate og Offset
    {
        const int x = L.mx[0], w = L.mw[0];
        lfoView->setBounds (x + 12, y (28), w - 24, 58);
        pauseButton.setBounds (x + w - 12 - 72, (int) mY + 2, 72, 22);     // titellinjen, midt paa linjen (y 13)
        shapeBox.setBounds (x + 60, y (rows::b), 94, rows::h);                 // 2.07: raekke B
        hzButton.setBounds (x + w - 12 - 106, y (rows::b), 48, rows::h);
        syncButton.setBounds (x + w - 12 - 54, y (rows::b), 54, rows::h);
        placeKnob (rateSlider,     (float) x + (float) w * 0.3f, mY + layout::knob, mY + layout::value);
        placeKnob (syncRateSlider, (float) x + (float) w * 0.3f, mY + layout::knob, mY + layout::value);
        placeKnob (offsetSlider,   (float) x + (float) w * 0.7f, mY + layout::knob, mY + layout::value);
    }
    // Baand: omraadet og baandet lige nu, Frequency / Sweep / Band Size (1.4; foer Amount / Width)
    {
        const int x = L.mx[1], w = L.mw[1];
        const int lcdW = edgeColX - 14 - (x + 12);
        rangeLcd->setBounds (x + 12, y (rows::a), lcdW, rows::h);              // 2.07: raekke A og B
        nowLcd->setBounds (x + 12, y (rows::b), lcdW, rows::h);
        // 1.15 (brugeren 2026-11-13): "Sweep skal være længst til venstre, Band size i midten, Frequency længst til
        // højre" (til og med 1.14: Frequency, Sweep, Band Size)
        for (int i = 0; i < 3; ++i)
            placeKnob (*std::array<juce::Slider*, 3> { &amountSlider, &widthSlider, &freqSlider }[(size_t) i],
                       (float) (x + 12) + (float) (w - 24) / 3.f * ((float) i + 0.5f), mY + layout::knob, mY + layout::value);
    }
    // Edges (1.9: en soejle i Band med "Edges" og "?" oeverst; til og med 1.8 et eget modul med "Edge steepness"-
    // tegningen). Midten af den oeverste og nederste knap flugter med midten af de to displays (y 53 og 103).
    modeHelp->setBounds (edgeColX + layout::edgeColW - 16, (int) mY + 25, 16, 16);
    for (int i = 0; i < P::kNumModes; ++i)
        modeButtons[i].setBounds (edgeColX, y (rows::a + 25.f * (float) i), layout::edgeColW, rows::h);   // a, midt, b
    // Output: Bypass, Dry / Wet og Output gain. Bypass flugter med midten af den foerste 0 ms-knap (brugeren 2026-11-05:
    // "Ryk bypass op, så den flugter med den første 0 - ms indstilling [...] gerne flugte lige med midten")
    {
        const int x = L.mx[3], w = L.mw[3], h = rows::h;                    // 2.07: 20 som raekkerne (foer 22)
        bypassButton.setBounds (x + 12, modeButtons[0].getBounds().getCentreY() - h / 2, w - 24, h);
        placeKnob (dryWetSlider, (float) x + (float) w * 0.27f, mY + layout::knob, mY + layout::value);
        placeKnob (outSlider,    (float) x + (float) w * 0.73f, mY + layout::knob, mY + layout::value);
    }
    helpOverlay->setBounds (getLocalBounds());
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
    g.drawSingleLineText ("PITCH BEND TEXTURE", 30, 28);       // 1.3: nyt navn (var "DOPPLER SATURATOR")
    // 2.07: underteksten paa samme grundlinje som titlen (28, foer 27)
    silkText (g, "Moving band  -  pitch bends as its edges sweep across the sound", (float) W - 30.f, 28.f, 9.5f,
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
    g.drawSingleLineText ("Drag left / right: Frequency  |  Double-click: reset  |  Right-click any control: automation  |  "
                          "Shaded: where the LFO moves the band's middle", W / 2, L.hintY + 12, juce::Justification::horizontallyCentred);

    static const char* titles[] = { "LFO", "Band", "Edges", "Output" };
    for (int i = 0; i < 4; ++i)
    {
        if (L.mw[i] <= 0) continue;                       // 1.9: intet Edges-modul
        const juce::Rectangle<float> m ((float) L.mx[i], mY, (float) L.mw[i], (float) layout::modH);
        g.setColour (juce::Colour (0xff1c1e21).withAlpha (0.55f));
        g.fillRoundedRectangle (m, 5.f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (m, 5.f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (m.reduced (1.f), 4.f, 1.f);
        const juce::String t = titles[i];
        silkText (g, t, m.getX() + 12.f, mY + layout::title, 11.f, hw::silk);
        // Linjen stopper foer Pause (LFO, 1.6)
        const float x1 = m.getX() + 12.f + (float) t.length() * 7.4f + 8.f,
                    x2 = m.getRight() - (i == 0 ? 92.f : 12.f);
        if (x2 - x1 > 8.f) groove (g, x1, mY + 13.f, x2, mY + 13.f);
    }

    // faste labels
    const auto silkC = hw::silk.withAlpha (0.85f);
    silkText (g, "Shape", (float) L.mx[0] + 12.f, mY + 106.5f, 9.5f, silkC);   // 2.07: midt paa raekke B (foer 109)
    // 1.5 (brugeren 2026-11-09): "Sørg for at de to displays, der viser range af frekvenser, har samme navn som de
    // tilhørende knobs (Sweep og Band Size)" (foer "Sweep range" og "Band now")
    silkText (g, "Sweep", (float) L.mx[1] + 12.f, mY + 37.f, 9.5f, silkC);
    silkText (g, "Band Size", (float) L.mx[1] + 12.f, mY + 87.f, 9.5f, silkC);
    silkText (g, "Edges", (float) (L.mx[1] + L.mw[1] - 12 - layout::edgeColW), mY + 37.f, 9.5f, silkC);   // 1.9
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
        // 1.4: Amount -> Sweep (kun omraadet, baandet bevaeger sig hen over), Width -> Band Size (selve baandet)
        // 1.15: Sweep, Band Size, Frequency (som knapperne i resized)
        static const char* m[] = { "Sweep", "Band Size", "Frequency" }, *sb[] = { "travel range", "the band itself", "middle" };
        knobText (1, 12.f + (float) (L.mw[1] - 24) / 3.f * ((float) i + 0.5f), m[i], sb[i]);
    }
    knobText (3, (float) L.mw[3] * 0.27f, "Dry / Wet", "");
    knobText (3, (float) L.mw[3] * 0.73f, "Output", "gain");
}

void DopplerSatEditor::paint (juce::Graphics& g)
{
    if (plate.getWidth() != getWidth() || plate.getHeight() != getHeight())
        renderPlate();
    g.drawImageAt (plate, 0, 0);
}
