#include "PluginEditor.h"
#include "ResponseMath.h"

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
            juce::Justification align = juce::Justification::centred)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (size)));
    g.drawFittedText (text, juce::Rectangle<int> (juce::roundToInt (x), juce::roundToInt (y),
                                                  juce::roundToInt (w), juce::roundToInt (h)),
                      align, 1, 0.75f);
}

float clamp01 (float n) noexcept { return juce::jlimit (0.0f, 1.0f, n); }
} // namespace

const std::array<ViFilterEditor::Knob, 6> ViFilterEditor::knobLayout {{
    { "cutoff",       "CUTOFF",      396.0f, 638.0f, 66.0f },
    { "resonance",    "RESONANCE",   612.0f, 638.0f, 61.0f },
    { "drive",        "DRIVE",       830.0f, 638.0f, 61.0f },
    { "output_gain",  "OUTPUT GAIN", 1048.0f, 638.0f, 66.0f },
    { "input_trim",   "INPUT TRIM",  158.0f, 701.0f, 41.0f },
    { "mix",          "MIX",        1281.0f, 701.0f, 41.0f }
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
    saveName.setFont (juce::Font (juce::FontOptions (16.0f)));
    saveName.onReturnKey = [this] { commitSave(); };
    saveName.onEscapeKey = [this] { saving = false; saveName.setVisible (false); repaint(); };

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1100, 650, 2200, 1300);
    setSize (1440, 820);
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
    saveName.setBounds (juce::roundToInt (525.0f * scaleX()),
                        juce::roundToInt (371.0f * scaleY()),
                        juce::roundToInt (390.0f * scaleX()),
                        juce::roundToInt (37.0f * scaleY()));
}

void ViFilterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff090a0b));
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::scale (scaleX(), scaleY()));
    paintPanel (g);
    paintHeader (g);
    paintResponse (g);
    paintControls (g);
    if (saving)
        paintSaveOverlay (g);
}

void ViFilterEditor::paintPanel (juce::Graphics& g)
{
    juce::Rectangle<float> panel (19, 18, 1402, 784);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1c1e), 50, 20,
                                             juce::Colour (0xff090a0b), 1400, 800, false));
    g.fillRoundedRectangle (panel, 13.0f);

    g.setColour (juce::Colour (0xff414244));
    g.drawRoundedRectangle (panel.reduced (0.6f), 13.0f, 1.0f);
    g.setColour (juce::Colour (0xff080808));
    g.drawRoundedRectangle (panel.reduced (2.0f), 11.0f, 1.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff202124), 40, 30,
                                             juce::Colour (0xff111214), 1400, 95, false));
    g.fillRoundedRectangle (juce::Rectangle<float> (21, 20, 1398, 72), 12.0f);
    g.setColour (juce::Colour (0xff3c3d3f));
    g.drawHorizontalLine (92, 21.0f, 1418.0f);
    g.setColour (juce::Colour (0xff070808));
    g.drawHorizontalLine (94, 22.0f, 1417.0f);

    g.setColour (juce::Colour (0xff090a0b));
    g.fillRoundedRectangle (juce::Rectangle<float> (39, 109, 1362, 350), 13.0f);
    g.setColour (juce::Colour (0xff3d3e40));
    g.drawRoundedRectangle (juce::Rectangle<float> (40, 109, 1360, 347), 13.0f, 1.0f);
    g.setColour (juce::Colour (0xff1b1c1f));
    g.drawRoundedRectangle (juce::Rectangle<float> (43, 112, 1354, 341), 11.0f, 1.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18191b), 160, 500,
                                             juce::Colour (0xff0d0e10), 1300, 795, false));
    g.fillRect (juce::Rectangle<float> (21, 475, 1398, 314));
    g.setColour (juce::Colour (0xff292a2c));
    g.drawHorizontalLine (478, 40.0f, 1400.0f);
    g.setColour (juce::Colour (0xff3b3c3e));
    g.drawVerticalLine (277, 640.0f, 762.0f);
    g.drawVerticalLine (1169, 640.0f, 762.0f);
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
    // Abstract waveform mark, using only anti-aliased JUCE paths.
    juce::Path mark;
    mark.startNewSubPath (52, 59);
    mark.cubicTo (61, 59, 61, 40, 69, 40);
    mark.cubicTo (76, 40, 76, 73, 82, 73);
    mark.cubicTo (88, 73, 91, 54, 99, 58);
    g.setColour (juce::Colour (ink));
    g.strokePath (mark, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    label (g, "Vi Filter", 108, 32, 250, 48, 29, juce::Colour (ink),
           juce::Justification::centredLeft);

    paintButton (g, { 525, 34, 45, 45 }, "<", false, 20);
    paintButton (g, { 572, 34, 291, 45 }, "", false);
    label (g, displayedPreset, 590, 35, 248, 43, 16, juce::Colour (ink),
           juce::Justification::centredLeft);
    label (g, "⌄", 833, 36, 24, 41, 25, juce::Colour (muted));
    paintButton (g, { 865, 34, 45, 45 }, ">", false, 20);
    paintButton (g, { 938, 34, 45, 45 }, "A", comparingA, 14);
    paintButton (g, { 985, 34, 45, 45 }, "B", ! comparingA, 14);
    paintButton (g, { 1048, 34, 44, 45 }, "↶", false, 20);
    paintButton (g, { 1095, 34, 44, 45 }, "↷", false, 20);

    label (g, "OS", 1157, 41, 36, 30, 12, juce::Colour (muted));
    const auto os = juce::roundToInt (currentValue ("oversampling"));
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 1200.0f + 62.0f * i, 34.0f, 60, 45 },
                     juce::String (1 << i) + "x", os == i, 13);

    if (feedbackTicks > 0 && feedback.isNotEmpty())
        label (g, feedback, 357, 57, 160, 18, 11, juce::Colour (muted));
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
    return 111.0f + 1111.0f * normalized;
}

