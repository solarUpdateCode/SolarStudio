#pragma once
#include "../Core/ProjectModel.h"
#include "../Core/AudioEngine.h"
#include "../Engines/VocalEngine.h"
#include "../Engines/InstrumentEngine.h"
#include "../Engines/GenerativeEngine.h"
#include "../Engines/AgentEngine.h"
#include "../Engines/VideoEngine.h"
#include "SolarLookAndFeel.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace solar {

class StudioSideDock : public juce::TabbedComponent {
public:
    std::function<void(const juce::String&)> onStatus;
    std::function<void()> onRefreshCanvas;

    StudioSideDock(ProjectState& state,
                   AudioEngine& audio,
                   SingerLibrary& singers,
                   InstrumentLibrary& instruments,
                   GenerativeEngine& gen,
                   AgentEngine& agent,
                   VideoEngine& video)
        : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop) {
        setTabBarDepth(30);
        setColour(juce::TabbedComponent::backgroundColourId, SolarPalette::panelBg);
        setColour(juce::TabbedComponent::outlineColourId, SolarPalette::borderSubtle);

        addTab("Voices",     SolarPalette::panelBg, new VoicesTab(state, singers, *this), true);
        addTab("Instruments",SolarPalette::panelBg, new InstrumentsTab(state, instruments, *this), true);
        addTab("Inspire",    SolarPalette::panelBg, new InspireTab(state, gen, *this), true);
        addTab("Stems/SFX",  SolarPalette::panelBg, new StemsSfxTab(state, gen, *this), true);
        addTab("AI Agent",   SolarPalette::panelBg, new AgentTab(state, audio, agent, *this), true);
        addTab("Video",      SolarPalette::panelBg, new VideoTab(state, video, *this), true);
        addTab("FX/Mix",     SolarPalette::panelBg, new MixerPluginsTab(state, audio, *this), true);
    }

    void notify(const juce::String& msg) {
        if (onStatus) onStatus(msg);
        if (onRefreshCanvas) onRefreshCanvas();
    }

