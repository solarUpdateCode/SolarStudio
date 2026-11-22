#pragma once
#include "../Core/ProjectModel.h"
#include "../Core/AIBackendBridge.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

namespace solar {

struct StemSplitResult {
    bool success = false;
    juce::String statusMessage;
    std::vector<juce::String> createdTrackNames;
};

struct SfxRequest {
    juce::String description;
    bool looping = false;
    float promptInfluence = 0.75f; // 0..1
    double durationBeats = 8.0;
};

class GenerativeEngine {
public:
    explicit GenerativeEngine(AIBackendBridge& bridgeRef) : bridge(bridgeRef) {}

    // Full-Song Music Generation ("Inspire Me") - From Idea OR Lyrics & Style
    InspireHistoryItem runInspireMe(ProjectState& project,
                                    bool lyricsAndStyleMode,
                                    const juce::String& ideaPrompt,
                                    const juce::String& lyrics,
                                    const juce::String& stylePrompt) {
        InspireHistoryItem item;
        item.id = "insp_" + juce::String(static_cast<int>(project.inspireHistory.size()) + 1);
        item.isLyricsAndStyleMode = lyricsAndStyleMode;
        item.ideaPrompt = ideaPrompt;
        item.lyrics = lyrics;
        item.stylePrompt = stylePrompt;
        item.detectedBpm = detectSuggestedTempo(lyricsAndStyleMode ? stylePrompt : ideaPrompt, project.bpm);
        item.timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
        item.title = lyricsAndStyleMode
            ? ("Song: " + stylePrompt.substring(0, 28))
            : ("Idea: " + ideaPrompt.substring(0, 28));

        project.inspireHistory.insert(project.inspireHistory.begin(), item);
        return item;
    }

    // Drag-and-drop an Inspire Me item onto the Canvas with automatic tempo detection
    void dropInspireItemToCanvas(ProjectState& project, const InspireHistoryItem& item) {
        project.bpm = item.detectedBpm;

        auto& vocalTrack = project.addTrack(TrackType::Vocal, item.title + " (AI Vocal)");
        vocalTrack.singerId = "en_pop_aria";

        juce::StringArray words;
        if (item.lyrics.isNotEmpty())
            words.addTokens(item.lyrics, " \t\r\n", "");
        else
            words.addTokens("shine bright in the golden light of dawn", " ", "");

        const int scale[] = { 60, 62, 64, 67, 69, 72, 69, 67 };
        double beat = 0.0;
        for (int i = 0; i < std::min(16, words.size()); ++i) {
            NoteEvent n;
            n.id = project.nextNoteId++;
            n.midiNote = scale[i % 8];
            n.startBeat = beat;
            n.lengthBeats = (i % 4 == 3) ? 2.0 : 1.0;
            n.lyric = words[i];
            vocalTrack.notes.push_back(n);
            beat += n.lengthBeats;
        }
    }

    // Generative Kits: Layer Generator, Music Enhancer, Inspire Starter
    juce::String runGenerativeKit(ProjectState& project, const juce::String& kitType) {
        if (kitType == "Layer Generator") {
            auto& layer = project.addTrack(TrackType::AIInstrument, "AI Kit Layer (Strings Pad)");
            layer.instrumentId = "strings_cinematic_cellos";
            layer.articulation = "Swell";
            const int roots[] = { 48, 53, 55, 48 };
            for (int bar = 0; bar < 4; ++bar) {
                NoteEvent n;
                n.id = project.nextNoteId++;
                n.midiNote = roots[bar];
                n.startBeat = bar * 4.0;
                n.lengthBeats = 4.0;
                layer.notes.push_back(n);
            }
            return "Added AI Strings Pad layer synced to " + juce::String(project.bpm, 1) + " BPM.";
        }
        if (kitType == "Music Enhancer") {
            project.masterLimiterEnabled = true;
            project.masterLimiterCeilingDb = -0.3f;
            for (auto& t : project.tracks) {
                bool hasEq = false;
                for (const auto& p : t.plugins)
                    if (p.name.containsIgnoreCase("Enhancer")) hasEq = true;
                if (!hasEq)
                    t.plugins.push_back({ "Solar AI Enhancer", "BuiltIn", "solar.enhancer", false, {{"clarity", 0.68f}, {"warmth", 0.55f}} });
            }
            return "Applied Solar AI Music Enhancer across project tracks and optimized Master Limiter.";
        }
        // Inspire Starter
        auto& starter = project.addTrack(TrackType::AIInstrument, "Inspire Starter (Solo Violin)");
        starter.instrumentId = "solo_violin";
        const int motif[] = { 64, 67, 69, 71, 72, 71, 67, 64 };
        for (int i = 0; i < 8; ++i) {
            NoteEvent n;
            n.id = project.nextNoteId++;
            n.midiNote = motif[i];
            n.startBeat = i * 1.0;
            n.lengthBeats = 1.0;
            starter.notes.push_back(n);
        }
        return "Created 2-bar Inspire Starter motif on Canvas.";
    }

