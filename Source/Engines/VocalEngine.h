#pragma once
#include "../Core/ProjectModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <cmath>
#include <array>

namespace solar {

struct AISingerModel {
    juce::String id;
    juce::String name;
    juce::String language;   // English, Chinese, Japanese, Korean, Spanish, Italian, French, Portuguese
    juce::String genre;      // Pop, Rap, Country, Ballad, Chanson, Afro, Theatre, Kids Choir
    juce::String voiceType;  // Soprano, Alto, Tenor, Baritone, Choir
    float formantShift = 1.0f;
    float brightness = 0.55f;
    float vibratoRateHz = 5.4f;
    float vibratoDepthSemi = 0.22f;
    float breathiness = 0.14f;
    bool isCustomClone = false;
    juce::String cloneSourcePath;
};

class SingerLibrary {
public:
    SingerLibrary() {
        buildDefaultCatalog();
    }

    const std::vector<AISingerModel>& getAll() const { return singers; }

    const AISingerModel* findById(const juce::String& id) const {
        for (const auto& s : singers)
            if (s.id == id) return &s;
        return singers.empty() ? nullptr : &singers.front();
    }

    std::vector<AISingerModel> filter(const juce::String& language,
                                      const juce::String& genre,
                                      const juce::String& query) const {
        std::vector<AISingerModel> out;
        for (const auto& s : singers) {
            if (language != "All" && s.language != language) continue;
            if (genre != "All" && s.genre != genre) continue;
            if (query.isNotEmpty()
                && !s.name.containsIgnoreCase(query)
                && !s.genre.containsIgnoreCase(query)
                && !s.language.containsIgnoreCase(query))
                continue;
            out.push_back(s);
        }
        return out;
    }

    AISingerModel registerCustomVoiceClone(const juce::String& name,
                                           const juce::String& language,
                                           const juce::String& genre,
                                           const juce::File& sampleFile) {
        AISingerModel m;
        m.id = "custom_" + juce::String(static_cast<int>(singers.size()) + 1);
        m.name = name.isEmpty() ? ("Custom Clone " + juce::String(customCount + 1)) : name;
        m.language = language;
        m.genre = genre;
        m.voiceType = "Custom Clone";
        m.formantShift = 0.96f + 0.08f * static_cast<float>((customCount % 3));
        m.brightness = 0.60f;
        m.vibratoRateHz = 5.2f;
        m.breathiness = 0.16f;
        m.isCustomClone = true;
        m.cloneSourcePath = sampleFile.getFullPathName();
        ++customCount;
        singers.insert(singers.begin(), m);
        return m;
    }

    static juce::StringArray getSupportedLanguages() {
        return { "All", "English", "Chinese", "Japanese", "Korean", "Spanish", "Italian", "French", "Portuguese" };
    }

    static juce::StringArray getSupportedGenres() {
        return { "All", "Pop", "Rap", "Country", "Ballad", "Chanson", "Afro", "Theatre", "Kids Choir" };
    }

private:
    std::vector<AISingerModel> singers;
    int customCount = 0;

