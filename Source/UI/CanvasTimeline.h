#pragma once
#include "../Core/ProjectModel.h"
#include "SolarLookAndFeel.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace solar {

// "The Canvas": WYSIWYH Multi-Track Timeline + Piano Roll + Lyric Double-Click
// + Dynamics Pen + Pitch Lock Brush + Visual Edit Tracking
class CanvasTimelineComponent : public juce::Component,
                                public juce::FileDragAndDropTarget {
public:
    std::function<void(const juce::File&)> onExternalFileDropped;
    std::function<void()> onStateChanged;

    explicit CanvasTimelineComponent(ProjectState& state) : project(state) {
        setWantsKeyboardFocus(false);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(SolarPalette::recessedOled);
        auto area = getLocalBounds();

        const int trackHeaderW = 210;
        const int topRulerH = 26;
        const int chordLaneH = 24;
        const int videoLaneH = project.videoTrack.empty() ? 0 : 34;
        const int curveLaneH = 92;

        auto headerCol = area.removeFromLeft(trackHeaderW);
        auto topBar = area.removeFromTop(topRulerH);
        auto chordBar = area.removeFromTop(chordLaneH);
        auto videoBar = area.removeFromTop(videoLaneH);
        auto curveArea = area.removeFromBottom(curveLaneH);
        pianoArea = area;
        curveRect = curveArea;
        headerArea = headerCol;

        // 1. Left Track List Header Column
        g.setColour(SolarPalette::panelBg);
        g.fillRect(headerCol);
        g.setColour(SolarPalette::borderSubtle);
        g.drawVerticalLine(headerCol.getRight() - 1, 0.0f, static_cast<float>(getHeight()));

        // Top corner label
        g.setColour(SolarPalette::champagneGold);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("THE CANVAS TRACKS", headerCol.removeFromTop(topRulerH).reduced(8, 0), juce::Justification::centredLeft);

        g.setColour(SolarPalette::textMuted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Chord Track", headerCol.removeFromTop(chordLaneH).reduced(8, 0), juce::Justification::centredLeft);
        if (videoLaneH > 0)
            g.drawText("Video Track", headerCol.removeFromTop(videoLaneH).reduced(8, 0), juce::Justification::centredLeft);

        // Draw track rows
        const int rowH = 54;
        for (size_t i = 0; i < project.tracks.size(); ++i) {
            const auto& t = project.tracks[i];
            auto row = headerCol.removeFromTop(rowH).reduced(4, 2);
            const bool sel = (static_cast<int>(i) == project.selectedTrackIndex);
            g.setColour(sel ? juce::Colour(0xFF282D38) : juce::Colour(0xFF171A21));
            g.fillRoundedRectangle(row.toFloat(), 5.0f);
            g.setColour(sel ? SolarPalette::solarAmber : SolarPalette::borderSubtle);
            g.drawRoundedRectangle(row.toFloat(), 5.0f, sel ? 1.5f : 1.0f);

            // Color accent bar
            g.setColour(t.colour);
            g.fillRoundedRectangle(row.getX() + 3.0f, row.getY() + 4.0f, 4.0f, row.getHeight() - 8.0f, 2.0f);

            g.setColour(SolarPalette::textPrimary);
            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.drawText(t.name, row.getX() + 12, row.getY() + 5, row.getWidth() - 56, 18, juce::Justification::centredLeft);

            juce::String sub = (t.type == TrackType::Vocal) ? ("Singer: " + t.singerId)
                             : (t.type == TrackType::AIInstrument) ? ("Inst: " + t.articulation)
                             : (t.type == TrackType::Stem) ? ("Stem: " + t.stemCategory)
                             : "Audio / FX";
            g.setColour(SolarPalette::textMuted);
            g.setFont(juce::FontOptions(10.5f));
            g.drawText(sub, row.getX() + 12, row.getY() + 24, row.getWidth() - 56, 16, juce::Justification::centredLeft);

            // M / S badges
            auto mRect = juce::Rectangle<int>(row.getRight() - 42, row.getY() + 14, 18, 20);
            auto sRect = juce::Rectangle<int>(row.getRight() - 22, row.getY() + 14, 18, 20);
            g.setColour(t.mute ? juce::Colour(0xFFD9534F) : juce::Colour(0xFF2B303A));
            g.fillRoundedRectangle(mRect.toFloat(), 3.0f);
            g.setColour(SolarPalette::textPrimary);
            g.drawText("M", mRect, juce::Justification::centred);

            g.setColour(t.solo ? SolarPalette::solarAmber : juce::Colour(0xFF2B303A));
            g.fillRoundedRectangle(sRect.toFloat(), 3.0f);
            g.setColour(t.solo ? SolarPalette::obsidianBg : SolarPalette::textPrimary);
            g.drawText("S", sRect, juce::Justification::centred);
        }

        // 2. Beat Ruler
        g.setColour(SolarPalette::panelBg);
        g.fillRect(topBar);
        g.setColour(SolarPalette::borderSubtle);
        for (int beat = 0; beat <= 64; ++beat) {
            const float x = beatToX(beat);
            if (x > pianoArea.getRight()) break;
            const bool isBar = (beat % project.timeSigNum == 0);
            g.setColour(isBar ? SolarPalette::champagneGold.withAlpha(0.45f) : SolarPalette::borderSubtle.withAlpha(0.5f));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(topBar.getY()), static_cast<float>(getHeight()));
            if (isBar) {
                g.setColour(SolarPalette::champagneGold);
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(juce::String(beat / project.timeSigNum + 1),
                           static_cast<int>(x) + 4, topBar.getY() + 4, 28, 18, juce::Justification::centredLeft);
            }
        }

        // 3. Chord Track Lane
        g.setColour(juce::Colour(0xFF151822));
        g.fillRect(chordBar);
        for (const auto& c : project.chordTrack) {
            auto r = juce::Rectangle<float>(beatToX(c.startBeat), static_cast<float>(chordBar.getY() + 2),
                                            static_cast<float>(c.lengthBeats * pixelsPerBeat - 2.0), static_cast<float>(chordLaneH - 4));
            g.setColour(juce::Colour(0xFF53407E));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(SolarPalette::textPrimary);
            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.drawText(c.label, r.toNearestInt(), juce::Justification::centred);
        }

        // 4. Video Track Lane (if video loaded)
        if (!project.videoTrack.empty() && videoLaneH > 0) {
            const auto& v = project.videoTrack.front();
            auto vr = juce::Rectangle<float>(beatToX(v.startBeat), static_cast<float>(videoBar.getY() + 2),
                                             static_cast<float>(v.durationBeats * pixelsPerBeat), static_cast<float>(videoLaneH - 4));
            g.setColour(juce::Colour(0xFF5A2C32));
            g.fillRoundedRectangle(vr, 4.0f);
            g.setColour(SolarPalette::textPrimary);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText("VIDEO: " + v.title, vr.reduced(6, 0).toNearestInt(), juce::Justification::centredLeft);
            for (const auto& cue : v.cues) {
                float cx = beatToX(cue.beat);
                g.setColour(SolarPalette::solarAmber);
                g.drawVerticalLine(static_cast<int>(cx), vr.getY(), vr.getBottom());
            }
        }

        // 5. Piano Roll Grid (clean & empty by default until user draws or generates notes)
        for (int note = minMidi; note <= maxMidi; ++note) {
            float y = midiToY(note);
            const int pc = note % 12;
            const bool blackKey = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            g.setColour(blackKey ? juce::Colour(0xFF101217) : juce::Colour(0xFF151820));
            g.fillRect(static_cast<float>(pianoArea.getX()), y, static_cast<float>(pianoArea.getWidth()), noteHeight);
            g.setColour(SolarPalette::borderSubtle.withAlpha(0.28f));
            g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(pianoArea.getX()), static_cast<float>(pianoArea.getRight()));
        }

        // 6. Pitch-Lock Regions & Notes on Selected Track (Visual Edit Tracking)
        if (const auto* track = project.getSelectedTrack()) {
            // Highlight Pitch-Locked bands in cyan
            for (const auto& lock : track->pitchLocks) {
                float lx = beatToX(lock.startBeat);
                float lw = static_cast<float>((lock.endBeat - lock.startBeat) * pixelsPerBeat);
                g.setColour(SolarPalette::pitchLockCyan.withAlpha(0.14f));
                g.fillRect(lx, static_cast<float>(pianoArea.getY()), lw, static_cast<float>(pianoArea.getHeight()));
                g.setColour(SolarPalette::pitchLockCyan.withAlpha(0.75f));
                g.drawHorizontalLine(pianoArea.getY() + 2, lx, lx + lw);
            }

            if (track->notes.empty()) {
                g.setColour(SolarPalette::textMuted.withAlpha(0.55f));
                g.setFont(juce::FontOptions(14.0f));
                g.drawText("Clean Canvas — Select Pencil to draw notes, double-click a note to edit lyrics, or drop audio/video files here",
                           pianoArea.reduced(24), juce::Justification::centred);
            } else {
                for (const auto& n : track->notes) {
                    auto nr = noteToRect(n);
                    // Visual Edit Tracking color coding
                    juce::Colour fillCol = n.pitchLocked ? SolarPalette::pitchLockCyan
                                         : n.userEdited  ? SolarPalette::userEditMint
                                         : track->colour;
                    g.setColour(fillCol.withAlpha(0.88f));
                    g.fillRoundedRectangle(nr, 4.0f);
                    g.setColour(n.selected ? juce::Colours::white : fillCol.brighter(0.35f));
                    g.drawRoundedRectangle(nr, 4.0f, n.selected ? 2.0f : 1.0f);

                    if (n.lyric.isNotEmpty() && nr.getWidth() > 16.0f) {
                        g.setColour(SolarPalette::obsidianBg);
                        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
                        g.drawText(n.lyric, nr.reduced(4, 0).toNearestInt(), juce::Justification::centredLeft);
                    }
                }
            }

            // 7. Expressive Parameter & Dynamics Curve Lane
            g.setColour(juce::Colour(0xFF11141B));
            g.fillRect(curveArea);
            g.setColour(SolarPalette::borderSubtle);
            g.drawHorizontalLine(curveArea.getY(), static_cast<float>(curveArea.getX()), static_cast<float>(curveArea.getRight()));

            g.setColour(SolarPalette::champagneGold);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(project.activeCurveLane + " Curve  |  Visual Tracking: Amber=AI, Mint=Dynamics Edit, Cyan=Pitch Locked",
                       curveArea.getX() + 8, curveArea.getY() + 4, curveArea.getWidth() - 16, 16, juce::Justification::centredLeft);

            const auto& curve = getActiveCurve(*track);
            if (curve.size() >= 2) {
                for (size_t i = 1; i < curve.size(); ++i) {
                    const auto& p0 = curve[i - 1];
                    const auto& p1 = curve[i];
                    juce::Colour segCol = (p1.source == EditSource::PitchLocked) ? SolarPalette::pitchLockCyan
                                        : (p1.source == EditSource::UserDynamics) ? SolarPalette::userEditMint
                                        : SolarPalette::solarAmber;
                    g.setColour(segCol);
                    g.drawLine(beatToX(p0.beat), curveValToY(p0.value),
                               beatToX(p1.beat), curveValToY(p1.value), 2.2f);
                }
            }
        }

        // 8. Playhead Line
        const float px = beatToX(project.playheadBeat);
        if (px >= pianoArea.getX() && px <= pianoArea.getRight()) {
            g.setColour(SolarPalette::solarAmber);
            g.drawVerticalLine(static_cast<int>(px), static_cast<float>(topBar.getY()), static_cast<float>(getHeight()));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        // Click in Track Header column
        if (headerArea.contains(e.getPosition())) {
            const int topOffset = 26 + 24 + (project.videoTrack.empty() ? 0 : 34);
            const int idx = (e.y - topOffset) / 54;
            if (idx >= 0 && idx < static_cast<int>(project.tracks.size())) {
                auto& t = project.tracks[static_cast<size_t>(idx)];
                if (e.x >= headerArea.getRight() - 46 && e.x < headerArea.getRight() - 24)
                    t.mute = !t.mute;
                else if (e.x >= headerArea.getRight() - 24)
                    t.solo = !t.solo;
                else
                    project.selectedTrackIndex = idx;
                repaint();
                if (onStateChanged) onStateChanged();
            }
            return;
        }

        // Click in Curve Area -> Dynamics Pen drawing
        if (curveRect.contains(e.getPosition())) {
            drawCurveAt(e.getPosition());
            return;
        }

        if (!pianoArea.contains(e.getPosition())) {
            // Scrub playhead on top ruler
            project.playheadBeat = std::max(0.0, xToBeat(static_cast<float>(e.x)));
            repaint();
            return;
        }

        auto* track = project.getSelectedTrack();
        if (!track) return;

        const double beat = std::max(0.0, std::round(xToBeat(static_cast<float>(e.x)) * 4.0) / 4.0);
        const int midi = juce::jlimit(minMidi, maxMidi, yToMidi(static_cast<float>(e.y)));

        if (project.activeTool == CanvasTool::PitchLockBrush) {
            track->pitchLocks.push_back({ beat, beat + 2.0 });
            for (auto& n : track->notes)
                if (n.startBeat >= beat && n.startBeat <= beat + 2.0) n.pitchLocked = true;
            repaint();
            return;
        }

        if (project.activeTool == CanvasTool::DynamicsPen) {
            drawCurveAt(e.getPosition());
            return;
        }

        if (project.activeTool == CanvasTool::Eraser) {
            for (size_t i = 0; i < track->notes.size(); ++i) {
                if (noteToRect(track->notes[i]).contains(e.position)) {
                    track->notes.erase(track->notes.begin() + static_cast<long>(i));
                    repaint();
                    return;
                }
            }
            return;
        }

        if (project.activeTool == CanvasTool::PencilNote) {
            NoteEvent ev;
            ev.id = project.nextNoteId++;
            ev.midiNote = midi;
            ev.startBeat = beat;
            ev.lengthBeats = 1.0;
            ev.lyric = (track->type == TrackType::Vocal) ? "la" : "";
            track->notes.push_back(ev);
            repaint();
            if (onStateChanged) onStateChanged();
            return;
        }

        // Select tool: select note or scrub playhead
        for (auto& n : track->notes)
            n.selected = noteToRect(n).contains(e.position);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (curveRect.contains(e.getPosition()) || project.activeTool == CanvasTool::DynamicsPen) {
            drawCurveAt(e.getPosition());
        } else if (project.activeTool == CanvasTool::PitchLockBrush && pianoArea.contains(e.getPosition())) {
            if (auto* t = project.getSelectedTrack()) {
                double beat = std::max(0.0, xToBeat(static_cast<float>(e.x)));
                if (!t->pitchLocks.empty())
                    t->pitchLocks.back().endBeat = std::max(t->pitchLocks.back().startBeat + 0.5, beat);
                repaint();
            }
        }
    }

    // Double-click individual vocal note to edit/correct recognized lyrics
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        auto* track = project.getSelectedTrack();
        if (!track) return;
        for (auto& n : track->notes) {
            if (noteToRect(n).contains(e.position)) {
                auto* aw = new juce::AlertWindow("Edit Lyric / Syllable",
                                                 "Update lyric for note (MIDI " + juce::String(n.midiNote) + "):",
                                                 juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor("lyric", n.lyric, "Lyric:");
                aw->addButton("Apply", 1);
                aw->addButton("Cancel", 0);
                aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, noteId = n.id](int result) {
                    if (result == 1) {
                        if (auto* tr = project.getSelectedTrack()) {
                            for (auto& ev : tr->notes) {
                                if (ev.id == noteId) {
                                    ev.lyric = aw->getTextEditorContents("lyric");
                                    ev.userEdited = true;
                                    break;
                                }
                            }
                        }
                        repaint();
                    }
                    delete aw;
                }));
                return;
            }
        }
    }

    // Drag-and-drop audio/video files onto Canvas
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        return !files.isEmpty();
    }

    void filesDropped(const juce::StringArray& files, int, int) override {
        for (const auto& f : files)
            if (onExternalFileDropped) onExternalFileDropped(juce::File(f));
    }