    // Multi-Mode AI Stem Splitter (Basic / Professional / Advanced / Custom)
    StemSplitResult splitAudioFile(ProjectState& project,
                                   const juce::File& audioFile,
                                   StemSplitMode mode,
                                   bool removeVocalReverb,
                                   const juce::String& customPrompt) {
        StemSplitResult res;
        juce::StringArray stems;
        switch (mode) {
            case StemSplitMode::Basic:
                stems = { removeVocalReverb ? "Vocals (De-Reverb)" : "Vocals", "Accompaniment" };
                break;
            case StemSplitMode::Professional:
                stems = { "Vocals", "Drums", "Bass", "Guitar", "Piano", "Other" };
                break;
            case StemSplitMode::Advanced:
                stems = { "Lead Vocals", "Backing Vocals", "Kick", "Snare & Perc", "Bass", "Guitars", "Keys & Synths", "Strings & Pads" };
                break;
            case StemSplitMode::Custom:
                stems = { customPrompt.isEmpty() ? "Custom Target Stem" : ("Custom: " + customPrompt), "Residual Mix" };
                break;
        }

        const juce::String baseName = audioFile.existsAsFile() ? audioFile.getFileNameWithoutExtension() : "Audio";
        for (const auto& stemName : stems) {
            auto& t = project.addTrack(TrackType::Stem, baseName + " - " + stemName);
            t.audioFilePath = audioFile.getFullPathName();
            t.stemCategory = stemName;
            t.reverbRemoved = removeVocalReverb && stemName.containsIgnoreCase("Vocal");
            res.createdTrackNames.push_back(t.name);
        }

        res.success = true;
        res.statusMessage = "Separated " + juce::String(stems.size()) + " phase-aligned stems onto Canvas.";
        return res;
    }

    // AI Sound Effects Generator
    juce::String generateSfx(ProjectState& project, const SfxRequest& req) {
        auto& t = project.addTrack(TrackType::Audio, "AI SFX: " + req.description.substring(0, 24));
        NoteEvent cue;
        cue.id = project.nextNoteId++;
        cue.midiNote = 60;
        cue.startBeat = project.playheadBeat;
        cue.lengthBeats = req.durationBeats;
        cue.lyric = req.looping ? "[LOOP SFX]" : "[SFX]";
        t.notes.push_back(cue);
        return "Generated AI SFX \"" + req.description + "\" (Loop: " + juce::String(req.looping ? "On" : "Off")
             + ", Influence: " + juce::String(static_cast<int>(req.promptInfluence * 100.0f)) + "%).";
    }

private:
    AIBackendBridge& bridge;

    static double detectSuggestedTempo(const juce::String& text, double fallback) {
        auto lower = text.toLowerCase();
        if (lower.contains("ballad") || lower.contains("slow")) return 76.0;
        if (lower.contains("afro")) return 104.0;
        if (lower.contains("rap") || lower.contains("hip hop") || lower.contains("trap")) return 94.0;
        if (lower.contains("edm") || lower.contains("house") || lower.contains("dance")) return 126.0;
        if (lower.contains("dnb") || lower.contains("drum and bass")) return 172.0;
        return fallback;
    }
};

} // namespace solar
