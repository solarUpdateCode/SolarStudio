#include "BridgeEditor.h"

namespace solar {

SolarBridgeEditor::SolarBridgeEditor(SolarBridgeProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    logo = juce::ImageCache::getFromMemory(SolarAssets::SolarStudio_logo_128_png,
                                           SolarAssets::SolarStudio_logo_128_pngSize);

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem("Bridge Bus " + juce::String(ch), ch);
    channelBox.setSelectedId(processor.getBridgeChannel(), juce::dontSendNotification);
    channelBox.onChange = [this] { processor.setBridgeChannel(channelBox.getSelectedId()); };

    statusLabel.setText("Synced to Host DAW Transport (VST3 / AU Bridge)", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE5B567));

    addAndMakeVisible(channelBox);
    addAndMakeVisible(statusLabel);

    startTimerHz(15);
    setSize(420, 170);
}

void SolarBridgeEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF131519));
    g.setColour(juce::Colour(0xFFE5B567).withAlpha(0.4f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 8.0f, 1.2f);

    if (logo.isValid())
        g.drawImage(logo, juce::Rectangle<float>(16.0f, 16.0f, 52.0f, 52.0f), juce::RectanglePlacement::centred);

    g.setColour(juce::Colour(0xFFE5B567));
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("SOLARSTUDIO BRIDGE", 78, 18, 280, 24, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFFF3EFE6));
    g.setFont(juce::FontOptions(13.0f));
    juce::String syncText = "Host BPM: " + juce::String(processor.getHostBpm(), 1)
                          + "  |  PPQ: " + juce::String(processor.getHostPpq(), 2)
                          + "  |  " + (processor.isHostPlaying() ? "PLAYING" : "STOPPED");
    g.drawText(syncText, 78, 44, 320, 22, juce::Justification::centredLeft);
}

void SolarBridgeEditor::resized() {
    channelBox.setBounds(18, 88, 180, 28);
    statusLabel.setBounds(18, 126, getWidth() - 36, 24);
}

} // namespace solar
