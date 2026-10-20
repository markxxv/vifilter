#include "PluginEditor.h"
#include "ResponseMath.h"
#include "Typography.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr juce::uint32 ink = 0xffe7e8e9;
constexpr juce::uint32 muted = 0xffa6a8ac;
constexpr juce::uint32 hairline = 0xff37383a;
constexpr juce::uint32 silver = 0xfff0f1f2;

void label (juce::Graphics& g, const juce::String& text, float x, float y,
            float w, float h, float size, juce::Colour colour,
            juce::Justification align = juce::Justification::centred,
            vi::typography::Weight weight = vi::typography::Weight::regular)
{
    g.setColour (colour);
    g.setFont (vi::typography::font (size, weight));
    g.drawFittedText (text, juce::Rectangle<int> (juce::roundToInt (x), juce::roundToInt (y),
                                                  juce::roundToInt (w), juce::roundToInt (h)),
                      align, 1, 0.75f);
}

float clamp01 (float n) noexcept { return juce::jlimit (0.0f, 1.0f, n); }
} // namespace

const std::array<ViFilterEditor::Knob, 6> ViFilterEditor::knobLayout {{
    { "cutoff",      "CUTOFF",     293.0f, 446.0f, 45.0f },
    { "resonance",   "RESONANCE",  430.0f, 446.0f, 43.0f },
    { "drive",       "DRIVE",      566.0f, 446.0f, 43.0f },
    { "output_gain", "OUTPUT",     704.0f, 446.0f, 45.0f },
    { "input_trim",  "INPUT TRIM", 105.0f, 472.0f, 30.0f },
    { "mix",         "MIX",        900.0f, 472.0f, 30.0f }
}};

ViFilterEditor::ViFilterEditor (ViFilterProcessor& owner)
    : AudioProcessorEditor (owner), processor (owner)
{
    darkLook.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141516));
    darkLook.setColour (juce::PopupMenu::textColourId, juce::Colour (silver));
    darkLook.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3d3f42));
    darkLook.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setLookAndFeel (&darkLook);

    addAndMakeVisible (saveName);
    saveName.setVisible (false);
    saveName.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff18191b));
    saveName.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    saveName.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff7b7e82));
    saveName.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::white);
    saveName.setFont (vi::typography::font (17.0f, vi::typography::Weight::regular));
    saveName.onReturnKey = [this] { commitSave(); };
    saveName.onEscapeKey = [this] { saving = false; saveName.setVisible (false); repaint(); };

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (880, 493, 1120, 627);
    setSize (980, 549);
    processor.getStateInformation (snapshotA);
    snapshotB = snapshotA;
    formatPresetName();
    startTimerHz (24);
}

ViFilterEditor::~ViFilterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ViFilterEditor::resized()
{
    const auto scale = contentScale();
    saveName.setBounds (juce::roundToInt (insetX() + 342.0f * scale),
                        juce::roundToInt (insetY() + 275.0f * scale),
                        juce::roundToInt (318.0f * scale),
                        juce::roundToInt (34.0f * scale));
}

void ViFilterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff090a0b));
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::scale (contentScale())
                      .translated (insetX(), insetY()));
    paintPanel (g);
    paintHeader (g);
    paintResponse (g);
    paintControls (g);
    if (saving)
        paintSaveOverlay (g);
}

void ViFilterEditor::paintPanel (juce::Graphics& g)
{
    const juce::Rectangle<float> body (9, 8, 982, 544);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1e20), 25, 12,
                                             juce::Colour (0xff090a0c), 981, 550, false));
    g.fillRoundedRectangle (body, 11.0f);
    g.setColour (juce::Colour (0xff454649));
    g.drawRoundedRectangle (body.reduced (0.5f), 11.0f, 0.9f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff252629), 12, 12,
                                             juce::Colour (0xff111214), 988, 71, false));
    g.fillRoundedRectangle (juce::Rectangle<float> (11, 10, 978, 62), 9.0f);
    g.setColour (juce::Colour (0xff3b3c3e));
    g.drawHorizontalLine (72, 11.0f, 989.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0e10), 160, 85,
                                             juce::Colour (0xff070809), 800, 300, false));
    g.fillRoundedRectangle (juce::Rectangle<float> (19, 82, 962, 243), 9.0f);
    g.setColour (juce::Colour (0xff393a3d));
    g.drawRoundedRectangle (juce::Rectangle<float> (19.5f, 82.5f, 961, 242), 9.0f, 0.9f);

    g.setColour (juce::Colour (0xff36373a));
    g.drawHorizontalLine (335, 19.0f, 981.0f);
    g.setColour (juce::Colour (0xff262729));
    g.drawVerticalLine (202, 413.0f, 535.0f);
    g.drawVerticalLine (800, 413.0f, 535.0f);
}

