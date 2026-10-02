#include "LessonRunner.h"
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"

namespace
{
    using Targets = LessonRunner::Targets;

    void eqStep (const LessonFile::Action& a, Targets& out)
    {
        using P = LearnerEQProcessor;

        if (a.verb == "eq-reset")
        {
            for (int b = 0; b < P::maxBands; ++b)
                out.push_back ({ P::onParamId (b), 0.0f });
            out.push_back ({ P::bypassParamId, 0.0f });
        }
        else if (a.verb == "eq")
        {
            const auto band = juce::jlimit (0, P::maxBands - 1, (int) a.number (0, 1.0f) - 1);
            out.push_back ({ P::onParamId (band), 1.0f });
            out.push_back ({ P::typeParamId (band), (float) LessonFile::eqTypes().indexOf (a.args[1]) });
            out.push_back ({ P::freqParamId (band), a.number (2, 1000.0f) });
            // A band being switched on with no gain or Q written gets the
            // neutral ones, not whatever the last lesson left there.
            out.push_back ({ P::gainParamId (band), a.number (3, 0.0f) });
            out.push_back ({ P::qParamId (band), a.number (4, 0.7f) });
        }
        else if (a.verb == "eq-off")
        {
            const auto band = juce::jlimit (0, P::maxBands - 1, (int) a.number (0, 1.0f) - 1);
            out.push_back ({ P::onParamId (band), 0.0f });
        }
    }

    void compStep (const LessonFile::Action& a, Targets& out)
    {
        using P = LearnerCompProcessor;

        if (a.verb == "comp")
        {
            const std::pair<const char*, const char*> map[] {
                { "threshold", P::thresholdParamId }, { "ratio", P::ratioParamId },
                { "attack", P::attackParamId },       { "release", P::releaseParamId },
                { "knee", P::kneeParamId },           { "makeup", P::makeupParamId },
                { "mix", P::dryWetParamId } };

            for (const auto& [key, id] : map)
                if (a.has (key))
                    out.push_back ({ id, a.value (key, 0.0f) });
        }
        else if (a.verb == "sidechain")
        {
            const auto source = a.args[0] == "kick" ? P::scKick : a.args[0] == "ext" ? P::scExternal : P::scSelf;
            out.push_back ({ P::scSourceParamId, (float) source });

            if (a.has ("hpf"))
                out.push_back ({ P::scHpfParamId, a.named["hpf"] == "off" ? P::scHpfOffHz : a.value ("hpf", P::scHpfOffHz) });
        }
    }

    void verbStep (const LessonFile::Action& a, Targets& out)
    {
        using P = LearnerVerbProcessor;

        if (a.verb != "verb")
            return;

        if (a.has ("type"))
            out.push_back ({ P::typeParamId, (float) LessonFile::verbTypes().indexOf (a.named["type"]) });
        if (a.has ("routing"))
            out.push_back ({ P::routingParamId, (float) (a.named["routing"] == "send" ? P::send : P::insert) });

        const std::pair<const char*, const char*> map[] {
            { "decay", P::decayParamId }, { "predelay", P::preDelayParamId }, { "size", P::sizeParamId },
            { "damping", P::dampingParamId }, { "width", P::widthParamId }, { "mix", P::dryWetParamId } };

        for (const auto& [key, id] : map)
            if (a.has (key))
                out.push_back ({ id, a.value (key, 0.0f) });
    }
}

LessonRunner::Targets LessonRunner::targetsFor (const LessonFile::Step& step, const juce::String& plugin)
{
    Targets out;

    for (const auto& a : step.actions)
    {
        if (a.verb == "bypass")
            out.push_back ({ "bypass", a.args[0] == "on" ? 1.0f : 0.0f });   // the same id in all three
        else if (plugin == "eq")
            eqStep (a, out);
        else if (plugin == "comp")
            compStep (a, out);
        else if (plugin == "verb")
            verbStep (a, out);
    }

    return out;
}

LessonRunner::~LessonRunner()
{
    finish();
}

void LessonRunner::apply (juce::AudioProcessorValueTreeState& apvts, const Targets& targets)
{
    finish();

    for (const auto& [id, value] : targets)
    {
        auto* parameter = apvts.getParameter (id);
        if (parameter == nullptr)
            continue;

        const auto to = parameter->convertTo0to1 (value);

        // Steps, switches, choices: no in-between worth watching.
        if (dynamic_cast<juce::AudioParameterFloat*> (parameter) == nullptr)
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (to);
            parameter->endChangeGesture();
            continue;
        }

        const auto from = parameter->getValue();
        if (std::abs (from - to) < 1.0e-5f)
            continue;

        parameter->beginChangeGesture();
        glides.push_back ({ parameter, from, to });
    }

    if (! glides.empty())
    {
        startMs = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }
}

void LessonRunner::timerCallback()
{
    const auto t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - startMs) / glideMs);
    const auto eased = (float) (t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow (-2.0 * t + 2.0, 3.0) / 2.0);

    for (const auto& g : glides)
        g.parameter->setValueNotifyingHost (g.from + (g.to - g.from) * eased);

    if (t >= 1.0)
        endGestures();
}

void LessonRunner::finish()
{
    for (const auto& g : glides)
        g.parameter->setValueNotifyingHost (g.to);

    endGestures();
}

void LessonRunner::endGestures()
{
    stopTimer();

    for (const auto& g : glides)
        g.parameter->endChangeGesture();

    glides.clear();
}