private:
    // -------------------------------------------------------------------------
    // Tab 1: 140+ AI Singers, Voice Cloning, Voice Changer, Speech & Doubles
    // -------------------------------------------------------------------------
    class VoicesTab : public juce::Component, public juce::ListBoxModel {
    public:
        VoicesTab(ProjectState& s, SingerLibrary& lib, StudioSideDock& p)
            : project(s), library(lib), dock(p) {
            langBox.addItemList(SingerLibrary::getSupportedLanguages(), 1);
            langBox.setSelectedId(1);
            langBox.onChange = [this] { refreshList(); };

            genreBox.addItemList(SingerLibrary::getSupportedGenres(), 1);
            genreBox.setSelectedId(1);
            genreBox.onChange = [this] { refreshList(); };

            searchBox.setTextToShowWhenEmpty("Search 144 AI voices...", SolarPalette::textMuted);
            searchBox.onTextChange = [this] { refreshList(); };

            listBox.setModel(this);
            listBox.setRowHeight(32);

            assignBtn.setButtonText("Assign Voice / Convert");
            assignBtn.onClick = [this] {
                int row = listBox.getSelectedRow();
                if (row >= 0 && row < static_cast<int>(filtered.size())) {
                    if (auto* t = project.getSelectedTrack()) {
                        t->singerId = filtered[static_cast<size_t>(row)].id;
                        t->type = TrackType::Vocal;
                        dock.notify("Assigned AI Singer: " + filtered[static_cast<size_t>(row)].name
                                    + " (" + filtered[static_cast<size_t>(row)].language + " / "
                                    + filtered[static_cast<size_t>(row)].genre + ")");
                    }
                }
            };

            cloneBtn.setButtonText("Clone Custom Voice...");
            cloneBtn.onClick = [this] {
                chooser = std::make_unique<juce::FileChooser>("Select vocal sample for AI Voice Cloning", juce::File(), "*.wav;*.flac;*.mp3");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc) {
                        auto f = fc.getResult();
                        if (f.existsAsFile()) {
                            auto cloned = library.registerCustomVoiceClone(f.getFileNameWithoutExtension() + " AI", "English", "Pop", f);
                            refreshList();
                            dock.notify("Trained custom AI Voice Clone: " + cloned.name);
                        }
                    });
            };

            doublesBtn.setButtonText("Vocal Doubles");
            doublesBtn.setClickingTogglesState(true);
            doublesBtn.onClick = [this] {
                if (auto* t = project.getSelectedTrack()) {
                    t->vocalDoubleEnabled = doublesBtn.getToggleState();
                    dock.notify(juce::String("Vocal Doubles ") + (t->vocalDoubleEnabled ? "ON" : "OFF") + " on " + t->name);
                }
            };

            speechBtn.setButtonText("Speech Mode");
            speechBtn.setClickingTogglesState(true);
            speechBtn.onClick = [this] {
                if (auto* t = project.getSelectedTrack()) {
                    t->speechMode = speechBtn.getToggleState();
                    dock.notify(juce::String("Speech Voiceover Mode ") + (t->speechMode ? "ON" : "OFF") + " on " + t->name);
                }
            };

            addAndMakeVisible(langBox);
            addAndMakeVisible(genreBox);
            addAndMakeVisible(searchBox);
            addAndMakeVisible(listBox);
            addAndMakeVisible(assignBtn);
            addAndMakeVisible(cloneBtn);
            addAndMakeVisible(doublesBtn);
            addAndMakeVisible(speechBtn);
            refreshList();
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            auto filters = r.removeFromTop(26);
            langBox.setBounds(filters.removeFromLeft(filters.getWidth() / 2 - 2));
            filters.removeFromLeft(4);
            genreBox.setBounds(filters);
            r.removeFromTop(6);
            searchBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);

            auto bottom2 = r.removeFromBottom(28);
            doublesBtn.setBounds(bottom2.removeFromLeft(bottom2.getWidth() / 2 - 2));
            bottom2.removeFromLeft(4);
            speechBtn.setBounds(bottom2);
            r.removeFromBottom(6);

            auto bottom1 = r.removeFromBottom(28);
            assignBtn.setBounds(bottom1.removeFromLeft(bottom1.getWidth() / 2 - 2));
            bottom1.removeFromLeft(4);
            cloneBtn.setBounds(bottom1);
            r.removeFromBottom(6);

            listBox.setBounds(r);
        }

        int getNumRows() override { return static_cast<int>(filtered.size()); }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (row < 0 || row >= static_cast<int>(filtered.size())) return;
            const auto& m = filtered[static_cast<size_t>(row)];
            g.fillAll(selected ? SolarPalette::solarAmber.withAlpha(0.28f)
                               : (row % 2 == 0 ? SolarPalette::recessedOled : juce::Colour(0xFF14171E)));
            g.setColour(m.isCustomClone ? SolarPalette::userEditMint : SolarPalette::textPrimary);
            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.drawText(m.name, 8, 2, w - 16, 15, juce::Justification::centredLeft);
            g.setColour(SolarPalette::textMuted);
            g.setFont(juce::FontOptions(10.5f));
            g.drawText(m.language + "  |  " + m.genre + "  |  " + m.voiceType, 8, 16, w - 16, 14, juce::Justification::centredLeft);
        }

    private:
        ProjectState& project;
        SingerLibrary& library;
        StudioSideDock& dock;
        std::vector<AISingerModel> filtered;
        juce::ComboBox langBox, genreBox;
        juce::TextEditor searchBox;
        juce::ListBox listBox;
        juce::TextButton assignBtn, cloneBtn, doublesBtn, speechBtn;
        std::unique_ptr<juce::FileChooser> chooser;

        void refreshList() {
            filtered = library.filter(langBox.getText(), genreBox.getText(), searchBox.getText());
            listBox.updateContent();
            listBox.repaint();
        }
    };

    // -------------------------------------------------------------------------
    // Tab 2: AI Instruments & Articulation Modeling
    // -------------------------------------------------------------------------
    class InstrumentsTab : public juce::Component {
    public:
        InstrumentsTab(ProjectState& s, InstrumentLibrary& lib, StudioSideDock& p)
            : project(s), library(lib), dock(p) {
            int idx = 1;
            for (const auto& m : library.getAll())
                instBox.addItem(m.name + " (" + m.ensembleType + ")", idx++);
            instBox.setSelectedId(1);
            instBox.onChange = [this] { updateArticulations(); };

            applyBtn.setButtonText("Assign AI Instrument to Track");
            applyBtn.onClick = [this] {
                const int i = instBox.getSelectedItemIndex();
                if (i >= 0 && i < static_cast<int>(library.getAll().size())) {
                    if (auto* t = project.getSelectedTrack()) {
                        const auto& m = library.getAll()[static_cast<size_t>(i)];
                        t->type = TrackType::AIInstrument;
                        t->instrumentId = m.id;
                        t->articulation = articBox.getText();
                        dock.notify("Assigned " + m.name + " [" + t->articulation + "] to " + t->name);
                    }
                }
            };

            infoLabel.setText("Includes Symphonic/Chamber/Cinematic Strings (Violins, Violas, Cellos, Bass), Solo Violin, Alto/Tenor Saxophone, Trumpet, and Armenian Duduk.",
                              juce::dontSendNotification);
            infoLabel.setColour(juce::Label::textColourId, SolarPalette::textMuted);

            addAndMakeVisible(instBox);
            addAndMakeVisible(articBox);
            addAndMakeVisible(applyBtn);
            addAndMakeVisible(infoLabel);
            updateArticulations();
        }

        void resized() override {
            auto r = getLocalBounds().reduced(10);
            instBox.setBounds(r.removeFromTop(28));
            r.removeFromTop(8);
            articBox.setBounds(r.removeFromTop(28));
            r.removeFromTop(10);
            applyBtn.setBounds(r.removeFromTop(30));
            r.removeFromTop(12);
            infoLabel.setBounds(r.removeFromTop(90));
        }

    private:
        ProjectState& project;
        InstrumentLibrary& library;
        StudioSideDock& dock;
        juce::ComboBox instBox, articBox;
        juce::TextButton applyBtn;
        juce::Label infoLabel;

        void updateArticulations() {
            articBox.clear();
            const int i = instBox.getSelectedItemIndex();
            if (i >= 0 && i < static_cast<int>(library.getAll().size())) {
                articBox.addItemList(library.getAll()[static_cast<size_t>(i)].articulations, 1);
                articBox.setSelectedId(1);
            }
        }
    };

    // -------------------------------------------------------------------------
    // Tab 3: Inspire Me (From Idea / Lyrics & Style) + History + Generative Kits
    // -------------------------------------------------------------------------
    class InspireTab : public juce::Component {
    public:
        InspireTab(ProjectState& s, GenerativeEngine& g, StudioSideDock& p)
            : project(s), gen(g), dock(p) {
            modeBox.addItem("From Idea (Text Description)", 1);
            modeBox.addItem("Lyrics & Style (Separate)", 2);
            modeBox.setSelectedId(1);

            promptEd.setMultiLine(true);
            promptEd.setTextToShowWhenEmpty("Describe your song idea or enter style (e.g. Emotional cinematic pop ballad 76 BPM)...", SolarPalette::textMuted);

            lyricsEd.setMultiLine(true);
            lyricsEd.setTextToShowWhenEmpty("Optional lyrics (for Lyrics & Style mode)...", SolarPalette::textMuted);

            generateBtn.setButtonText("Inspire Me - Generate Song");
            generateBtn.onClick = [this] {
                const bool lsMode = (modeBox.getSelectedId() == 2);
                auto item = gen.runInspireMe(project, lsMode, promptEd.getText(), lyricsEd.getText(), promptEd.getText());
                historyBox.addItem(item.title + " (" + juce::String(item.detectedBpm, 0) + " BPM)",
                                   static_cast<int>(project.inspireHistory.size()));
                historyBox.setSelectedId(static_cast<int>(project.inspireHistory.size()));
                dock.notify("Generated \"" + item.title + "\" (Auto BPM: " + juce::String(item.detectedBpm, 0) + ").");
            };

            dropBtn.setButtonText("Drop to Canvas (Auto BPM)");
            dropBtn.onClick = [this] {
                if (!project.inspireHistory.empty()) {
                    gen.dropInspireItemToCanvas(project, project.inspireHistory.front());
                    dock.notify("Dropped Inspire Me track onto Canvas with automatic tempo detection.");
                }
            };

            copyPromptBtn.setButtonText("Copy Prompt");
            copyPromptBtn.onClick = [this] {
                if (!project.inspireHistory.empty()) {
                    juce::SystemClipboard::copyTextToClipboard(project.inspireHistory.front().ideaPrompt);
                    dock.notify("Copied Inspire Me prompt to clipboard.");
                }
            };

            layerKitBtn.setButtonText("Layer Generator");
            layerKitBtn.onClick = [this] { dock.notify(gen.runGenerativeKit(project, "Layer Generator")); };
            enhancerKitBtn.setButtonText("Music Enhancer");
            enhancerKitBtn.onClick = [this] { dock.notify(gen.runGenerativeKit(project, "Music Enhancer")); };
            starterKitBtn.setButtonText("Inspire Starter");
            starterKitBtn.onClick = [this] { dock.notify(gen.runGenerativeKit(project, "Inspire Starter")); };

            addAndMakeVisible(modeBox);
            addAndMakeVisible(promptEd);
            addAndMakeVisible(lyricsEd);
            addAndMakeVisible(generateBtn);
            addAndMakeVisible(historyBox);
            addAndMakeVisible(dropBtn);
            addAndMakeVisible(copyPromptBtn);
            addAndMakeVisible(layerKitBtn);
            addAndMakeVisible(enhancerKitBtn);
            addAndMakeVisible(starterKitBtn);
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            modeBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            promptEd.setBounds(r.removeFromTop(64));
            r.removeFromTop(6);
            lyricsEd.setBounds(r.removeFromTop(64));
            r.removeFromTop(6);
            generateBtn.setBounds(r.removeFromTop(28));
            r.removeFromTop(8);
            historyBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            auto hRow = r.removeFromTop(26);
            dropBtn.setBounds(hRow.removeFromLeft(hRow.getWidth() * 6 / 10));
            hRow.removeFromLeft(4);
            copyPromptBtn.setBounds(hRow);
            r.removeFromTop(10);
            layerKitBtn.setBounds(r.removeFromTop(26));
            r.removeFromTop(4);
            enhancerKitBtn.setBounds(r.removeFromTop(26));
            r.removeFromTop(4);
            starterKitBtn.setBounds(r.removeFromTop(26));
        }

    private:
        ProjectState& project;
        GenerativeEngine& gen;
        StudioSideDock& dock;
        juce::ComboBox modeBox, historyBox;
        juce::TextEditor promptEd, lyricsEd;
        juce::TextButton generateBtn, dropBtn, copyPromptBtn, layerKitBtn, enhancerKitBtn, starterKitBtn;
    };

    // -------------------------------------------------------------------------
    // Tab 4: 4-Mode AI Stem Splitter & AI Sound Effects Generator
    // -------------------------------------------------------------------------
    class StemsSfxTab : public juce::Component {
    public:
        StemsSfxTab(ProjectState& s, GenerativeEngine& g, StudioSideDock& p)
            : project(s), gen(g), dock(p) {
            stemModeBox.addItem("Basic (Vocals + Accomp)", 1);
            stemModeBox.addItem("Professional (6 Stems)", 2);
            stemModeBox.addItem("Advanced (All Detected Stems)", 3);
            stemModeBox.addItem("Custom (Text-Prompted Target)", 4);
            stemModeBox.setSelectedId(2);

            deReverbToggle.setButtonText("Remove Vocal Reverb (Basic)");
            customStemPrompt.setTextToShowWhenEmpty("Custom target sound (e.g. acoustic guitar)...", SolarPalette::textMuted);

            splitBtn.setButtonText("Import & Split Audio (MP3/WAV/FLAC)...");
            splitBtn.onClick = [this] {
                chooser = std::make_unique<juce::FileChooser>("Choose Audio File for AI Stem Splitting", juce::File(), "*.mp3;*.wav;*.flac");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc) {
                        auto f = fc.getResult();
                        if (f.existsAsFile()) {
                            StemSplitMode m = static_cast<StemSplitMode>(stemModeBox.getSelectedId() - 1);
                            auto res = gen.splitAudioFile(project, f, m, deReverbToggle.getToggleState(), customStemPrompt.getText());
                            dock.notify(res.statusMessage);
                        }
                    });
            };

            sfxPrompt.setTextToShowWhenEmpty("Describe sound effect (e.g. rain on glass, cinematic riser)...", SolarPalette::textMuted);
            sfxLoopToggle.setButtonText("Looping SFX");
            sfxInfluence.setRange(0.0, 100.0, 1.0);
            sfxInfluence.setValue(75.0);
            sfxInfluence.setTextValueSuffix("% Influence");
            sfxGenBtn.setButtonText("Generate AI Sound Effect");
            sfxGenBtn.onClick = [this] {
                SfxRequest req;
                req.description = sfxPrompt.getText().isEmpty() ? "Cinematic Impact" : sfxPrompt.getText();
                req.looping = sfxLoopToggle.getToggleState();
                req.promptInfluence = static_cast<float>(sfxInfluence.getValue() / 100.0);
                dock.notify(gen.generateSfx(project, req));
            };

            addAndMakeVisible(stemModeBox);
            addAndMakeVisible(deReverbToggle);
            addAndMakeVisible(customStemPrompt);
            addAndMakeVisible(splitBtn);
            addAndMakeVisible(sfxPrompt);
            addAndMakeVisible(sfxLoopToggle);
            addAndMakeVisible(sfxInfluence);
            addAndMakeVisible(sfxGenBtn);
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            stemModeBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            deReverbToggle.setBounds(r.removeFromTop(24));
            r.removeFromTop(4);
            customStemPrompt.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            splitBtn.setBounds(r.removeFromTop(28));
            r.removeFromTop(18);

            sfxPrompt.setBounds(r.removeFromTop(28));
            r.removeFromTop(6);
            sfxLoopToggle.setBounds(r.removeFromTop(24));
            r.removeFromTop(4);
            sfxInfluence.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            sfxGenBtn.setBounds(r.removeFromTop(28));
        }

    private:
        ProjectState& project;
        GenerativeEngine& gen;
        StudioSideDock& dock;
        juce::ComboBox stemModeBox;
        juce::ToggleButton deReverbToggle, sfxLoopToggle;
        juce::TextEditor customStemPrompt, sfxPrompt;
        juce::Slider sfxInfluence;
        juce::TextButton splitBtn, sfxGenBtn;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    // -------------------------------------------------------------------------
    // Tab 5: AI Agent & Natural Language Control (Built-In + ChatGPT/Claude)
    // -------------------------------------------------------------------------
    class AgentTab : public juce::Component {
    public:
        AgentTab(ProjectState& s, AudioEngine& a, AgentEngine& ag, StudioSideDock& p)
            : project(s), audio(a), agent(ag), dock(p) {
            providerBox.addItemList({ "Built-In Solar Agent", "OpenAI (ChatGPT)", "Anthropic (Claude)", "Google Gemini", "Local Ollama / MCP" }, 1);
            providerBox.setSelectedId(1);

            logBox.setMultiLine(true);
            logBox.setReadOnly(true);
            logBox.setText("Solar AI Agent ready.\nTry: \"estimate chords\", \"generate harmony\", \"add reverb\", or \"set bpm to 128\".\n");

            inputBox.setTextToShowWhenEmpty("Type a DAW command...", SolarPalette::textMuted);
            inputBox.onReturnKey = [this] { sendCommand(); };

            sendBtn.setButtonText("Run");
            sendBtn.onClick = [this] { sendCommand(); };

            chordBtn.setButtonText("Estimate Key & Chords");
            chordBtn.onClick = [this] {
                auto reply = agent.estimateChordsAndKey(project);
                logBox.moveCaretToEnd();
                logBox.insertTextAtCaret("\n[Agent] " + reply + "\n");
                dock.notify(reply);
            };

            harmonyBtn.setButtonText("Generate Harmony Track");
            harmonyBtn.onClick = [this] {
                auto reply = agent.generateHarmonyTrack(project, 4);
                logBox.moveCaretToEnd();
                logBox.insertTextAtCaret("\n[Agent] " + reply + "\n");
                dock.notify(reply);
            };

            addAndMakeVisible(providerBox);
            addAndMakeVisible(logBox);
            addAndMakeVisible(inputBox);
            addAndMakeVisible(sendBtn);
            addAndMakeVisible(chordBtn);
            addAndMakeVisible(harmonyBtn);
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            providerBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            auto quick = r.removeFromTop(26);
            chordBtn.setBounds(quick.removeFromLeft(quick.getWidth() / 2 - 2));
            quick.removeFromLeft(4);
            harmonyBtn.setBounds(quick);
            r.removeFromTop(6);

            auto bottom = r.removeFromBottom(28);
            sendBtn.setBounds(bottom.removeFromRight(56));
            bottom.removeFromRight(4);
            inputBox.setBounds(bottom);
            r.removeFromBottom(6);

            logBox.setBounds(r);
        }

    private:
        ProjectState& project;
        AudioEngine& audio;
        AgentEngine& agent;
        StudioSideDock& dock;
        juce::ComboBox providerBox;
        juce::TextEditor logBox, inputBox;
        juce::TextButton sendBtn, chordBtn, harmonyBtn;

        void sendCommand() {
            auto cmd = inputBox.getText().trim();
            if (cmd.isEmpty()) return;
            inputBox.clear();
            auto reply = agent.executeNaturalLanguageCommand(project, cmd, audio.getKnownPlugins());
            logBox.moveCaretToEnd();
            logBox.insertTextAtCaret("\n[You] " + cmd + "\n[Agent] " + reply + "\n");
            dock.notify(reply);
        }
    };

    // -------------------------------------------------------------------------
    // Tab 6: Video Track, Video Composer Agent & AI Music Video Generator
    // -------------------------------------------------------------------------
    class VideoTab : public juce::Component {
    public:
        VideoTab(ProjectState& s, VideoEngine& v, StudioSideDock& p)
            : project(s), video(v), dock(p) {
            importBtn.setButtonText("Import Video to Timeline (MP4/MOV)...");
            importBtn.onClick = [this] {
                chooser = std::make_unique<juce::FileChooser>("Import Video", juce::File(), "*.mp4;*.mov;*.mkv");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc) {
                        auto f = fc.getResult();
                        if (f.existsAsFile()) {
                            auto clip = video.importVideoToTimeline(project, f);
                            dock.notify("Imported video \"" + clip.title + "\" with " + juce::String(clip.cues.size()) + " detected scene cues.");
                        }
                    });
            };

            composeBtn.setButtonText("1-Click AI Video Composer Agent");
            composeBtn.onClick = [this] {
                dock.notify(video.runVideoComposerAgent(project));
            };

            mvModeBox.addItem("Highly Customized (Full-Length)", 1);
            mvModeBox.addItem("Promo Clip (Short Social Cut)", 2);
            mvModeBox.setSelectedId(2);

            mvStyleBox.setText("Cinematic Golden Solar Neon");
            mvGenBtn.setButtonText("Generate AI Music Video");
            mvGenBtn.onClick = [this] {
                auto mode = (mvModeBox.getSelectedId() == 1) ? MusicVideoMode::HighlyCustomized : MusicVideoMode::PromoClip;
                auto job = video.startMusicVideoGeneration(mode, mvStyleBox.getText(), project.loopStartBeat, project.loopEndBeat);
                dock.notify(job.status);
            };

            addAndMakeVisible(importBtn);
            addAndMakeVisible(composeBtn);
            addAndMakeVisible(mvModeBox);
            addAndMakeVisible(mvStyleBox);
            addAndMakeVisible(mvGenBtn);
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            importBtn.setBounds(r.removeFromTop(28));
            r.removeFromTop(6);
            composeBtn.setBounds(r.removeFromTop(28));
            r.removeFromTop(18);
            mvModeBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(6);
            mvStyleBox.setBounds(r.removeFromTop(26));
            r.removeFromTop(8);
            mvGenBtn.setBounds(r.removeFromTop(28));
        }

    private:
        ProjectState& project;
        VideoEngine& video;
        StudioSideDock& dock;
        juce::TextButton importBtn, composeBtn, mvGenBtn;
        juce::ComboBox mvModeBox;
        juce::TextEditor mvStyleBox;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    // -------------------------------------------------------------------------
    // Tab 7: Third-Party VST3/AU Plugin Host & Master Bus Mixer
    // -------------------------------------------------------------------------
    class MixerPluginsTab : public juce::Component {
    public:
        MixerPluginsTab(ProjectState& s, AudioEngine& a, StudioSideDock& p)
            : project(s), audio(a), dock(p) {
            scanBtn.setButtonText("Scan Installed VST3 / AU Plugins");
            scanBtn.onClick = [this] {
                int count = audio.scanInstalledPlugins();
                pluginLog.setText("Scanned plugins: " + juce::String(count) + " VST3/AU instruments & effects ready.\n");
                for (const auto& d : audio.getKnownPlugins().getTypes())
                    pluginLog.insertTextAtCaret(" - [" + d.pluginFormatName + "] " + d.name + "\n");
                dock.notify("Plugin scan complete (" + juce::String(count) + " found).");
            };

            limiterToggle.setButtonText("Master Bus Limiter (-0.3 dBFS)");
            limiterToggle.setToggleState(project.masterLimiterEnabled, juce::dontSendNotification);
            limiterToggle.onClick = [this] {
                project.masterLimiterEnabled = limiterToggle.getToggleState();
                dock.notify(juce::String("Master Limiter ") + (project.masterLimiterEnabled ? "Enabled" : "Bypassed"));
            };

            masterVol.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            masterVol.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
            masterVol.setRange(0.0, 1.25, 0.01);
            masterVol.setValue(project.masterVolume);
            masterVol.onValueChange = [this] { project.masterVolume = static_cast<float>(masterVol.getValue()); };

            pluginLog.setMultiLine(true);
            pluginLog.setReadOnly(true);
            pluginLog.setText("Built-In Solar FX Chain: Solar EQ, Solar Silk Reverb, Master Bus Limiter.\nClick Scan to discover installed VST3/AU plugins.");

            addAndMakeVisible(scanBtn);
            addAndMakeVisible(limiterToggle);
            addAndMakeVisible(masterVol);
            addAndMakeVisible(pluginLog);
        }

        void resized() override {
            auto r = getLocalBounds().reduced(8);
            scanBtn.setBounds(r.removeFromTop(28));
            r.removeFromTop(8);
            limiterToggle.setBounds(r.removeFromTop(24));
            r.removeFromTop(8);
            masterVol.setBounds(r.removeFromTop(90).withSizeKeepingCentre(90, 90));
            r.removeFromTop(8);
            pluginLog.setBounds(r);
        }

    private:
        ProjectState& project;
        AudioEngine& audio;
        StudioSideDock& dock;
        juce::TextButton scanBtn;
        juce::ToggleButton limiterToggle;
        juce::Slider masterVol;
        juce::TextEditor pluginLog;
    };
};

} // namespace solar
