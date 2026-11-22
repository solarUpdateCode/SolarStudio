#pragma once
#include "../Core/ProjectModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

namespace solar {

struct AIInstrumentModel {
    juce::String id;
    juce::String name;
    juce::String category; // Strings Ensemble, Solo Strings, Woodwinds, Brass, World
    juce::String ensembleType; // Symphonic, Chamber, Cinematic, Solo
    juce::StringArray articulations;
    float warmth = 0.6f;
    float bowOrReed = 0.5f;
};

class InstrumentLibrary {
public:
    InstrumentLibrary() {
        models = {
            { "strings_symphonic_violins", "Symphonic Violins I & II", "Strings Ensemble", "Symphonic",
              { "Legato", "Spiccato", "Staccato", "Pizzicato", "Tremolo", "Swell" }, 0.65f, 0.72f },
            { "strings_chamber_violas", "Chamber Violas Ensemble", "Strings Ensemble", "Chamber",
              { "Legato", "Spiccato", "Pizzicato", "Tremolo", "Sordino" }, 0.72f, 0.60f },
            { "strings_cinematic_cellos", "Cinematic Cellos Section", "Strings Ensemble", "Cinematic",
              { "Legato", "Marcato", "Spiccato", "Pizzicato", "Swell" }, 0.80f, 0.66f },
            { "strings_symphonic_bass", "Symphonic Contrabass Section", "Strings Ensemble", "Symphonic",
              { "Legato", "Staccato", "Pizzicato", "Bartok Snap" }, 0.88f, 0.55f },
            { "solo_violin", "AI Solo Violin", "Solo Strings", "Solo",
              { "Expressive Legato", "Vibrato", "Spiccato", "Pizzicato", "Harmonics" }, 0.58f, 0.80f },
            { "solo_alto_sax", "AI Alto Saxophone", "Woodwinds", "Solo",
              { "Smooth Legato", "Jazz Vibrato", "Growl", "Subtone", "Staccato" }, 0.70f, 0.76f },
            { "solo_tenor_sax", "AI Tenor Saxophone", "Woodwinds", "Solo",
              { "Smooth Legato", "Breath Subtone", "Growl", "Fall", "Staccato" }, 0.78f, 0.74f },
            { "solo_trumpet", "AI Solo Trumpet", "Brass", "Solo",
              { "Open Legato", "Marcato", "Harmon Mute", "Swell", "Shake" }, 0.52f, 0.88f },
            { "solo_duduk", "AI Armenian Duduk", "World", "Solo",
              { "Expressive Legato", "Ornament Vibrato", "Breath Swell", "Diminuendo" }, 0.84f, 0.58f }
        };
    }

    const std::vector<AIInstrumentModel>& getAll() const { return models; }

    const AIInstrumentModel* findById(const juce::String& id) const {
        for (const auto& m : models)
            if (m.id == id) return &m;
        return models.empty() ? nullptr : &models.front();
    }

private:
    std::vector<AIInstrumentModel> models;
};

class InstrumentSynthesizer {
public:
    void prepare(double sr) {
        sampleRate = std::max(8000.0, sr);
        phaseA = 0.0;
        phaseB = 0.0;
        vibPhase = 0.0;
    }

    void renderBlock(const Track& track,
                     const AIInstrumentModel& model,
                     double startBeat,
                     double beatsPerSample,
                     juce::AudioBuffer<float>& outBuffer) {
        const int numSamples = outBuffer.getNumSamples();
        const int numChannels = outBuffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) return;

        auto* left = outBuffer.getWritePointer(0);
        auto* right = numChannels > 1 ? outBuffer.getWritePointer(1) : nullptr;

        const bool isShortArtic = track.articulation.containsIgnoreCase("Staccato")
                               || track.articulation.containsIgnoreCase("Spiccato")
                               || track.articulation.containsIgnoreCase("Pizzicato");
        const bool isTremolo = track.articulation.containsIgnoreCase("Tremolo");
        const bool isEnsemble = model.ensembleType != "Solo";

        for (int i = 0; i < numSamples; ++i) {
            const double beat = startBeat + static_cast<double>(i) * beatsPerSample;
            const NoteEvent* activeNote = nullptr;
            for (const auto& n : track.notes) {
                if (beat >= n.startBeat && beat < (n.startBeat + n.lengthBeats)) {
                    activeNote = &n;
                    break;
                }
            }
            if (!activeNote) continue;

            const double normPos = (beat - activeNote->startBeat) / std::max(1e-4, activeNote->lengthBeats);
            float env = 1.0f;
            if (isShortArtic) {
                env = static_cast<float>(std::exp(-normPos * 5.5));
            } else if (normPos < 0.12) {
                env = static_cast<float>(normPos / 0.12);
            } else if (normPos > 0.88) {
                env = static_cast<float>(std::max(0.0, (1.0 - normPos) / 0.12));
            }

            if (isTremolo) {
                env *= 0.72f + 0.28f * static_cast<float>(std::sin(normPos * 42.0));
            }

            const float dyn = track.sampleCurve(track.dynamicsCurve, beat, 0.80f);
            vibPhase += 2.0 * juce::MathConstants<double>::pi * 5.2 / sampleRate;
            if (vibPhase > juce::MathConstants<double>::twoPi) vibPhase -= juce::MathConstants<double>::twoPi;

            const double freq = 440.0 * std::pow(2.0, (static_cast<double>(activeNote->midiNote) + 0.12 * std::sin(vibPhase) - 69.0) / 12.0);
            phaseA += freq / sampleRate;
            phaseB += (freq * (isEnsemble ? 1.0038 : 1.0005)) / sampleRate;
            if (phaseA >= 1.0) phaseA -= 1.0;
            if (phaseB >= 1.0) phaseB -= 1.0;

            float sigL = synthesizeTimbre(phaseA, model);
            float sigR = isEnsemble ? synthesizeTimbre(phaseB, model) : sigL;

            const float gain = env * activeNote->velocity * dyn * 0.24f;
            left[i] += sigL * gain;
            if (right != nullptr) right[i] += sigR * gain;
        }
    }

private:
    double sampleRate = 44100.0;
    double phaseA = 0.0;
    double phaseB = 0.0;
    double vibPhase = 0.0;

    static float synthesizeTimbre(double p, const AIInstrumentModel& model) {
        const double twoPi = juce::MathConstants<double>::twoPi;
        if (model.id.contains("duduk")) {
            // Hollow warm double-reed odd-harmonic dominance
            return static_cast<float>(0.68 * std::sin(twoPi * p) + 0.28 * std::sin(twoPi * 3.0 * p) + 0.10 * std::sin(twoPi * 5.0 * p));
        }
        if (model.id.contains("trumpet")) {
            // Bright brass harmonic stack
            return static_cast<float>(std::tanh(0.65 * std::sin(twoPi * p) + 0.48 * std::sin(twoPi * 2.0 * p) + 0.36 * std::sin(twoPi * 3.0 * p) + 0.24 * std::sin(twoPi * 4.0 * p)));
        }
        if (model.id.contains("sax")) {
            // Conical reed warmth
            return static_cast<float>(0.62 * std::sin(twoPi * p) + 0.44 * std::sin(twoPi * 2.0 * p) + 0.24 * std::sin(twoPi * 3.0 * p) + 0.14 * std::sin(twoPi * 5.0 * p));
        }
        // Bowed string saw-harmonic body approximation
        float saw = static_cast<float>(2.0 * p - 1.0);
        float fund = static_cast<float>(std::sin(twoPi * p));
        return (1.0f - model.warmth * 0.5f) * saw * 0.45f + fund * 0.55f;
    }
};

} // namespace solar