void ViFilterEditor::paintButton (juce::Graphics& g, juce::Rectangle<float> area,
                                   const juce::String& text, bool selected, float fontSize)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (selected ? 0xff414346 : 0xff1b1c1f),
                                             area.getX(), area.getY(),
                                             juce::Colour (selected ? 0xff1c1e20 : 0xff0c0d0f),
                                             area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (juce::Colour (selected ? 0xffd9dbdd : 0xff434446));
    g.drawRoundedRectangle (area.reduced (0.75f), 5.0f, selected ? 1.5f : 0.8f);
    if (selected)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (area.expanded (2.3f), 7.0f, 2.8f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (area.getX() + 8.0f, area.getY() + 2.0f,
                    area.getRight() - 8.0f, area.getY() + 2.0f);
    }
    label (g, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
           fontSize, juce::Colour (selected ? ink : muted));
}

void ViFilterEditor::paintHeader (juce::Graphics& g)
{
    juce::Path mark;
    mark.startNewSubPath (28, 43);
    mark.cubicTo (37, 43, 36, 26, 43, 26);
    mark.cubicTo (50, 26, 50, 57, 57, 57);
    mark.cubicTo (63, 57, 65, 42, 74, 42);
    g.setColour (juce::Colour (ink));
    g.strokePath (mark, juce::PathStrokeType (1.8f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (vi::typography::font (26.0f, vi::typography::Weight::light));
    g.drawText ("Vi Filter", juce::Rectangle<int> (83, 21, 188, 38),
                juce::Justification::centredLeft, false);

    paintButton (g, { 289, 25, 32, 33 }, "<", false, 14);
    paintButton (g, { 325, 25, 201, 33 }, "", false);
    label (g, displayedPreset, 339, 27, 155, 28, 14, juce::Colour (ink),
           juce::Justification::centredLeft, vi::typography::Weight::medium);
    g.setColour (juce::Colour (muted));
    g.drawLine (509, 38, 515, 43, 1.4f);
    g.drawLine (515, 43, 521, 38, 1.4f);
    paintButton (g, { 530, 25, 32, 33 }, ">", false, 14);

    paintButton (g, { 579, 25, 33, 33 }, "A", comparingA, 13);
    paintButton (g, { 615, 25, 33, 33 }, "B", ! comparingA, 13);
    paintButton (g, { 664, 25, 34, 33 }, "", false);
    paintButton (g, { 701, 25, 34, 33 }, "", false);
    juce::Path undo, redo;
    undo.startNewSubPath (688, 47);
    undo.cubicTo (688, 34, 671, 34, 672, 44);
    undo.lineTo (672, 48);
    redo.startNewSubPath (711, 47);
    redo.cubicTo (711, 34, 728, 34, 727, 44);
    redo.lineTo (727, 48);
    g.setColour (juce::Colour (ink));
    g.strokePath (undo, juce::PathStrokeType (1.5f));
    g.strokePath (redo, juce::PathStrokeType (1.5f));
    g.drawLine (668, 44, 672, 48, 1.5f); g.drawLine (676, 44, 672, 48, 1.5f);
    g.drawLine (723, 44, 727, 48, 1.5f); g.drawLine (731, 44, 727, 48, 1.5f);

    label (g, "OS", 752, 31, 24, 21, 11.0f, juce::Colour (muted));
    const auto os = juce::roundToInt (currentValue ("oversampling"));
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 785.0f + 65.0f * i, 25, 61, 33 },
                     juce::String (1 << i) + "x", os == i, 12.5f);

    if (feedbackTicks > 0 && feedback.isNotEmpty())
        label (g, feedback, 276, 55, 260, 15, 10, juce::Colour (muted));
}

