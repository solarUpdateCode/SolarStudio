#pragma once
#include "BridgeProcessor.h"
#include "SolarStudioAssets.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace solar {

class SolarBridgeEditor : public juce::AudioProcessorEditor,
                          public juce::Timer {
public:
    explicit SolarBridgeEditor(SolarBridgeProcessor& p);
    ~SolarBridgeEditor() override { stopTimer(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override { repaint(); }

private:
    SolarBridgeProcessor& processor;
    juce::Image logo;
    juce::ComboBox channelBox;
    juce::Label statusLabel;
};

} // namespace solar
