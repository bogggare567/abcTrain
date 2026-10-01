#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/ui/SegmentedChoice.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "PluginProcessor.h"
#include <functional>

// The sidechain row under Learner Comp's knobs: what the detector listens
// to, the key's high-pass, and whether you hear the output or the key.
//
// Three segmented rows rather than a dropdown and a knob: every choice here
// is a small closed set, and seeing all of them at once is the lesson -
// "the compressor can listen to something other than what it turns down".
// The words stay English (Sidechain, Self, Ext, Kick, HPF, Key): they are
// the words on real compressors.
class SidechainStrip : public juce::Component
{
public:
    explicit SidechainStrip (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        source.setOptions ({ LearnerCompProcessor::scSelf, LearnerCompProcessor::scExternal, LearnerCompProcessor::scKick },
                           { "Self", "Ext", "Kick" });
        hpf.setOptions ({ 0, 60, 120, 200 }, { "OFF", "60 Hz", "120 Hz", "200 Hz" });
        hpf.setUppercase (false);
        monitor.setOptions ({ 0, 1 }, { "Out", "Key" });

        source.onChange = [this] (int v) { setParam (LearnerCompProcessor::scSourceParamId, (float) v); changed(); };
        hpf.onChange = [this] (int v) { setParam (LearnerCompProcessor::scHpfParamId, v == 0 ? LearnerCompProcessor::scHpfOffHz : (float) v); changed(); };
        monitor.onChange = [this] (int v) { setParam (LearnerCompProcessor::scListenParamId, (float) v); changed(); };

        for (auto* c : { &source, &hpf, &monitor })
            addAndMakeVisible (c);

        syncFromParameters();
    }

    // Called when the person touches any of the three - the editor shows
    // what a sidechain is for at that moment.
    std::function<void()> onTouched;

    void setAccent (juce::Colour c)
    {
        for (auto* s : { &source, &hpf, &monitor })
            s->setAccent (c);
    }

    // 30 Hz from the editor: automation and presets move the parameters
    // without passing through these controls.
    void syncFromParameters()
    {
        source.setValue ((int) value (LearnerCompProcessor::scSourceParamId));
        const auto f = value (LearnerCompProcessor::scHpfParamId);
        hpf.setValue (f <= LearnerCompProcessor::scHpfOffHz + 0.5f ? 0 : nearestStep (f));
        monitor.setValue (value (LearnerCompProcessor::scListenParamId) > 0.5f ? 1 : 0);
    }

    int getPreferredWidth() const
    {
        return captionWidth ("Sidechain") + source.getPreferredWidth() + gap
             + captionWidth ("HPF") + hpf.getPreferredWidth() + gap
             + captionWidth ("Listen") + monitor.getPreferredWidth();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto font = AbcTrainLookAndFeel::microFont();

        for (auto [text, x] : { std::pair { juce::String ("Sidechain"), captionX[0] },
                                std::pair { juce::String ("HPF"), captionX[1] },
                                std::pair { juce::String ("Listen"), captionX[2] } })
            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (text),
                                                  juce::Rectangle<float> ((float) x, 0.0f, (float) captionWidth (text), (float) getHeight()),
                                                  font, theme.textDim, 1.4f, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const auto place = [&r, this] (int index, const juce::String& caption, SegmentedChoice& c, bool last)
        {
            captionX[index] = r.getX();
            r.removeFromLeft (captionWidth (caption));
            c.setBounds (r.removeFromLeft (juce::jmin (r.getWidth(), c.getPreferredWidth())));
            if (! last)
                r.removeFromLeft (gap);
        };
        place (0, "Sidechain", source, false);
        place (1, "HPF", hpf, false);
        place (2, "Listen", monitor, true);
    }

private:
    static constexpr int gap = 18;

    static int captionWidth (const juce::String& text)
    {
        return juce::roundToInt (AbcTrainLookAndFeel::trackedTextWidth (AbcTrainLookAndFeel::toCaps (text),
                                                                        AbcTrainLookAndFeel::microFont(), 1.4f)) + 10;
    }

    static int nearestStep (float hz)
    {
        int best = 60;
        for (int s : { 60, 120, 200 })
            if (std::abs (hz - (float) s) < std::abs (hz - (float) best))
                best = s;
        return best;
    }

    float value (const char* id) const
    {
        auto* raw = apvts.getRawParameterValue (id);
        return raw != nullptr ? raw->load() : 0.0f;
    }

    void setParam (const char* id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    void changed()
    {
        if (onTouched != nullptr)
            onTouched();
    }

    juce::AudioProcessorValueTreeState& apvts;
    SegmentedChoice source, hpf, monitor;
    int captionX[3] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainStrip)
};
