#pragma once
#include "../Core/ProjectModel.h"
#include "../Core/AIBackendBridge.h"
#include "../Core/AudioEngine.h"
#include "../Engines/VocalEngine.h"
#include "../Engines/InstrumentEngine.h"
#include "../Engines/GenerativeEngine.h"
#include "../Engines/AgentEngine.h"
#include "../Engines/VideoEngine.h"
#include "SolarLookAndFeel.h"
#include "CanvasTimeline.h"
#include "SidePanels.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace solar {

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::KeyListener {
public:
    MainComponent()
        : audioEngine(project, singerLib, instrumentLib),
          generativeEngine(aiBridge),
          agentEngine(aiBridge),
          canvas(project),
          sideDock(project, audioEngine, singerLib, instrumentLib, generativeEngine, agentEngine, videoEngine) {
        setLookAndFeel(&solarLaf);
        setWantsKeyboardFocus(true);
        addKeyListener(this);

        logoImage = SolarLookAndFeel::getLogoImage(128);

        // Top Transport Controls
        playBtn.setButtonText("PLAY");
        playBtn.setClickingTogglesState(true);
        playBtn.onClick = [this] { togglePlayback(); };

        stopBtn.setButtonText("STOP");
        stopBtn.onClick = [this] {
            project.isPlaying = false;
            project.playheadBeat = 0.0;
            playBtn.setToggleState(false, juce::dontSendNotification);
            canvas.repaint();
        };

        loopBtn.setButtonText("LOOP");
        loopBtn.setClickingTogglesState(true);
        loopBtn.onClick = [this] { project.loopEnabled = loopBtn.getToggleState(); };

        bpmEditor.setText(juce::String(project.bpm, 1), false);
        bpmEditor.setJustification(juce::Justification::centred);
        bpmEditor.onReturnKey = [this] { applyBpmFromEditor(); };
        bpmEditor.onFocusLost = [this] { applyBpmFromEditor(); };

        keyBox.addItemList({ "C Maj", "A Min", "G Maj", "E Min", "D Maj", "F Maj", "Bb Maj" }, 1);
        keyBox.setSelectedId(1);

        turboBtn.setButtonText("TURBO");
        turboBtn.setClickingTogglesState(true);
        turboBtn.setToggleState(project.turboMode, juce::dontSendNotification);
        turboBtn.onClick = [this] {
            project.turboMode = turboBtn.getToggleState();
            setStatus(juce::String("Turbo Mode ") + (project.turboMode ? "Active (Low-Latency Synthesis)" : "Off (High-Quality Pre-Render)"));
        };

        bridgeBtn.setButtonText("BRIDGE");
        bridgeBtn.setClickingTogglesState(true);
        bridgeBtn.onClick = [this] {
            project.bridgeSyncEnabled = bridgeBtn.getToggleState();
            setStatus(juce::String("SolarStudio DAW Bridge ") + (project.bridgeSyncEnabled ? "Streaming to Host DAW" : "Disconnected"));
        };

        exportWavBtn.setButtonText("EXPORT WAV");
        exportWavBtn.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Export Master Mix (WAV)", juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("SolarStudio_Mix.wav"), "*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    if (f != juce::File()) {
                        bool ok = audioEngine.exportMixToWav(f);
                        setStatus(ok ? ("Exported WAV: " + f.getFileName()) : "WAV export failed.");
                    }
                });
        };

        exportMidiBtn.setButtonText("EXPORT MIDI");
        exportMidiBtn.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Export MIDI", juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("SolarStudio_Project.mid"), "*.mid");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    if (f != juce::File()) {
                        bool ok = audioEngine.exportMidi(f);
                        setStatus(ok ? ("Exported MIDI: " + f.getFileName()) : "MIDI export failed.");
                    }
                });
        };

        // Canvas Tools Row
        setupToolButton(toolSelectBtn, "Select", CanvasTool::Select, true);
        setupToolButton(toolPencilBtn, "Pencil", CanvasTool::PencilNote, false);
        setupToolButton(toolDynPenBtn, "Dynamics Pen", CanvasTool::DynamicsPen, false);
        setupToolButton(toolPitchLockBtn, "Pitch Lock Brush", CanvasTool::PitchLockBrush, false);
        setupToolButton(toolEraserBtn, "Eraser", CanvasTool::Eraser, false);

        curveLaneBox.addItemList({ "Dynamics", "Pitch", "Breath", "Tension", "Falsetto" }, 1);
        curveLaneBox.setSelectedId(1);
        curveLaneBox.onChange = [this] {
            project.activeCurveLane = curveLaneBox.getText();
            canvas.repaint();
        };

        addVocalTrackBtn.setButtonText("+ Vocal Track");
        addVocalTrackBtn.onClick = [this] {
            project.addTrack(TrackType::Vocal, "AI Vocal " + juce::String(project.nextTrackId));
            canvas.repaint();
        };

        addInstTrackBtn.setButtonText("+ AI Instrument");
        addInstTrackBtn.onClick = [this] {
            project.addTrack(TrackType::AIInstrument, "AI Inst " + juce::String(project.nextTrackId));
            canvas.repaint();
        };

        vocalToMidiBtn.setButtonText("Vocal-to-MIDI...");
        vocalToMidiBtn.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Select Vocal Audio for Vocal-to-MIDI & Lyric Extraction", juce::File(), "*.wav;*.mp3;*.flac");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    if (f.existsAsFile()) {
                        auto extracted = VocalToMidiExtractor::extractFromAudioFile(f, project.bpm, project.nextNoteId);
                        auto& tr = project.addTrack(TrackType::Vocal, f.getFileNameWithoutExtension() + " (MIDI)");
                        tr.notes = std::move(extracted);
                        canvas.repaint();
                        setStatus("Extracted " + juce::String(tr.notes.size()) + " vocal notes & lyrics. Double-click any note to edit lyrics.");
                    }
                });
        };

        clearCanvasBtn.setButtonText("Clear Roll");
        clearCanvasBtn.onClick = [this] {
            project.clearAllNotes();
            canvas.repaint();
            setStatus("Cleared piano roll notes and curves.");
        };

        canvas.onExternalFileDropped = [this](const juce::File& f) {
            auto ext = f.getFileExtension().toLowerCase();
            if (ext == ".mp4" || ext == ".mov" || ext == ".mkv") {
                videoEngine.importVideoToTimeline(project, f);
                setStatus("Dropped video onto Canvas: " + f.getFileName());
            } else {
                auto res = generativeEngine.splitAudioFile(project, f, StemSplitMode::Basic, false, {});
                setStatus(res.statusMessage);
            }
            canvas.repaint();
        };

        canvas.onStateChanged = [this] { repaint(); };
        sideDock.onStatus = [this](const juce::String& m) { setStatus(m); };
        sideDock.onRefreshCanvas = [this] { canvas.repaint(); };

        statusLabel.setText("SolarStudio v2.1.0 Ready  |  Spacebar: Play/Pause  |  Clean Empty Canvas", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, SolarPalette::champagneGold);

        for (auto* c : std::initializer_list<juce::Component*>{
                 &playBtn, &stopBtn, &loopBtn, &bpmEditor, &keyBox, &turboBtn, &bridgeBtn,
                 &exportWavBtn, &exportMidiBtn,
                 &toolSelectBtn, &toolPencilBtn, &toolDynPenBtn, &toolPitchLockBtn, &toolEraserBtn,
                 &curveLaneBox, &addVocalTrackBtn, &addInstTrackBtn, &vocalToMidiBtn, &clearCanvasBtn,
                 &canvas, &sideDock, &statusLabel }) {
            addAndMakeVisible(c);
        }

        audioEngine.initialiseAudio();
        startTimerHz(30);
        setSize(1360, 820);
    }

    ~MainComponent() override {
        stopTimer();
        audioEngine.shutdownAudio();
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(SolarPalette::obsidianBg);

        // Top Hardware Bar Background
        auto topBar = getLocalBounds().removeFromTop(54);
        juce::ColourGradient topGrad(juce::Colour(0xFF20242D), 0.0f, 0.0f,
                                     juce::Colour(0xFF14171D), 0.0f, 54.0f, false);
        g.setGradientFill(topGrad);
        g.fillRect(topBar);
        g.setColour(SolarPalette::champagneGold.withAlpha(0.35f));
        g.drawHorizontalLine(53, 0.0f, static_cast<float>(getWidth()));

        // Draw SolarStudio Logo in Top-Left Header
        if (logoImage.isValid()) {
            g.drawImage(logoImage, juce::Rectangle<float>(10.0f, 7.0f, 40.0f, 40.0f),
                        juce::RectanglePlacement::centred);
        }
        g.setColour(SolarPalette::champagneGold);
        g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        g.drawText("SOLARSTUDIO", 58, 8, 145, 22, juce::Justification::centredLeft);
        g.setColour(SolarPalette::textMuted);
        g.setFont(juce::FontOptions(10.5f));
        g.drawText("AI VOCAL & MUSIC DAW v2.1", 58, 29, 165, 16, juce::Justification::centredLeft);

        // Bottom Status Bar + Master Peak Meter
        auto bot = getLocalBounds().removeFromBottom(26);
        g.setColour(SolarPalette::panelBg);
        g.fillRect(bot);
        const float peak = juce::jlimit(0.0f, 1.0f, audioEngine.getMasterPeakLevel());
        auto meter = bot.removeFromRight(150).reduced(8, 7);
        g.setColour(SolarPalette::recessedOled);
        g.fillRoundedRectangle(meter.toFloat(), 3.0f);
        g.setColour(peak > 0.92f ? juce::Colour(0xFFE05252) : SolarPalette::solarAmber);
        g.fillRoundedRectangle(meter.toFloat().withWidth(meter.getWidth() * peak), 3.0f);
    }

    void resized() override {
        auto r = getLocalBounds();
        auto topBar = r.removeFromTop(54);
        topBar.removeFromLeft(224); // logo + brand title
        topBar = topBar.reduced(6, 12);

        playBtn.setBounds(topBar.removeFromLeft(58)); topBar.removeFromLeft(4);
        stopBtn.setBounds(topBar.removeFromLeft(54)); topBar.removeFromLeft(4);
        loopBtn.setBounds(topBar.removeFromLeft(54)); topBar.removeFromLeft(10);
        bpmEditor.setBounds(topBar.removeFromLeft(64)); topBar.removeFromLeft(6);
        keyBox.setBounds(topBar.removeFromLeft(78)); topBar.removeFromLeft(10);
        turboBtn.setBounds(topBar.removeFromLeft(66)); topBar.removeFromLeft(4);
        bridgeBtn.setBounds(topBar.removeFromLeft(68));

        exportMidiBtn.setBounds(topBar.removeFromRight(92)); topBar.removeFromRight(4);
        exportWavBtn.setBounds(topBar.removeFromRight(92));

        // Tool Strip
        auto toolBar = r.removeFromTop(34).reduced(8, 4);
        toolSelectBtn.setBounds(toolBar.removeFromLeft(62)); toolBar.removeFromLeft(4);
        toolPencilBtn.setBounds(toolBar.removeFromLeft(62)); toolBar.removeFromLeft(4);
        toolDynPenBtn.setBounds(toolBar.removeFromLeft(98)); toolBar.removeFromLeft(4);
        toolPitchLockBtn.setBounds(toolBar.removeFromLeft(116)); toolBar.removeFromLeft(4);
        toolEraserBtn.setBounds(toolBar.removeFromLeft(60)); toolBar.removeFromLeft(10);
        curveLaneBox.setBounds(toolBar.removeFromLeft(96)); toolBar.removeFromLeft(10);

        clearCanvasBtn.setBounds(toolBar.removeFromRight(76)); toolBar.removeFromRight(6);
        vocalToMidiBtn.setBounds(toolBar.removeFromRight(106)); toolBar.removeFromRight(6);
        addInstTrackBtn.setBounds(toolBar.removeFromRight(106)); toolBar.removeFromRight(6);
        addVocalTrackBtn.setBounds(toolBar.removeFromRight(98));

        // Bottom Status Bar
        auto bot = r.removeFromBottom(26);
        bot.removeFromRight(156);
        statusLabel.setBounds(bot.reduced(10, 0));

        // Right AI Studio Dock + Center Canvas
        sideDock.setBounds(r.removeFromRight(360));
        canvas.setBounds(r);
    }

    void timerCallback() override {
        if (project.isPlaying) {
            canvas.repaint();
            repaint(getLocalBounds().removeFromBottom(26));
        }
    }

    // Spacebar toggles Play/Pause (when not typing inside a TextEditor)
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override {
        if (key == juce::KeyPress::spaceKey) {
            if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
                return false;
            togglePlayback();
            return true;
        }
        return false;
    }