float ViFilterEditor::currentValue (const char* id) const noexcept
{
    if (auto* value = processor.parameterState().getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);
    return 0.0f;
}

juce::RangedAudioParameter* ViFilterEditor::getParameter (const char* id) const
{
    return processor.parameterState().getParameter (id);
}

void ViFilterEditor::setParameter (const char* id, float rawValue)
{
    if (auto* parameter = getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (rawValue));
        parameter->endChangeGesture();
    }
}

float ViFilterEditor::cutoffToX (float cutoff) const noexcept
{
    const auto normalized = std::log (juce::jlimit (20.0f, 20000.0f, cutoff) / 20.0f)
                          / std::log (1000.0f);
    return 75.0f + 775.0f * normalized;
}

float ViFilterEditor::xToCutoff (float x) const noexcept
{
    return 20.0f * std::pow (1000.0f, clamp01 ((x - 75.0f) / 775.0f));
}

float ViFilterEditor::responseDb (double frequency) const noexcept
{
    return static_cast<float> (20.0 * std::log10 (std::max (1.0e-8,
        vi::response::magnitude (std::max (processor.getSampleRate(), 44100.0),
            frequency, currentValue ("cutoff"), currentValue ("resonance"),
            juce::roundToInt (currentValue ("filter_mode")), currentValue ("mix") * 0.01,
            currentValue ("input_trim"), currentValue ("output_gain")))));
}

float ViFilterEditor::frequencyY (double frequency) const noexcept
{
    const auto db = juce::jlimit (-24.0f, 24.0f, responseDb (frequency));
    if (linearScale)
    {
        const auto amplitude = std::pow (10.0f, db / 20.0f);
        return 281.0f - 167.0f * clamp01 (amplitude / 4.0f);
    }
    return 114.0f + (24.0f - db) * 167.0f / 48.0f;
}