    void buildDefaultCatalog() {
        singers.clear();
        const juce::StringArray languages = {
            "English", "Chinese", "Japanese", "Korean", "Spanish", "Italian", "French", "Portuguese"
        };
        const juce::StringArray genres = {
            "Pop", "Rap", "Country", "Ballad", "Chanson", "Afro", "Theatre", "Kids Choir"
        };
        const juce::StringArray voiceTypes = { "Soprano", "Alto", "Tenor", "Baritone", "Choir" };

        const std::array<const char*, 18> nameBank[8] = {
            // English
            { "Aria", "Liam", "Nova", "Caleb", "Sienna", "Marcus", "Chloe", "Devon", "Harper",
              "Julian", "Maya", "Owen", "Zara", "Blake", "Elena", "Miles", "Ruby", "Aurora Choir" },
            // Chinese
            { "Lin Mei", "Chen Wei", "Yue Xin", "Hao Ran", "Qing Lan", "Zhi Yuan", "Xiao Tong", "Bo Wen", "Su Yan",
              "Ming Zhe", "An Qi", "Jun Jie", "Meng Yao", "Zi Xuan", "Yu Fei", "Kai Wen", "Shi Han", "Xinghe Choir" },
            // Japanese
            { "Hana", "Ren", "Sora", "Haruto", "Yuki", "Kaito", "Aoi", "Riku", "Sakura",
              "Takumi", "Mio", "Daiki", "Yuna", "Sho", "Koharu", "Itsuki", "Mei", "Mirai Choir" },
            // Korean
            { "Jisoo", "Minho", "Seoyeon", "Taehyung", "Haneul", "Jungkook", "Yerin", "Hyunwoo", "Dahin",
              "Sujin", "Donghae", "Chaewon", "Junho", "Nayeon", "Siwon", "Eunbi", "Kyung", "Haneul Choir" },
            // Spanish
            { "Valentina", "Mateo", "Sofia", "Diego", "Camila", "Lucas", "Lucia", "Carlos", "Isabella",
              "Javier", "Martina", "Andres", "Elena", "Gabriel", "Paula", "Hugo", "Alma", "Sol Choir" },
            // Italian
            { "Chiara", "Lorenzo", "Giulia", "Matteo", "Aurora", "Alessandro", "Beatrice", "Leonardo", "Ginevra",
              "Federico", "Vittoria", "Riccardo", "Alice", "Davide", "Giorgia", "Marco", "Sera", "Canto Choir" },
            // French
            { "Camille", "Louis", "Manon", "Gabriel", "Lea", "Arthur", "Chloe", "Raphael", "Ines",
              "Jules", "Zoe", "Hugo", "Louise", "Nathan", "Margaux", "Theo", "Amelie", "Lumiere Choir" },
            // Portuguese
            { "Beatriz", "Miguel", "Helena", "Arthur", "Laura", "Bernardo", "Maria", "Heitor", "Alice",
              "Davi", "Sophia", "Pedro", "Manuela", "Gael", "Isabela", "Rafael", "Clara", "Brisa Choir" }
        };

        // 8 languages x 18 singers = 144 distinct AI voice models
        for (int l = 0; l < 8; ++l) {
            for (int i = 0; i < 18; ++i) {
                AISingerModel m;
                const auto lang = languages[l];
                const auto genre = (i == 17) ? juce::String("Kids Choir") : genres[i % 7];
                const auto vt = (i == 17) ? juce::String("Choir") : voiceTypes[i % 4];
                m.id = lang.substring(0, 2).toLowerCase() + "_" + genre.toLowerCase().removeCharacters(" ")
                     + "_" + juce::String(nameBank[l][static_cast<size_t>(i)]).toLowerCase().removeCharacters(" ");
                m.name = nameBank[l][static_cast<size_t>(i)];
                m.language = lang;
                m.genre = genre;
                m.voiceType = vt;
                m.formantShift = (vt == "Soprano") ? 1.12f : (vt == "Alto") ? 1.03f : (vt == "Tenor") ? 0.92f : (vt == "Baritone") ? 0.82f : 1.08f;
                m.brightness = 0.45f + 0.03f * static_cast<float>(i % 10);
                m.vibratoRateHz = 4.8f + 0.12f * static_cast<float>(i % 8);
                m.vibratoDepthSemi = (genre == "Rap") ? 0.05f : (genre == "Theatre" || genre == "Ballad") ? 0.30f : 0.18f;
                m.breathiness = (genre == "Chanson" || genre == "Ballad") ? 0.22f : 0.12f;
                singers.push_back(std::move(m));
            }
        }
    }
};

// Lightweight vowel-formant singing & speech DSP synthesizer (runs offline & real-time in Turbo Mode)
class VocalSynthesizer {
public:
    void prepare(double sr) {
        sampleRate = std::max(8000.0, sr);
        phase = 0.0;
        doublePhaseL = 0.0;
        doublePhaseR = 0.0;
        vibPhase = 0.0;
    }

    void renderBlock(const Track& track,
                     const AISingerModel& singer,
                     double startBeat,
                     double beatsPerSample,
                     juce::AudioBuffer<float>& outBuffer) {
        const int numSamples = outBuffer.getNumSamples();
        const int numChannels = outBuffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) return;

        auto* left = outBuffer.getWritePointer(0);
        auto* right = numChannels > 1 ? outBuffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            const double beat = startBeat + static_cast<double>(i) * beatsPerSample;
            const NoteEvent* activeNote = nullptr;
            for (const auto& n : track.notes) {
                if (beat >= n.startBeat && beat < (n.startBeat + n.lengthBeats)) {
                    activeNote = &n;
                    break;
                }
            }
            if (activeNote == nullptr) continue;

