#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

namespace solar {

enum class TrackType {
    Vocal,
    AIInstrument,
    Audio,
    Stem,
    Chord,
    Video,
    Master
};

enum class EditSource {
    AIGenerated,
    UserDynamics,
    PitchLocked,
    UserLyric
};

enum class CanvasTool {
    Select,
    PencilNote,
    DynamicsPen,
    PitchLockBrush,
    SplitScissors,
    Eraser
};

enum class StemSplitMode {
    Basic,        // Vocals + Accompaniment (+ optional vocal reverb removal)
    Professional, // 6 stems: Vocals, Drums, Bass, Guitar, Piano, Other
    Advanced,     // All detected stems
    Custom        // Text-prompted target sound extraction
};

struct ControlPoint {
    double beat = 0.0;
    float value = 0.5f; // normalized 0..1 (or semitone offset for pitch)
    EditSource source = EditSource::AIGenerated;
};

struct PitchLockRange {
    double startBeat = 0.0;
    double endBeat = 0.0;
};

struct NoteEvent {
    int id = 0;
    int midiNote = 60;          // C4
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    float velocity = 0.85f;
    juce::String lyric = "la";
    juce::String phoneme;
    bool pitchLocked = false;
    bool userEdited = false;
    bool selected = false;
};

struct ChordEvent {
    double startBeat = 0.0;
    double lengthBeats = 4.0;
    int rootMidi = 60;
    juce::String quality = "Maj";
    juce::String label = "C";
};

struct PluginSlot {
    juce::String name;
    juce::String format = "BuiltIn"; // "VST3", "AU", "BuiltIn"
    juce::String identifier;
    bool bypassed = false;
    std::map<juce::String, float> params;
};

struct VideoCueMarker {
    double beat = 0.0;
    juce::String label;
    juce::String mood;
};

struct VideoClip {
    juce::String filePath;
    juce::String title;
    double startBeat = 0.0;
    double durationBeats = 32.0;
    double fps = 30.0;
    std::vector<VideoCueMarker> cues;
};

struct InspireHistoryItem {
    juce::String id;
    bool isLyricsAndStyleMode = false;
    juce::String ideaPrompt;
    juce::String lyrics;
    juce::String stylePrompt;
    double detectedBpm = 120.0;
    juce::String timestamp;
    juce::String title;
};

struct Track {
    int id = 0;
    juce::String name = "Track 1";
    TrackType type = TrackType::Vocal;
    juce::Colour colour = juce::Colour(0xFFE89B3C);

    // Voice / Instrument assignment
    juce::String singerId = "en_pop_aria";
    juce::String instrumentId = "strings_symphonic_violins";
    juce::String articulation = "Legato";
    bool vocalDoubleEnabled = false;
    bool speechMode = false;

    // Mixer controls
    float volume = 0.85f;
    float pan = 0.0f; // -1..+1
    bool mute = false;
    bool solo = false;
    bool armed = false;

    // Performance & pre-render state
    float preRenderProgress = 1.0f; // 0..1
    bool needsRender = false;

    // Content
    std::vector<NoteEvent> notes;
    std::vector<ControlPoint> dynamicsCurve;
    std::vector<ControlPoint> pitchCurve;
    std::vector<ControlPoint> breathCurve;
    std::vector<ControlPoint> tensionCurve;
    std::vector<ControlPoint> falsettoCurve;
    std::vector<PitchLockRange> pitchLocks;
    std::vector<PluginSlot> plugins;

    // Audio / Stem backing file (optional)
    juce::String audioFilePath;
    juce::String stemCategory; // e.g., "Vocals", "Drums", "Bass"
    bool reverbRemoved = false;

    bool isBeatPitchLocked(double beat) const {
        for (const auto& r : pitchLocks)
            if (beat >= r.startBeat && beat <= r.endBeat)
                return true;
        return false;
    }

    float sampleCurve(const std::vector<ControlPoint>& curve, double beat, float defaultVal) const {
        if (curve.empty()) return defaultVal;
        if (beat <= curve.front().beat) return curve.front().value;
        if (beat >= curve.back().beat) return curve.back().value;
        for (size_t i = 1; i < curve.size(); ++i) {
            if (beat <= curve[i].beat) {
                double span = std::max(1e-6, curve[i].beat - curve[i - 1].beat);
                float t = static_cast<float>((beat - curve[i - 1].beat) / span);
                return curve[i - 1].value + t * (curve[i].value - curve[i - 1].value);
            }
        }
        return defaultVal;
    }
};

class ProjectState {
public:
    juce::String projectName = "Untitled SolarStudio Project";
    double bpm = 120.0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    juce::String keyRoot = "C";
    juce::String keyScale = "Major";

    // Transport & Acceleration
    bool isPlaying = false;
    bool loopEnabled = false;
    double loopStartBeat = 0.0;
    double loopEndBeat = 16.0;
    double playheadBeat = 0.0;
    bool turboMode = true;
    bool preRenderEnabled = true;
    bool bridgeSyncEnabled = false;

    // Master bus
    float masterVolume = 0.90f;
    bool masterLimiterEnabled = true;
    float masterLimiterCeilingDb = -0.3f;