void ViFilterEditor::paintResponse (juce::Graphics& g)
{
    constexpr float left = 75.0f, top = 114.0f;
    constexpr float right = 850.0f, bottom = 281.0f;
    const std::array<double, 10> frequencies {{ 20, 50, 100, 200, 500,
                                                  1000, 2000, 5000, 10000, 20000 }};
    const std::array<const char*, 10> names {{ "20", "50", "100", "200", "500",
                                               "1k", "2k", "5k", "10k", "20k" }};

    // Restrained grid; keep the response readable on a 14-inch MacBook.
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        const auto x = cutoffToX (static_cast<float> (frequencies[i]));
        g.setColour (juce::Colour (0xff2d2e30));
        g.drawVerticalLine (juce::roundToInt (x), top, bottom);
        label (g, names[i], x - 19, 286, 38, 20, 11.5f, juce::Colour (muted),
               juce::Justification::centred, vi::typography::Weight::medium);
    }
    label (g, "Hz", 849, 288, 22, 18, 10.5f, juce::Colour (muted));

    for (int db : { -24, -12, 0, 12, 24 })
    {
        const auto y = top + (24 - db) * (bottom - top) / 48.0f;
        g.setColour (juce::Colour (db == 0 ? 0xff414346 : 0xff292a2c));
        g.drawLine (left, y, right, y, 0.8f);
        label (g, (db > 0 ? "+" : "") + juce::String (db), 26, y - 9, 38, 19, 11.0f,
               juce::Colour (muted), juce::Justification::centredRight,
               vi::typography::Weight::medium);
    }
    g.setColour (juce::Colour (0xff75777a));
    g.drawLine (left, top, left, bottom, 0.75f);
    g.drawLine (left, bottom, right, bottom, 0.75f);

    {
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (juce::Rectangle<int> (75, 114, 775, 168));
    if (showSpectrum)
    {
        juce::Path trace;
        const auto rate = std::max (processor.getSampleRate(), 44100.0);
        for (int i = 0; i < 260; ++i)
        {
            const auto x = left + (right - left) * i / 259.0f;
            const auto f = xToCutoff (x);
            const auto bin = juce::jlimit (1, fftSize / 2 - 1,
                                      juce::roundToInt (f / rate * fftSize));
            const auto amplitude = std::max (1.0e-6f, spectrum[static_cast<size_t> (bin)]);
            const auto db = 20.0f * std::log10 (amplitude) - 8.0f;
            const auto y = juce::jlimit (top, bottom,
                                        top + (12.0f - db) * (bottom - top) / 60.0f);
            if (i == 0) trace.startNewSubPath (x, y);
            else trace.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff898b8e).withAlpha (0.58f));
        g.strokePath (trace, juce::PathStrokeType (1.2f));
        auto area = trace;
        area.lineTo (right, bottom);
        area.lineTo (left, bottom);
        area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (
            juce::Colour (0xffd8dadc).withAlpha (0.10f), left, top,
            juce::Colours::transparentBlack, left, bottom, false));
        g.fillPath (area);
    }

    juce::Path curve;
    for (int i = 0; i < 288; ++i)
    {
        const auto x = left + (right - left) * i / 287.0f;
        const auto y = frequencyY (xToCutoff (x));
        if (i == 0) curve.startNewSubPath (x, y);
        else curve.lineTo (x, y);
    }
    auto fill = curve;
    fill.lineTo (right, bottom);
    fill.lineTo (left, bottom);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0xffe6e7e8).withAlpha (0.12f), left, top,
        juce::Colours::transparentBlack, left, bottom, false));
    g.fillPath (fill);
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.strokePath (curve, juce::PathStrokeType (4.0f));
    g.setColour (juce::Colour (0xffe6e8e9));
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    const auto cx = cutoffToX (currentValue ("cutoff"));
    const auto cy = frequencyY (currentValue ("cutoff"));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillEllipse (cx - 11, cy - 11, 22, 22);
    g.setColour (juce::Colour (0xffededee));
    g.drawEllipse (cx - 6.5f, cy - 6.5f, 13, 13, 1.6f);
    g.setColour (juce::Colour (0xff0e0f10));
    g.fillEllipse (cx - 4, cy - 4, 8, 8);

    } // scoped response/spectrum clipping

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff242528), 870, 110,
                                             juce::Colour (0xff101113), 972, 230, false));
    g.fillRoundedRectangle ({ 868, 111, 102, 116 }, 6.0f);
    g.setColour (juce::Colour (0xff424347));
    g.drawRoundedRectangle ({ 868, 111, 102, 116 }, 6.0f, 0.75f);
    label (g, "ANALYZER", 875, 119, 76, 20, 10.0f, juce::Colour (muted),
           juce::Justification::centredLeft);
    paintButton (g, { 938, 141, 25, 23 }, "", showSpectrum, 11.0f);
    g.setColour (juce::Colour (showSpectrum ? 0xffeeeeef : 0xff808286));
    g.fillEllipse (showSpectrum ? 951.0f : 941.0f, 147.0f, 11, 11);
    label (g, "SCALE", 876, 169, 78, 17, 10.0f, juce::Colour (muted),
           juce::Justification::centredLeft);
    paintButton (g, { 876, 190, 42, 27 }, "dB", ! linearScale, 11.0f);
    paintButton (g, { 921, 190, 42, 27 }, "Lin", linearScale, 11.0f);
}

juce::String ViFilterEditor::formatKnobValue (const Knob& k) const
{
    const auto value = currentValue (k.id);
    if (juce::String (k.id) == "cutoff")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (value)) + " Hz";
    if (juce::String (k.id) == "resonance")
        return juce::String (value, 2);
    if (juce::String (k.id) == "mix")
        return juce::String (juce::roundToInt (value)) + " %";
    return juce::String (value, 1) + " dB";
}