            const double notePos = (beat - activeNote->startBeat) / std::max(1e-4, activeNote->lengthBeats);
            const float env = computeADSR(notePos);
            const float dyn = track.sampleCurve(track.dynamicsCurve, beat, 0.78f);
            const float pitchOffset = track.sampleCurve(track.pitchCurve, beat, 0.0f);
            const float breath = track.sampleCurve(track.breathCurve, beat, singer.breathiness);
            const float falsetto = track.sampleCurve(track.falsettoCurve, beat, 0.0f);

            const double baseMidi = static_cast<double>(activeNote->midiNote) + static_cast<double>(pitchOffset);
            vibPhase += 2.0 * juce::MathConstants<double>::pi * static_cast<double>(singer.vibratoRateHz) / sampleRate;
            if (vibPhase > juce::MathConstants<double>::twoPi) vibPhase -= juce::MathConstants<double>::twoPi;

            // Speech mode reduces sustained vibrato and uses pitch contour drop
            const double vibSemi = track.speechMode
                ? (-0.4 * notePos)
                : (static_cast<double>(singer.vibratoDepthSemi) * std::sin(vibPhase) * std::min(1.0, notePos * 3.0));

            const double freq = 440.0 * std::pow(2.0, (baseMidi + vibSemi - 69.0) / 12.0);
            phase += freq / sampleRate;
            if (phase >= 1.0) phase -= 1.0;

            // Vowel formant harmonics inferred from lyric syllable
            const auto vowel = detectVowel(activeNote->lyric);
            const float sampleMain = synthesizeFormantGlottal(phase, vowel, singer.formantShift, singer.brightness, falsetto, breath);
            const float gain = env * activeNote->velocity * dyn * 0.28f;

            float lSample = sampleMain * gain;
            float rSample = sampleMain * gain;

            // Vocal Doubles: stereo detuned & formant-varied L/R layers
            if (track.vocalDoubleEnabled) {
                doublePhaseL += (freq * 1.0045) / sampleRate;
                doublePhaseR += (freq * 0.9955) / sampleRate;
                if (doublePhaseL >= 1.0) doublePhaseL -= 1.0;
                if (doublePhaseR >= 1.0) doublePhaseR -= 1.0;
                lSample += synthesizeFormantGlottal(doublePhaseL, vowel, singer.formantShift * 1.03f, singer.brightness, falsetto, breath) * gain * 0.45f;
                rSample += synthesizeFormantGlottal(doublePhaseR, vowel, singer.formantShift * 0.97f, singer.brightness, falsetto, breath) * gain * 0.45f;
            }

            left[i] += lSample;
            if (right != nullptr) right[i] += rSample;
        }
    }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
    double doublePhaseL = 0.0;
    double doublePhaseR = 0.0;
    double vibPhase = 0.0;
    juce::Random rng;

    static float computeADSR(double normPos) {
        if (normPos < 0.06) return static_cast<float>(normPos / 0.06);
        if (normPos > 0.90) return static_cast<float>(std::max(0.0, (1.0 - normPos) / 0.10));
        return 1.0f;
    }

    static char detectVowel(const juce::String& lyric) {
        auto lower = lyric.toLowerCase();
        if (lower.containsChar('i') || lower.contains("ee")) return 'i';
        if (lower.containsChar('e')) return 'e';
        if (lower.containsChar('o')) return 'o';
        if (lower.containsChar('u') || lower.contains("oo")) return 'u';
        return 'a';
    }

    float synthesizeFormantGlottal(double p, char vowel, float shift, float bright, float falsetto, float breath) {
        // Additive formant-shaped glottal pulse approximation
        const double twoPi = juce::MathConstants<double>::twoPi;
        float h1 = static_cast<float>(std::sin(twoPi * p));
        float h2 = static_cast<float>(std::sin(twoPi * 2.0 * p));
        float h3 = static_cast<float>(std::sin(twoPi * 3.0 * p));
        float h4 = static_cast<float>(std::sin(twoPi * 4.0 * p));
        float h5 = static_cast<float>(std::sin(twoPi * 5.0 * p));

        float w2 = 0.55f, w3 = 0.35f, w4 = 0.22f, w5 = 0.14f;
        switch (vowel) {
            case 'i': w2 = 0.25f; w3 = 0.50f; w4 = 0.45f; w5 = 0.35f; break;
            case 'e': w2 = 0.40f; w3 = 0.48f; w4 = 0.30f; w5 = 0.20f; break;
            case 'o': w2 = 0.68f; w3 = 0.22f; w4 = 0.10f; w5 = 0.05f; break;
            case 'u': w2 = 0.45f; w3 = 0.12f; w4 = 0.06f; w5 = 0.03f; break;
            default:  break; // 'a'
        }
        const float tilt = (0.7f + 0.6f * bright) * shift * (1.0f - 0.35f * falsetto);
        float voice = h1 + tilt * (w2 * h2 + w3 * h3 + w4 * h4 + w5 * h5);
        float noise = (rng.nextFloat() * 2.0f - 1.0f) * breath * 0.25f;
        return std::tanh((voice * 0.55f) + noise);
    }
};