private:
    ProjectState project;
    AIBackendBridge aiBridge;
    SingerLibrary singerLib;
    InstrumentLibrary instrumentLib;
    AudioEngine audioEngine;
    GenerativeEngine generativeEngine;
    AgentEngine agentEngine;
    VideoEngine videoEngine;

    SolarLookAndFeel solarLaf;
    juce::Image logoImage;

    CanvasTimelineComponent canvas;
    StudioSideDock sideDock;

    juce::TextButton playBtn, stopBtn, loopBtn, turboBtn, bridgeBtn, exportWavBtn, exportMidiBtn;
    juce::TextButton toolSelectBtn, toolPencilBtn, toolDynPenBtn, toolPitchLockBtn, toolEraserBtn;
    juce::TextButton addVocalTrackBtn, addInstTrackBtn, vocalToMidiBtn, clearCanvasBtn;
    juce::TextEditor bpmEditor;
    juce::ComboBox keyBox, curveLaneBox;
    juce::Label statusLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    void setupToolButton(juce::TextButton& b, const juce::String& text, CanvasTool tool, bool active) {
        b.setButtonText(text);
        b.setRadioGroupId(101);
        b.setClickingTogglesState(true);
        b.setToggleState(active, juce::dontSendNotification);
        b.onClick = [this, tool] { project.activeTool = tool; };
    }

    void togglePlayback() {
        project.isPlaying = !project.isPlaying;
        playBtn.setToggleState(project.isPlaying, juce::dontSendNotification);
        setStatus(project.isPlaying ? "Playing..." : "Paused.");
    }

    void applyBpmFromEditor() {
        double v = bpmEditor.getText().getDoubleValue();
        project.bpm = juce::jlimit(40.0, 300.0, v > 0.0 ? v : 120.0);
        bpmEditor.setText(juce::String(project.bpm, 1), false);
    }

    void setStatus(const juce::String& text) {
        statusLabel.setText(text, juce::dontSendNotification);
    }
};

} // namespace solar
