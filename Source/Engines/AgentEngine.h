#pragma once
#include "../Core/ProjectModel.h"
#include "../Core/AIBackendBridge.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace solar {

struct AgentMessage {
    bool isUser = false;
    juce::String sender;
    juce::String text;
};

class AgentEngine {
public:
    explicit AgentEngine(AIBackendBridge& bridgeRef) : bridge(bridgeRef) {}

    // Key & Chord Estimation from active track melody -> writes to Chord Track
    juce::String estimateChordsAndKey(ProjectState& project) {
        project.chordTrack.clear();
        const Track* src = project.getSelectedTrack();
        const int defaultRoots[] = { 60, 67, 69, 65 };
        const char* defaultLabels[] = { "C", "G", "Am", "F" };

        if (src == nullptr || src->notes.empty()) {
            for (int bar = 0; bar < 4; ++bar) {
                project.chordTrack.push_back({ bar * 4.0, 4.0, defaultRoots[bar], bar == 2 ? "Min" : "Maj", defaultLabels[bar] });
            }
            project.keyRoot = "C";
            project.keyScale = "Major";
            return "Estimated Key: C Major. Wrote C - G - Am - F progression to the Chord Track.";
        }

        // Histogram pitch classes per 4-beat bar
        double maxBeat = 0.0;
        for (const auto& n : src->notes) maxBeat = std::max(maxBeat, n.startBeat + n.lengthBeats);
        const int numBars = std::max(1, static_cast<int>(std::ceil(maxBeat / 4.0)));
        const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        for (int bar = 0; bar < numBars; ++bar) {
            int counts[12] = {};
            for (const auto& n : src->notes) {
                if (n.startBeat >= bar * 4.0 && n.startBeat < (bar + 1) * 4.0)
                    counts[n.midiNote % 12] += 1;
            }
            int bestPc = 0;
            for (int pc = 1; pc < 12; ++pc)
                if (counts[pc] > counts[bestPc]) bestPc = pc;
            bool isMinor = (counts[(bestPc + 3) % 12] > counts[(bestPc + 4) % 12]);
            juce::String lbl = juce::String(noteNames[bestPc]) + (isMinor ? "m" : "");
            project.chordTrack.push_back({ bar * 4.0, 4.0, 60 + bestPc, isMinor ? "Min" : "Maj", lbl });
        }
        return "Analyzed melody (" + juce::String(numBars) + " bars) and populated the Chord Track.";
    }

    // Generates a separate Harmony Track from the selected Vocal/Instrument melody
    juce::String generateHarmonyTrack(ProjectState& project, int semitoneInterval = 4) {
        Track* src = project.getSelectedTrack();
        if (!src || src->notes.empty())
            return "Add notes to a melody track first so the AI Agent can harmonize them.";

        auto notesCopy = src->notes;
        auto singerCopy = src->singerId;
        auto& harm = project.addTrack(TrackType::Vocal, src->name + " (AI Harmony)");
        harm.singerId = singerCopy;
        harm.volume = 0.72f;
        for (auto n : notesCopy) {
            n.id = project.nextNoteId++;
            n.midiNote = juce::jlimit(36, 96, n.midiNote + semitoneInterval);
            harm.notes.push_back(n);
        }
        return "Generated AI Harmony track (" + juce::String(semitoneInterval > 0 ? "+" : "") + juce::String(semitoneInterval) + " semitones).";
    }

    // Natural Language / Voice Command Execution (Built-In + External LLM)
    juce::String executeNaturalLanguageCommand(ProjectState& project,
                                               const juce::String& userPrompt,
                                               const juce::KnownPluginList& installedPlugins) {
        auto lower = userPrompt.toLowerCase();

        // 1. Key & Chord Estimation
        if (lower.contains("chord") || lower.contains("key") || lower.contains("scale")) {
            return estimateChordsAndKey(project);
        }

        // 2. Harmony Generation
        if (lower.contains("harmony") || lower.contains("harmonize")) {
            int interval = lower.contains("fifth") || lower.contains("5th") ? 7 : (lower.contains("low") ? -5 : 4);
            return generateHarmonyTrack(project, interval);
        }

        // 3. Tempo control
        if (lower.contains("bpm") || lower.contains("tempo")) {
            double val = userPrompt.retainCharacters("0123456789.").getDoubleValue();
            if (val >= 40.0 && val <= 300.0) {
                project.bpm = val;
                return "Updated project tempo to " + juce::String(project.bpm, 1) + " BPM.";
            }
        }

        // 4. Automated Third-Party Plugin Search & Parameter Control
        if (lower.contains("reverb") || lower.contains("delay") || lower.contains("compressor") || lower.contains("eq") || lower.contains("plugin") || lower.contains("synth")) {
            Track* t = project.getSelectedTrack();
            if (!t) return "No track selected.";

            juce::String targetFx = lower.contains("reverb") ? "Reverb"
                                  : lower.contains("delay") ? "Delay"
                                  : lower.contains("compressor") ? "Compressor"
                                  : lower.contains("eq") ? "EQ" : "Synth";

            // Search installed third-party VST3/AU plugins first
            for (const auto& desc : installedPlugins.getTypes()) {
                if (desc.name.containsIgnoreCase(targetFx) || desc.category.containsIgnoreCase(targetFx)) {
                    t->plugins.push_back({ desc.name, desc.pluginFormatName, desc.createIdentifierString(), false, {{"mix", 0.35f}, {"drive", 0.50f}} });
                    return "Found installed " + desc.pluginFormatName + " plugin \"" + desc.name + "\" and inserted it on " + t->name + ".";
                }
            }

            // Built-in Solar FX fallback
            t->plugins.push_back({ "Solar " + targetFx, "BuiltIn", "solar." + targetFx.toLowerCase(), false, {{"mix", 0.32f}, {"decay", 0.50f}} });
            return "Inserted Solar " + targetFx + " on " + t->name + " and dialed in balanced mix parameters.";
        }

        // 5. Vocal Doubles / Speech Mode
        if (lower.contains("double")) {
            if (auto* t = project.getSelectedTrack()) {
                t->vocalDoubleEnabled = !lower.contains("off");
                return juce::String(t->vocalDoubleEnabled ? "Enabled" : "Disabled") + " AI Vocal Doubles on " + t->name + ".";
            }
        }

        if (lower.contains("turbo")) {
            project.turboMode = !lower.contains("off");
            return juce::String("Turbo Mode ") + (project.turboMode ? "enabled." : "disabled.");
        }

        // Try external / local LLM bridge if configured
        if (bridge.isLocalServerReachable()) {
            auto* req = new juce::DynamicObject();
            req->setProperty("prompt", userPrompt);
            req->setProperty("provider", bridge.config.externalLlmProvider);
            auto resp = bridge.postJson("/agent/command", juce::var(req));
            if (resp.isObject() && resp.hasProperty("reply"))
                return resp["reply"].toString();
        }

        return "Solar Agent ready (" + bridge.config.externalLlmProvider + "). Try: \"estimate chords\", \"generate harmony\", \"add reverb\", \"enable vocal doubles\", or \"set bpm to 128\".";
    }

private:
    AIBackendBridge& bridge;
};

} // namespace solar