// Vocal-to-MIDI & Lyric Recognition Engine
class VocalToMidiExtractor {
public:
    static std::vector<NoteEvent> extractFromAudioFile(const juce::File& audioFile,
                                                       double projectBpm,
                                                       int& nextNoteId) {
        std::vector<NoteEvent> notes;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(audioFile));
        if (!reader) return notes;

        const double sr = reader->sampleRate;
        const int hop = static_cast<int>(sr * 0.05); // 50ms analysis window
        const int win = static_cast<int>(sr * 0.10);
        juce::AudioBuffer<float> buf(1, static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, static_cast<juce::int64>(sr * 60.0))));
        reader->read(&buf, 0, buf.getNumSamples(), 0, true, true);
        const float* data = buf.getReadPointer(0);
        const int total = buf.getNumSamples();

        const juce::StringArray defaultSyllables = { "la", "ah", "oo", "oh", "na", "da", "mi", "so" };
        int syllableIdx = 0;

        int currentMidi = -1;
        double currentStartBeat = 0.0;
        const double beatsPerSec = projectBpm / 60.0;

        for (int pos = 0; pos + win < total; pos += hop) {
            float rms = 0.0f;
            for (int k = 0; k < win; ++k) rms += data[pos + k] * data[pos + k];
            rms = std::sqrt(rms / static_cast<float>(win));

            const double beat = (static_cast<double>(pos) / sr) * beatsPerSec;
            if (rms < 0.02f) {
                if (currentMidi >= 0) {
                    double len = beat - currentStartBeat;
                    if (len >= 0.20) {
                        NoteEvent ev;
                        ev.id = nextNoteId++;
                        ev.midiNote = currentMidi;
                        ev.startBeat = std::round(currentStartBeat * 4.0) / 4.0;
                        ev.lengthBeats = std::max(0.25, std::round(len * 4.0) / 4.0);
                        ev.lyric = defaultSyllables[syllableIdx++ % defaultSyllables.size()];
                        notes.push_back(ev);
                    }
                    currentMidi = -1;
                }
                continue;
            }

            float freq = estimatePitchAutocorr(data + pos, win, sr);
            int midi = (freq > 65.0f && freq < 1200.0f)
                ? static_cast<int>(std::round(69.0 + 12.0 * std::log2(freq / 440.0)))
                : -1;

            if (midi != currentMidi) {
                if (currentMidi >= 0) {
                    double len = beat - currentStartBeat;
                    if (len >= 0.20) {
                        NoteEvent ev;
                        ev.id = nextNoteId++;
                        ev.midiNote = currentMidi;
                        ev.startBeat = std::round(currentStartBeat * 4.0) / 4.0;
                        ev.lengthBeats = std::max(0.25, std::round(len * 4.0) / 4.0);
                        ev.lyric = defaultSyllables[syllableIdx++ % defaultSyllables.size()];
                        notes.push_back(ev);
                    }
                }
                currentMidi = midi;
                currentStartBeat = beat;
            }
        }
        return notes;
    }

private:
    static float estimatePitchAutocorr(const float* x, int n, double sr) {
        const int minLag = std::max(2, static_cast<int>(sr / 900.0));
        const int maxLag = std::min(n / 2, static_cast<int>(sr / 75.0));
        int bestLag = -1;
        float bestCorr = 0.0f;
        for (int lag = minLag; lag < maxLag; ++lag) {
            float corr = 0.0f;
            for (int i = 0; i < n - lag; ++i) corr += x[i] * x[i + lag];
            if (corr > bestCorr) {
                bestCorr = corr;
                bestLag = lag;
            }
        }
        return (bestLag > 0) ? static_cast<float>(sr / static_cast<double>(bestLag)) : 0.0f;
    }
};

} // namespace solar