float ViFilterEditor::xToCutoff (float x) const noexcept
{
    const auto position = clamp01 ((x - 111.0f) / 1111.0f);
    return 20.0f * std::pow (1000.0f, position);
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
        return 406.0f - 263.0f * clamp01 (amplitude / 4.0f);
    }
    return 143.0f + (24.0f - db) * 263.0f / 48.0f;
}

void ViFilterEditor::paintResponse (juce::Graphics& g)
{
    constexpr float left = 111.0f, top = 143.0f;
    constexpr float right = 1222.0f, bottom = 406.0f;
    // Low-contrast logarithmic divisions; no fake spectrum data is painted.
    const std::array<double, 10> frequencies {{ 20, 50, 100, 200, 500, 1000,
                                                  2000, 5000, 10000, 20000 }};
    const std::array<const char*, 10> names {{ "20", "50", "100", "200", "500",
                                               "1k", "2k", "5k", "10k", "20k" }};
    g.setColour (juce::Colour (0xff242527));
    for (int decade = 1; decade <= 4; ++decade)
    {
        const auto base = std::pow (10.0, decade);
        for (int m = 2; m < 10; ++m)
        {
            const auto f = base * m;
            if (f > 20000 || f < 20)
                continue;
            const auto x = cutoffToX (static_cast<float> (f));
            g.drawVerticalLine (juce::roundToInt (x), top, bottom);
        }
    }
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        const auto x = cutoffToX (static_cast<float> (frequencies[i]));
        g.setColour (juce::Colour (0xff353639));
        g.drawVerticalLine (juce::roundToInt (x), top, bottom);
        g.setColour (juce::Colour (0xff7d7f83));
        g.drawLine (x, bottom, x, bottom + 7.0f, 1.0f);
        label (g, names[i], x - 25.0f, 414, 50, 19, 12.5f, juce::Colour (muted));
    }
    label (g, "Hz", 1232, 414, 30, 18, 12, juce::Colour (muted));
    for (int n = -24; n <= 24; n += 12)
    {
        const auto y = top + (24 - n) * (bottom - top) / 48.0f;
        g.setColour (juce::Colour (n == 0 ? 0xff414346 : 0xff303133));
        g.drawLine (left, y, right, y, 0.8f);
        label (g, (n > 0 ? "+" : "") + juce::String (n), 56, y - 9, 45, 18, 12.5f,
               juce::Colour (muted), juce::Justification::centredRight);
    }
    label (g, "dB", 62, 269, 31, 18, 12, juce::Colour (muted));
    g.setColour (juce::Colour (0xff939497));
    g.drawLine (left, top, left, bottom, 1.0f);
    g.drawLine (left, bottom, right, bottom, 1.0f);

    // An actual 2048 point output FFT, updated on the message thread.
    if (showSpectrum)
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (juce::Rectangle<int> (111, 143, 1111, 263));
        juce::Path trace;
        const auto rate = std::max (processor.getSampleRate(), 44100.0);
        for (int i = 0; i < 340; ++i)
        {
            const auto x = left + (right - left) * i / 339.0f;
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
        g.setColour (juce::Colour (0xff747679).withAlpha (0.50f));
        g.strokePath (trace, juce::PathStrokeType (1.3f));
        auto shade = trace;
        shade.lineTo (right, bottom);
        shade.lineTo (left, bottom);
        shade.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8dadd).withAlpha (0.10f),
                                                 400, top, juce::Colours::transparentBlack,
                                                 400, bottom, false));
        g.fillPath (shade);
    }

    juce::Path response;
    constexpr int count = 360;
    for (int i = 0; i < count; ++i)
    {
        const auto x = left + (right - left) * i / (count - 1.0f);
        const auto y = frequencyY (xToCutoff (x));
        if (i == 0) response.startNewSubPath (x, y);
        else response.lineTo (x, y);
    }
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (juce::Rectangle<int> (111, 143, 1111, 263));
        auto area = response;
        area.lineTo (right, bottom);
        area.lineTo (left, bottom);
        area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8dbde).withAlpha (0.14f),
                                                 left, 180, juce::Colours::transparentBlack,
                                                 left, bottom, false));
        g.fillPath (area);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (response, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (response, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colour (0xffe7e8e9));
        g.strokePath (response, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
    }
    const auto nodeX = cutoffToX (currentValue ("cutoff"));
    const auto nodeY = frequencyY (currentValue ("cutoff"));
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.fillEllipse (nodeX - 15.0f, nodeY - 15.0f, 30.0f, 30.0f);
    g.setColour (juce::Colour (0xffdedfe1));
    g.drawEllipse (nodeX - 9.0f, nodeY - 9.0f, 18.0f, 18.0f, 2.1f);
    g.setColour (juce::Colour (0xff111214));
    g.fillEllipse (nodeX - 6.0f, nodeY - 6.0f, 12.0f, 12.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222326), 1250, 165,
                                             juce::Colour (0xff0d0e10), 1380, 315, false));
    g.fillRoundedRectangle (juce::Rectangle<float> (1250, 166, 129, 122), 9.0f);
    g.setColour (juce::Colour (hairline));
    g.drawRoundedRectangle (juce::Rectangle<float> (1250, 166, 129, 122), 9.0f, 1.0f);
    label (g, "ANALYZER", 1260, 181, 74, 21, 10, juce::Colour (ink),
           juce::Justification::centredLeft);
    paintButton (g, { 1336, 180, 33, 22 }, showSpectrum ? "●" : "○", showSpectrum, 13);
    label (g, "SCALE", 1260, 216, 100, 20, 11, juce::Colour (muted),
           juce::Justification::centredLeft);
    paintButton (g, { 1260, 242, 51, 32 }, "dB", ! linearScale, 13);
    paintButton (g, { 1315, 242, 53, 32 }, "Lin", linearScale, 13);
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
    const auto large = k.radius > 50;
    label (g, k.title, k.x - 105, large ? 501.0f : 626.0f,
           210, 24, large ? 13.0f : 12.0f, juce::Colour (ink));
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

    g.setColour (juce::Colour (0xfff1f2f3));
    g.drawLine (cx + std::sin (angle) * capR * 0.30f,
                cy - std::cos (angle) * capR * 0.30f,
                cx + std::sin (angle) * capR * 0.83f,
                cy - std::cos (angle) * capR * 0.83f, large ? 2.8f : 2.0f);
    label (g, formatKnobValue (k), cx - 100, large ? 742.0f : 763.0f,
           200, 22, large ? 18.0f : 15.0f, juce::Colour (ink));
}