    // Active UI state
    int selectedTrackIndex = 0;
    CanvasTool activeTool = CanvasTool::Select;
    juce::String activeCurveLane = "Dynamics"; // Dynamics, Pitch, Breath, Tension, Falsetto

    std::vector<Track> tracks;
    std::vector<ChordEvent> chordTrack;
    std::vector<VideoClip> videoTrack;
    std::vector<InspireHistoryItem> inspireHistory;

    int nextTrackId = 1;
    int nextNoteId = 1;

    ProjectState() {
        resetToCleanWorkspace();
    }

    // Always opens with a clean, empty piano roll & arrangement (no auto-loaded demo notes)
    void resetToCleanWorkspace() {
        tracks.clear();
        chordTrack.clear();
        videoTrack.clear();
        playheadBeat = 0.0;
        isPlaying = false;
        nextTrackId = 1;
        nextNoteId = 1;

        Track leadVocal;
        leadVocal.id = nextTrackId++;
        leadVocal.name = "AI Lead Vocal";
        leadVocal.type = TrackType::Vocal;
        leadVocal.colour = juce::Colour(0xFFF09A2B);
        leadVocal.singerId = "en_pop_aria";
        leadVocal.plugins.push_back({ "Solar EQ", "BuiltIn", "solar.eq", false, {{"low", 0.5f}, {"mid", 0.55f}, {"high", 0.62f}} });
        leadVocal.plugins.push_back({ "Solar Silk Reverb", "BuiltIn", "solar.reverb", false, {{"mix", 0.22f}, {"decay", 0.45f}} });
        tracks.push_back(std::move(leadVocal));

        Track aiStrings;
        aiStrings.id = nextTrackId++;
        aiStrings.name = "AI Strings Ensemble";
        aiStrings.type = TrackType::AIInstrument;
        aiStrings.colour = juce::Colour(0xFFD6A85E);
        aiStrings.instrumentId = "strings_symphonic_violins";
        aiStrings.articulation = "Legato";
        tracks.push_back(std::move(aiStrings));

        selectedTrackIndex = 0;
    }

    Track& addTrack(TrackType type, const juce::String& name) {
        Track t;
        t.id = nextTrackId++;
        t.type = type;
        t.name = name;
        switch (type) {
            case TrackType::Vocal:        t.colour = juce::Colour(0xFFF09A2B); break;
            case TrackType::AIInstrument: t.colour = juce::Colour(0xFFD6A85E); break;
            case TrackType::Audio:        t.colour = juce::Colour(0xFF4CA8C8); break;
            case TrackType::Stem:         t.colour = juce::Colour(0xFF68B684); break;
            case TrackType::Chord:        t.colour = juce::Colour(0xFF9C7AE0); break;
            case TrackType::Video:        t.colour = juce::Colour(0xFFD46A6A); break;
            case TrackType::Master:       t.colour = juce::Colour(0xFFC8A055); break;
        }
        tracks.push_back(std::move(t));
        selectedTrackIndex = static_cast<int>(tracks.size()) - 1;
        return tracks.back();
    }

    Track* getSelectedTrack() {
        if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(tracks.size()))
            return &tracks[static_cast<size_t>(selectedTrackIndex)];
        return nullptr;
    }

    void clearAllNotes() {
        for (auto& t : tracks) {
            t.notes.clear();
            t.dynamicsCurve.clear();
            t.pitchCurve.clear();
            t.pitchLocks.clear();
        }
    }

    juce::var toJson() const {
        auto* root = new juce::DynamicObject();
        root->setProperty("projectName", projectName);
        root->setProperty("bpm", bpm);
        root->setProperty("timeSigNum", timeSigNum);
        root->setProperty("timeSigDen", timeSigDen);
        root->setProperty("keyRoot", keyRoot);
        root->setProperty("keyScale", keyScale);
        root->setProperty("turboMode", turboMode);
        root->setProperty("preRenderEnabled", preRenderEnabled);

        juce::Array<juce::var> trackArr;
        for (const auto& t : tracks) {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", t.id);
            obj->setProperty("name", t.name);
            obj->setProperty("type", static_cast<int>(t.type));
            obj->setProperty("singerId", t.singerId);
            obj->setProperty("instrumentId", t.instrumentId);
            obj->setProperty("articulation", t.articulation);
            obj->setProperty("volume", t.volume);
            obj->setProperty("pan", t.pan);
            obj->setProperty("mute", t.mute);
            obj->setProperty("solo", t.solo);

            juce::Array<juce::var> noteArr;
            for (const auto& n : t.notes) {
                auto* nObj = new juce::DynamicObject();
                nObj->setProperty("midiNote", n.midiNote);
                nObj->setProperty("startBeat", n.startBeat);
                nObj->setProperty("lengthBeats", n.lengthBeats);
                nObj->setProperty("velocity", n.velocity);
                nObj->setProperty("lyric", n.lyric);
                nObj->setProperty("pitchLocked", n.pitchLocked);
                noteArr.add(juce::var(nObj));
            }
            obj->setProperty("notes", noteArr);
            trackArr.add(juce::var(obj));
        }
        root->setProperty("tracks", trackArr);
        return juce::var(root);
    }
};

} // namespace solar