void ViFilterEditor::paintKnob (juce::Graphics& g, const Knob& k)
{
    const auto large = k.radius > 35;
    label (g, k.title, k.x - 78, large ? 347.0f : 418.0f,
           156, 23, large ? 12.5f : 11.5f, juce::Colour (ink),
           juce::Justification::centred, vi::typography::Weight::regular);
    const auto r = k.radius;
    const auto cx = k.x, cy = k.y;

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - r - 5, cy - r + 8, 2 * (r + 5), 2 * (r + 5));
    g.setColour (juce::Colour (0xff3b3d40));
    g.drawEllipse (cx - r - 3, cy - r - 3, 2 * (r + 3), 2 * (r + 3), 1.0f);
    g.setColour (juce::Colour (0xff08090a));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);

    const auto normalized = [&]
    {
        if (auto* param = getParameter (k.id))
            return param->getValue();
        return 0.0f;
    }();
    constexpr auto angleStart = -2.37f;
    constexpr auto angleEnd = 2.37f;
    const auto angle = angleStart + clamp01 (normalized) * (angleEnd - angleStart);

    for (int i = 0; i <= 38; ++i)
    {
        const auto a = angleStart + (angleEnd - angleStart) * i / 38.0f;
        const auto near = r + (large ? 12.0f : 7.0f);
        const auto far = near + ((i % 5 == 0) ? 7.0f : 4.0f);
        const auto bright = a <= angle + 0.0001f;
        g.setColour (juce::Colour (bright ? 0xffa6a8ab : 0xff55575a));
        g.drawLine (cx + std::sin (a) * near, cy - std::cos (a) * near,
                    cx + std::sin (a) * far, cy - std::cos (a) * far,
                    i % 5 == 0 ? 1.4f : 0.9f);
    }

    juce::Path backgroundArc, valueArc;
    backgroundArc.addCentredArc (cx, cy, r + 3, r + 3, 0, angleStart, angleEnd, true);
    valueArc.addCentredArc (cx, cy, r + 3, r + 3, 0, angleStart,
                           std::max (angleStart + 0.0001f, angle), true);
    g.setColour (juce::Colour (0xff242528));
    g.strokePath (backgroundArc, juce::PathStrokeType (5.3f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.11f));
    g.strokePath (valueArc, juce::PathStrokeType (9.0f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffeeeeef));
    g.strokePath (valueArc, juce::PathStrokeType (3.5f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto capR = r - 8.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff48494c),
                                             cx - capR, cy - capR,
                                             juce::Colour (0xff101113),
                                             cx + capR, cy + capR, false));
    g.fillEllipse (cx - capR, cy - capR, 2 * capR, 2 * capR);
    g.setColour (juce::Colour (0xff050606));
    g.drawEllipse (cx - capR + 1, cy - capR + 1, 2 * (capR - 1), 2 * (capR - 1), 2.0f);
    g.setColour (juce::Colour (0xff888a8d).withAlpha (0.42f));
    g.drawEllipse (cx - capR + 4, cy - capR + 4, 2 * (capR - 4), 2 * (capR - 4), 0.8f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff37383b).withAlpha (0.45f),
                                             cx - capR * 0.7f, cy - capR * 0.8f,
                                             juce::Colour (0xff121315).withAlpha (0.01f),
                                             cx + capR * 0.8f, cy + capR * 0.7f, true));
    g.fillEllipse (cx - capR + 5, cy - capR + 5, 2 * (capR - 5), 2 * (capR - 5));

    // A fine radial brushed-metal grain, rasterized by JUCE at any DPI.
    for (int tick = 0; tick < 90; ++tick)
    {
        const auto a = 2.0f * pi * tick / 90.0f;
        const auto inner = capR * 0.15f;
        const auto outer = capR * 0.88f;
        g.setColour (juce::Colours::white.withAlpha (
            0.008f + 0.011f * std::abs (std::sin (a * 5.0f))));
        g.drawLine (cx + std::sin (a) * inner, cy - std::cos (a) * inner,
                    cx + std::sin (a) * outer, cy - std::cos (a) * outer, 0.65f);
    }

    g.setColour (juce::Colour (0xfff1f2f3));
    g.drawLine (cx + std::sin (angle) * capR * 0.30f,
                cy - std::cos (angle) * capR * 0.30f,
                cx + std::sin (angle) * capR * 0.83f,
                cy - std::cos (angle) * capR * 0.83f, large ? 2.8f : 2.0f);
    label (g, formatKnobValue (k), cx - 72, large ? 516.0f : 518.0f,
           144, 22, large ? 16.0f : 13.5f, juce::Colour (ink),
           juce::Justification::centred, vi::typography::Weight::medium);
}