void ViFilterEditor::paintControls (juce::Graphics& g)
{
    label (g, "FILTER MODE", 49, 499, 230, 24, 13, juce::Colour (ink));
    const auto mode = juce::roundToInt (currentValue ("filter_mode"));
    const std::array<const char*, 3> modes {{ "LP", "HP", "BP" }};
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 49.0f + 76.0f * i, 548.0f, 73.0f, 51.0f },
                     modes[static_cast<size_t> (i)], mode == i, 15);

    label (g, "ANALOG CHARACTER", 1167, 499, 233, 24, 13, juce::Colour (ink));
    const auto character = juce::roundToInt (currentValue ("analog_character"));
    const std::array<const char*, 3> characters {{ "Soft", "Tube", "Tape" }};
    for (int i = 0; i < 3; ++i)
        paintButton (g, { 1172.0f + 74.0f * i, 548.0f, 71.0f, 51.0f },
                     characters[static_cast<size_t> (i)], character == i, 14);

    for (const auto& knob : knobLayout)
        paintKnob (g, knob);
}

void ViFilterEditor::paintSaveOverlay (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.66f));
    g.fillRoundedRectangle ({ 472, 299, 494, 186 }, 12.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff303134), 474, 302,
                                             juce::Colour (0xff111214), 955, 485, false));
    g.fillRoundedRectangle ({ 479, 306, 480, 167 }, 10.0f);
    g.setColour (juce::Colour (0xff7a7c80));
    g.drawRoundedRectangle ({ 479, 306, 480, 167 }, 10.0f, 1.0f);
    label (g, "SAVE PRESET", 525, 326, 380, 29, 16, juce::Colour (ink),
           juce::Justification::centredLeft);
    paintButton (g, { 719, 420, 92, 37 }, "SAVE", true, 13);
    paintButton (g, { 820, 420, 94, 37 }, "CANCEL", false, 12);
}