private:
    ProjectState& project;
    juce::Rectangle<int> headerArea, pianoArea, curveRect;
    double pixelsPerBeat = 42.0;
    float noteHeight = 14.0f;
    int minMidi = 48; // C3
    int maxMidi = 84; // C6

    float beatToX(double beat) const { return static_cast<float>(pianoArea.getX()) + static_cast<float>(beat * pixelsPerBeat); }
    double xToBeat(float x) const { return static_cast<double>(x - static_cast<float>(pianoArea.getX())) / pixelsPerBeat; }
    float midiToY(int midi) const { return static_cast<float>(pianoArea.getY()) + static_cast<float>(maxMidi - midi) * noteHeight; }
    int yToMidi(float y) const { return maxMidi - static_cast<int>((y - static_cast<float>(pianoArea.getY())) / noteHeight); }
    float curveValToY(float v) const {
        return static_cast<float>(curveRect.getBottom() - 8) - juce::jlimit(0.0f, 1.0f, v) * static_cast<float>(curveRect.getHeight() - 28);
    }

    juce::Rectangle<float> noteToRect(const NoteEvent& n) const {
        return { beatToX(n.startBeat), midiToY(n.midiNote) + 1.0f,
                 static_cast<float>(std::max(10.0, n.lengthBeats * pixelsPerBeat - 2.0)), noteHeight - 2.0f };
    }

    std::vector<ControlPoint>& getActiveCurve(Track& t) const {
        if (project.activeCurveLane == "Pitch")    return t.pitchCurve;
        if (project.activeCurveLane == "Breath")   return t.breathCurve;
        if (project.activeCurveLane == "Tension")  return t.tensionCurve;
        if (project.activeCurveLane == "Falsetto") return t.falsettoCurve;
        return t.dynamicsCurve;
    }

    const std::vector<ControlPoint>& getActiveCurve(const Track& t) const {
        if (project.activeCurveLane == "Pitch")    return t.pitchCurve;
        if (project.activeCurveLane == "Breath")   return t.breathCurve;
        if (project.activeCurveLane == "Tension")  return t.tensionCurve;
        if (project.activeCurveLane == "Falsetto") return t.falsettoCurve;
        return t.dynamicsCurve;
    }

    void drawCurveAt(juce::Point<int> pt) {
        auto* t = project.getSelectedTrack();
        if (!t) return;
        const double beat = std::max(0.0, xToBeat(static_cast<float>(pt.x)));
        const float val = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(curveRect.getBottom() - 8 - pt.y) / std::max(10.0f, static_cast<float>(curveRect.getHeight() - 28)));

        auto& curve = getActiveCurve(*t);
        EditSource src = t->isBeatPitchLocked(beat) ? EditSource::PitchLocked : EditSource::UserDynamics;
        curve.push_back({ beat, val, src });
        std::sort(curve.begin(), curve.end(), [](const ControlPoint& a, const ControlPoint& b) { return a.beat < b.beat; });
        repaint();
    }
};

} // namespace solar