void ViFilterEditor::paintControls (juce::Graphics& g)
{
    label (g, "FILTER MODE", 23, 349, 164, 22, 11.5f, juce::Colour (ink));
    const auto mode = juce::roundToInt (currentValue ("filter_mode"));
    const std::array<const char*, 3> modes {{ "LP", "HP", "BP" }};
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 26.0f + 54.0f * i, 378.0f, 51, 34 },
                     modes[static_cast<size_t> (i)], mode == i, 12.0f);

    label (g, "ANALOG CHARACTER", 814, 349, 163, 22, 11.5f, juce::Colour (ink));
    const auto character = juce::roundToInt (currentValue ("analog_character"));
    const std::array<const char*, 3> characters {{ "Soft", "Tube", "Tape" }};
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 813.0f + 55.0f * i, 378.0f, 52, 34 },
                     characters[static_cast<size_t> (i)], character == i, 11.5f);

    for (const auto& knob : knobLayout)
        paintKnob (g, knob);
}

void ViFilterEditor::paintSaveOverlay (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle ({ 291, 199, 419, 179 }, 11.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff292a2c), 302, 211,
                                             juce::Colour (0xff111214), 701, 377, false));
    g.fillRoundedRectangle ({ 300, 207, 400, 164 }, 9.0f);
    g.setColour (juce::Colour (0xff636568));
    g.drawRoundedRectangle ({ 300, 207, 400, 164 }, 9.0f, 1.0f);
    label (g, "SAVE PRESET", 342, 224, 316, 24, 15.0f, juce::Colour (ink),
           juce::Justification::centredLeft);
    paintButton (g, { 482, 327, 75, 30 }, "SAVE", true, 12);
    paintButton (g, { 566, 327, 91, 30 }, "CANCEL", false, 12);
}

juce::Point<float> ViFilterEditor::point (const juce::MouseEvent& e) const noexcept
{
    return { (e.position.x - insetX()) / contentScale(),
             (e.position.y - insetY()) / contentScale() };
}

int ViFilterEditor::knobAt (juce::Point<float> p) const noexcept
{
    for (size_t i = 0; i < knobLayout.size(); ++i)
    {
        const auto& k = knobLayout[i];
        if (std::hypot (p.x - k.x, p.y - k.y) <= k.radius + 12.0f)
            return static_cast<int> (i);
    }
    return -1;
}

void ViFilterEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = point (e);
    const auto hit = [p] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x, y, w, h).contains (p);
    };
    if (saving)
    {
        if (hit (482, 327, 75, 30)) commitSave();
        else if (hit (566, 327, 91, 30))
        {
            saving = false;
            saveName.setVisible (false);
            repaint();
        }
        return;
    }
    if (hit (325, 25, 201, 33))
    {
        openPresetMenu();
        return;
    }
    if (hit (289, 25, 32, 33) || hit (530, 25, 32, 33))
    {
        const auto current = processor.getCurrentProgram();
        const auto offset = hit (289, 25, 32, 33) ? -1 : 1;
        const auto count = processor.getNumPrograms() - 1;
        const auto next = ((current <= 0 ? 1 : current) - 1 + offset + count) % count + 1;
        processor.setCurrentProgram (next);
        formatPresetName();
        repaint();
        return;
    }
    if (hit (579, 25, 33, 33)) { switchAB (true); return; }
    if (hit (615, 25, 33, 33)) { switchAB (false); return; }
    if (hit (664, 25, 34, 33)) { processor.undoManager().undo(); repaint(); return; }
    if (hit (701, 25, 34, 33)) { processor.undoManager().redo(); repaint(); return; }
    for (int i = 0; i < 3; ++i)
    {
        if (hit (785.0f + 65 * i, 25, 61, 33))
        {
            setParameter ("oversampling", static_cast<float> (i)); repaint(); return;
        }
        if (hit (26.0f + 54 * i, 378, 51, 34))
        {
            setParameter ("filter_mode", static_cast<float> (i)); repaint(); return;
        }
        if (hit (813.0f + 55 * i, 378, 52, 34))
        {
            setParameter ("analog_character", static_cast<float> (i)); repaint(); return;
        }
    }
    if (hit (938, 141, 25, 23)) { showSpectrum = ! showSpectrum; repaint(); return; }
    if (hit (876, 190, 42, 27)) { linearScale = false; repaint(); return; }
    if (hit (921, 190, 42, 27)) { linearScale = true; repaint(); return; }

    activeKnob = knobAt (p);
    if (activeKnob >= 0)
    {
        activeParameter = getParameter (knobLayout[static_cast<size_t> (activeKnob)].id);
        if (activeParameter != nullptr)
        {
            activeParameter->beginChangeGesture();
            dragStartValue = activeParameter->getValue();
            dragStartY = e.position.y;
        }
        return;
    }
    if (hit (75, 114, 775, 167))
    {
        draggingResponse = true;
        activeParameter = getParameter ("cutoff");
        if (activeParameter != nullptr)
            activeParameter->beginChangeGesture();
        mouseDrag (e);
    }
}

void ViFilterEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (activeParameter == nullptr) return;

    if (activeKnob >= 0)
    {
        const auto distance = (dragStartY - e.position.y) / scaleY();
        const auto sensitivity = e.mods.isShiftDown() ? 0.0008f : 0.0038f;
        activeParameter->setValueNotifyingHost (clamp01 (dragStartValue + distance * sensitivity));
    }
    else if (draggingResponse)
    {
        activeParameter->setValueNotifyingHost (activeParameter->convertTo0to1 (
            xToCutoff (point (e).x)));
    }
    repaint();
}

void ViFilterEditor::mouseUp (const juce::MouseEvent&)
{
    if (activeParameter != nullptr)
        activeParameter->endChangeGesture();
    activeParameter = nullptr;
    activeKnob = -1;
    draggingResponse = false;
}

void ViFilterEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto index = knobAt (point (e));
    if (index < 0) return;
    const auto* id = knobLayout[static_cast<size_t> (index)].id;
    for (const auto& spec : vi::presets::specs)
        if (juce::String (id) == spec.id)
        {
            setParameter (id, spec.defaultValue);
            repaint();
            return;
        }
}

void ViFilterEditor::mouseWheelMove (const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& details)
{
    const auto index = knobAt (point (e));
    if (index < 0) return;
    if (auto* param = getParameter (knobLayout[static_cast<size_t> (index)].id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (clamp01 (param->getValue() + details.deltaY * 0.12f));
        param->endChangeGesture();
    }
    repaint();
}

bool ViFilterEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && saving)
    {
        saving = false;
        saveName.setVisible (false);
        repaint();
        return true;
    }
    return false;
}

void ViFilterEditor::switchAB (bool selectA)
{
    if (selectA == comparingA)
        return;
    if (comparingA)
        processor.getStateInformation (snapshotA);
    else
        processor.getStateInformation (snapshotB);

    const auto& selected = selectA ? snapshotA : snapshotB;
    if (selected.getSize() > 0)
        processor.setStateInformation (selected.getData(), static_cast<int> (selected.getSize()));
    comparingA = selectA;
    formatPresetName();
    repaint();
}

void ViFilterEditor::formatPresetName()
{
    const auto program = processor.getCurrentProgram();
    displayedPreset = program > 0 ? processor.getProgramName (program) : "Custom";
}

void ViFilterEditor::showSaveDialog()
{
    saving = true;
    saveName.setText (displayedPreset == "Custom" ? "My Filter" : displayedPreset, false);
    saveName.setVisible (true);
    saveName.grabKeyboardFocus();
    saveName.selectAll();
    repaint();
}

void ViFilterEditor::commitSave()
{
    juce::File saved;
    const auto result = processor.saveUserPreset (saveName.getText(), &saved);
    feedback = result.wasOk() ? "PRESET SAVED" : result.getErrorMessage();
    feedbackTicks = 90;
    if (result.wasOk())
    {
        displayedPreset = saveName.getText();
        saving = false;
        saveName.setVisible (false);
    }
    repaint();
}