juce::Point<float> ViFilterEditor::point (const juce::MouseEvent& e) const noexcept
{
    return { e.position.x / scaleX(), e.position.y / scaleY() };
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
        if (hit (719, 420, 92, 37)) commitSave();
        else if (hit (820, 420, 94, 37))
        {
            saving = false; saveName.setVisible (false); repaint();
        }
        return;
    }
    if (hit (572, 34, 291, 45))
    {
        openPresetMenu();
        return;
    }
    if (hit (525, 34, 45, 45) || hit (865, 34, 45, 45))
    {
        const auto current = processor.getCurrentProgram();
        const auto offset = hit (525, 34, 45, 45) ? -1 : 1;
        const auto count = processor.getNumPrograms() - 1;
        const auto next = ((current <= 0 ? 1 : current) - 1 + offset + count) % count + 1;
        processor.setCurrentProgram (next);
        formatPresetName();
        repaint();
        return;
    }
    if (hit (938, 34, 45, 45)) { switchAB (true); return; }
    if (hit (985, 34, 45, 45)) { switchAB (false); return; }
    if (hit (1048, 34, 44, 45)) { processor.undoManager().undo(); repaint(); return; }
    if (hit (1095, 34, 44, 45)) { processor.undoManager().redo(); repaint(); return; }
    for (int i = 0; i < 3; ++i)
    {
        if (hit (1200.0f + 62 * i, 34, 60, 45))
        {
            setParameter ("oversampling", static_cast<float> (i)); repaint(); return;
        }
        if (hit (49.0f + 76 * i, 548, 73, 51))
        {
            setParameter ("filter_mode", static_cast<float> (i)); repaint(); return;
        }
        if (hit (1172.0f + 74 * i, 548, 71, 51))
        {
            setParameter ("analog_character", static_cast<float> (i)); repaint(); return;
        }
    }
    if (hit (1336, 180, 33, 22)) { showSpectrum = ! showSpectrum; repaint(); return; }
    if (hit (1260, 242, 51, 32)) { linearScale = false; repaint(); return; }
    if (hit (1315, 242, 53, 32)) { linearScale = true; repaint(); return; }

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
    if (hit (111, 143, 1111, 263))
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
                            getScreenPosition().x + juce::roundToInt (572 * scaleX()),
                            getScreenPosition().y + juce::roundToInt (79 * scaleY()),
                            juce::roundToInt (291 * scaleX()), 1)),
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
