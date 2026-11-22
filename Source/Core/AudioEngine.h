#pragma once
#include "ProjectModel.h"
#include "../Engines/VocalEngine.h"
#include "../Engines/InstrumentEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace solar {

class AudioEngine : public juce::AudioIODeviceCallback {
public:
    AudioEngine(ProjectState& state, SingerLibrary& singers, InstrumentLibrary& instruments)
        : project(state), singerLib(singers), instrumentLib(instruments) {
        pluginFormatManager.addDefaultFormats();
        audioFormatManager.registerBasicFormats();
    }

    ~AudioEngine() override {
        shutdownAudio();
    }

    void initialiseAudio() {
        deviceManager.initialiseWithDefaultDevices(0, 2);
        deviceManager.addAudioCallback(this);
    }

    void shutdownAudio() {
        deviceManager.removeAudioCallback(this);
        deviceManager.closeAudioDevice();
    }

    // Scan installed third-party VST3 / AU plugins
    int scanInstalledPlugins() {
        int found = 0;
        for (int i = 0; i < pluginFormatManager.getNumFormats(); ++i) {
            auto* fmt = pluginFormatManager.getFormat(i);
            if (!fmt) continue;
            auto searchPaths = fmt->getDefaultLocationsToSearch();
            juce::PluginDirectoryScanner scanner(knownPlugins, *fmt, searchPaths, true, juce::File());
            juce::String name;
            int guard = 0;
            while (scanner.scanNextFile(true, name) && guard++ < 64) {
                ++found;
            }
        }
        return knownPlugins.getNumTypes();
    }

    const juce::KnownPluginList& getKnownPlugins() const { return knownPlugins; }

    // Offline multi-track WAV export with Master Limiter
    bool exportMixToWav(const juce::File& targetFile, double durationBeats = 32.0) {
        const double sr = 44100.0;
        const double seconds = (durationBeats / std::max(40.0, project.bpm)) * 60.0;
        const int totalSamples = static_cast<int>(sr * std::max(1.0, seconds));

        juce::AudioBuffer<float> mix(2, totalSamples);
        mix.clear();

        VocalSynthesizer offlineVocal;
        InstrumentSynthesizer offlineInst;
        offlineVocal.prepare(sr);
        offlineInst.prepare(sr);

        const double beatsPerSample = (project.bpm / 60.0) / sr;
        const int blockSize = 512;
        juce::AudioBuffer<float> temp(2, blockSize);

        for (int pos = 0; pos < totalSamples; pos += blockSize) {
            const int n = std::min(blockSize, totalSamples - pos);
            const double startBeat = static_cast<double>(pos) * beatsPerSample;
            temp.setSize(2, n, false, true, true);
            renderTracksInternal(temp, startBeat, beatsPerSample, offlineVocal, offlineInst);

            for (int ch = 0; ch < 2; ++ch)
                mix.addFrom(ch, pos, temp, ch, 0, n);
        }

        // Apply soft master limiting
        if (project.masterLimiterEnabled) {
            const float ceiling = juce::Decibels::decibelsToGain(project.masterLimiterCeilingDb);
            for (int ch = 0; ch < 2; ++ch) {
                auto* ptr = mix.getWritePointer(ch);
                for (int i = 0; i < totalSamples; ++i)
                    ptr[i] = juce::jlimit(-ceiling, ceiling, std::tanh(ptr[i] * project.masterVolume));
            }
        }

        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream>(targetFile.createOutputStream())) {
            stream->setPosition(0);
            stream->truncate();
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(
                    wav.createWriterFor(stream.get(), sr, 2, 24, {}, 0))) {
                stream.release();
                writer->writeFromAudioSampleBuffer(mix, 0, totalSamples);
                return true;
            }
        }
        return false;
    }

    // Export all MIDI tracks to standard .mid file
    bool exportMidi(const juce::File& targetFile) const {
        juce::MidiFile midiFile;
        const short tpq = 480;
        midiFile.setTicksPerQuarterNote(tpq);

        for (const auto& t : project.tracks) {
            if (t.notes.empty()) continue;
            juce::MidiMessageSequence seq;
            seq.addEvent(juce::MidiMessage::textMetaEvent(3, t.name), 0.0);
            for (const auto& n : t.notes) {
                const double onTick = n.startBeat * tpq;
                const double offTick = (n.startBeat + n.lengthBeats) * tpq;
                seq.addEvent(juce::MidiMessage::noteOn(1, n.midiNote, n.velocity), onTick);
                if (n.lyric.isNotEmpty())
                    seq.addEvent(juce::MidiMessage::textMetaEvent(5, n.lyric), onTick);
                seq.addEvent(juce::MidiMessage::noteOff(1, n.midiNote), offTick);
            }
            seq.updateMatchedPairs();
            midiFile.addTrack(seq);
        }

        if (auto out = std::unique_ptr<juce::FileOutputStream>(targetFile.createOutputStream())) {
            out->setPosition(0);
            out->truncate();
            return midiFile.writeTo(*out);
        }
        return false;
    }

    float getMasterPeakLevel() const { return masterPeak.load(); }

    // AudioIODeviceCallback
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        sampleRate = device ? device->getCurrentSampleRate() : 44100.0;
        const int block = device ? device->getCurrentBufferSizeSamples() : 512;
        vocalSynth.prepare(sampleRate);
        instSynth.prepare(sampleRate);
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(block), 2 };
        masterLimiter.prepare(spec);
        masterLimiter.setThreshold(project.masterLimiterCeilingDb);
        masterLimiter.setRelease(45.0f);
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const*, int,
                                          float* const* outputChannelData, int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch])
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

        if (!project.isPlaying || numOutputChannels <= 0) {
            masterPeak.store(0.0f);
            return;
        }

        juce::AudioBuffer<float> mix(outputChannelData, numOutputChannels, numSamples);
        const double beatsPerSample = (project.bpm / 60.0) / std::max(8000.0, sampleRate);
        const double startBeat = project.playheadBeat;

        renderTracksInternal(mix, startBeat, beatsPerSample, vocalSynth, instSynth);

        mix.applyGain(project.masterVolume);
        if (project.masterLimiterEnabled) {
            juce::dsp::AudioBlock<float> block(mix);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            masterLimiter.setThreshold(project.masterLimiterCeilingDb);
            masterLimiter.process(ctx);
        }

        float peak = 0.0f;
        for (int ch = 0; ch < numOutputChannels; ++ch)
            peak = std::max(peak, mix.getMagnitude(ch, 0, numSamples));
        masterPeak.store(peak);

        project.playheadBeat += static_cast<double>(numSamples) * beatsPerSample;
        if (project.loopEnabled && project.playheadBeat >= project.loopEndBeat)
            project.playheadBeat = project.loopStartBeat;
    }