void ViFilterEditor::openPresetMenu()
{
    userFiles = processor.listUserPresets();
    juce::PopupMenu menu;
    menu.setLookAndFeel (&darkLook);
    menu.addSectionHeader ("FACTORY PRESETS");
    for (int index = 1; index < processor.getNumPrograms(); ++index)
        menu.addItem (100 + index, processor.getProgramName (index), true,
                      index == processor.getCurrentProgram());
    if (! userFiles.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("USER PRESETS");
        for (int i = 0; i < userFiles.size(); ++i)
            menu.addItem (1000 + i, userFiles[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem (9001, "SAVE CURRENT AS...");
    menu.showMenuAsync (juce::PopupMenu::Options()
                        .withTargetComponent (this)
                        .withTargetScreenArea (juce::Rectangle<int> (
                            getScreenPosition().x + juce::roundToInt (insetX() + 325 * contentScale()),
                            getScreenPosition().y + juce::roundToInt (insetY() + 58 * contentScale()),
                            juce::roundToInt (201 * contentScale()), 1)),
                        [safe = juce::Component::SafePointer<ViFilterEditor> (this)] (int result)
                        {
                            if (safe != nullptr)
                                safe->applyPresetSelection (result);
                        });
}

void ViFilterEditor::applyPresetSelection (int result)
{
    if (result == 9001) { showSaveDialog(); return; }
    if (result > 100 && result <= 100 + static_cast<int> (vi::presets::factory.size()))
    {
        processor.setCurrentProgram (result - 100);
        formatPresetName();
    }
    else if (result >= 1000 && result < 1000 + userFiles.size())
    {
        const auto file = userFiles[result - 1000];
        const auto loaded = processor.loadUserPreset (file);
        if (loaded.wasOk())
            displayedPreset = file.getFileNameWithoutExtension();
        else
        {
            feedback = loaded.getErrorMessage();
            feedbackTicks = 100;
        }
    }
    repaint();
}

void ViFilterEditor::updateAnalyzer()
{
    std::array<float, 1024> pending {};
    for (int passes = 0; passes < 16; ++passes)
    {
        const auto count = processor.popAnalysisSamples (pending.data(),
                                                        static_cast<int> (pending.size()));
        if (count == 0) break;
        for (int i = 0; i < count; ++i)
        {
            fftHistory[static_cast<size_t> (historyWritePosition)] = pending[static_cast<size_t> (i)];
            historyWritePosition = (historyWritePosition + 1) % fftSize;
        }
    }
    if (! showSpectrum)
        return;
    float energy = 0.0f;
    for (int i = 0; i < fftSize; ++i)
    {
        const auto x = fftHistory[static_cast<size_t> ((historyWritePosition + i) % fftSize)];
        const auto window = 0.5f - 0.5f * std::cos (2.0f * pi * i / (fftSize - 1));
        fftWorkspace[static_cast<size_t> (i)] = x * window;
        energy += x * x;
    }
    std::fill (fftWorkspace.begin() + fftSize, fftWorkspace.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftWorkspace.data());
    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        const auto next = fftWorkspace[i] / static_cast<float> (fftSize / 2);
        spectrum[i] = spectrum[i] * 0.78f + std::max (0.0f, next) * 0.22f;
    }
    measuredLevel = std::sqrt (energy / fftSize);
}

void ViFilterEditor::refreshAnalyzerNow()
{
    updateAnalyzer();
    repaint();
}

void ViFilterEditor::timerCallback()
{
    updateAnalyzer();
    if (feedbackTicks > 0)
        --feedbackTicks;
    if (! saving && processor.getCurrentProgram() > 0)
    {
        const auto name = processor.getProgramName (processor.getCurrentProgram());
        if (displayedPreset != name)
            displayedPreset = name;
    }
    else if (! saving && processor.getCurrentProgram() == 0)
    {
        for (const auto& preset : vi::presets::factory)
            if (displayedPreset == preset.name)
            {
                displayedPreset = "Custom";
                break;
            }
    }
    repaint();
}
