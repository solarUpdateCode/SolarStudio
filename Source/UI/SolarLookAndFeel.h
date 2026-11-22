#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "SolarStudioAssets.h"

namespace solar {

struct SolarPalette {
    static inline const juce::Colour obsidianBg     { 0xFF131519 };
    static inline const juce::Colour panelBg        { 0xFF1B1E24 };
    static inline const juce::Colour recessedOled   { 0xFF0E1014 };
    static inline const juce::Colour champagneGold  { 0xFFE5B567 };
    static inline const juce::Colour solarAmber     { 0xFFF29120 };
    static inline const juce::Colour solarCopper    { 0xFFC97832 };
    static inline const juce::Colour borderSubtle   { 0xFF2E333D };
    static inline const juce::Colour textPrimary    { 0xFFF3EFE6 };
    static inline const juce::Colour textMuted      { 0xFF9EA4B0 };
    static inline const juce::Colour pitchLockCyan  { 0xFF39C5D8 };
    static inline const juce::Colour userEditMint   { 0xFF5AD48A };
};

class SolarLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SolarLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, SolarPalette::obsidianBg);
        setColour(juce::TextButton::buttonColourId, SolarPalette::panelBg);
        setColour(juce::TextButton::buttonOnColourId, SolarPalette::solarAmber);
        setColour(juce::TextButton::textColourOffId, SolarPalette::textPrimary);
        setColour(juce::TextButton::textColourOnId, SolarPalette::obsidianBg);
        setColour(juce::ComboBox::backgroundColourId, SolarPalette::recessedOled);
        setColour(juce::ComboBox::outlineColourId, SolarPalette::champagneGold.withAlpha(0.45f));
        setColour(juce::ComboBox::textColourId, SolarPalette::textPrimary);
        setColour(juce::TextEditor::backgroundColourId, SolarPalette::recessedOled);
        setColour(juce::TextEditor::outlineColourId, SolarPalette::borderSubtle);
        setColour(juce::TextEditor::textColourId, SolarPalette::textPrimary);
        setColour(juce::ListBox::backgroundColourId, SolarPalette::recessedOled);
        setColour(juce::Slider::thumbColourId, SolarPalette::solarAmber);
        setColour(juce::Slider::trackColourId, SolarPalette::champagneGold.withAlpha(0.55f));
        setColour(juce::Label::textColourId, SolarPalette::textPrimary);
    }

    static juce::Image getLogoImage(int preferredSize = 256) {
        if (preferredSize <= 64)
            return juce::ImageCache::getFromMemory(SolarAssets::SolarStudio_logo_64_png, SolarAssets::SolarStudio_logo_64_pngSize);
        if (preferredSize <= 128)
            return juce::ImageCache::getFromMemory(SolarAssets::SolarStudio_logo_128_png, SolarAssets::SolarStudio_logo_128_pngSize);
        if (preferredSize <= 256)
            return juce::ImageCache::getFromMemory(SolarAssets::SolarStudio_logo_256_png, SolarAssets::SolarStudio_logo_256_pngSize);
        return juce::ImageCache::getFromMemory(SolarAssets::SolarStudio_logo_512_png, SolarAssets::SolarStudio_logo_512_pngSize);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool highlighted, bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool on = button.getToggleState();
        juce::ColourGradient grad(
            on ? SolarPalette::solarAmber : (highlighted ? juce::Colour(0xFF2B303A) : juce::Colour(0xFF232730)),
            bounds.getX(), bounds.getY(),
            on ? SolarPalette::solarCopper : juce::Colour(0xFF171A20),
            bounds.getX(), bounds.getBottom(), false);
        if (down) grad.multiplyOpacity(0.85f);

        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(on ? SolarPalette::champagneGold : SolarPalette::borderSubtle);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override {
        const float radius = static_cast<float>(juce::jmin(width, height)) * 0.42f;
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knurled brass/champagne metallic knob body
        juce::ColourGradient body(SolarPalette::champagneGold, cx - radius, cy - radius,
                                  SolarPalette::solarCopper.darker(0.55f), cx + radius, cy + radius, false);
        g.setGradientFill(body);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(SolarPalette::obsidianBg);
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Indicator arc
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius + 3.0f, radius + 3.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(SolarPalette::solarAmber);
        g.strokePath(arc, juce::PathStrokeType(2.2f));

        // Pointer tick
        juce::Path p;
        p.addRectangle(-1.5f, -radius * 0.85f, 3.0f, radius * 0.48f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
        g.setColour(SolarPalette::obsidianBg);
        g.fillPath(p);
    }
};

} // namespace solar