private:
    ProjectState& project;
    SingerLibrary& singerLib;
    InstrumentLibrary& instrumentLib;

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager pluginFormatManager;
    juce::AudioFormatManager audioFormatManager;
    juce::KnownPluginList knownPlugins;

    VocalSynthesizer vocalSynth;
    InstrumentSynthesizer instSynth;
    juce::dsp::Limiter<float> masterLimiter;

    double sampleRate = 44100.0;
    std::atomic<float> masterPeak { 0.0f };

    void renderTracksInternal(juce::AudioBuffer<float>& out,
                              double startBeat,
                              double beatsPerSample,
                              VocalSynthesizer& vSynth,
                              InstrumentSynthesizer& iSynth) {
        bool anySolo = false;
        for (const auto& t : project.tracks)
            if (t.solo) { anySolo = true; break; }

        juce::AudioBuffer<float> trackBuf(out.getNumChannels(), out.getNumSamples());
        for (const auto& t : project.tracks) {
            if (t.mute || (anySolo && !t.solo) || t.notes.empty()) continue;
            trackBuf.clear();

            if (t.type == TrackType::Vocal) {
                if (auto* singer = singerLib.findById(t.singerId))
                    vSynth.renderBlock(t, *singer, startBeat, beatsPerSample, trackBuf);
            } else if (t.type == TrackType::AIInstrument) {
                if (auto* inst = instrumentLib.findById(t.instrumentId))
                    iSynth.renderBlock(t, *inst, startBeat, beatsPerSample, trackBuf);
            }

            const float gainL = t.volume * std::cos((t.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            const float gainR = t.volume * std::sin((t.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            out.addFrom(0, 0, trackBuf, 0, 0, out.getNumSamples(), gainL);
            if (out.getNumChannels() > 1)
                out.addFrom(1, 0, trackBuf, 1, 0, out.getNumSamples(), gainR);
        }
    }
};

} // namespace solar
