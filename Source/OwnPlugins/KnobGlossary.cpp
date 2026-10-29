#include "KnobGlossary.h"

namespace KnobGlossary
{
    namespace
    {
        struct Rule
        {
            const char* key;
            const char* phrases;   // '|'-separated; matched as whole words, longest first
        };

        // Order matters: the first rule that matches wins, so the specific
        // ones ("pre-delay", "low shelf", "delay time") come before the
        // general ones ("delay", "gain", "time").
        const Rule rules[] {
            { "predelay",   "pre-delay|predelay|pre delay|pre dly|predly" },
            { "lookahead",  "lookahead|look-ahead|look ahead" },
            { "makeup",     "makeup|make-up|make up|output gain|out gain|output|out" },
            { "inputgain",  "input gain|in gain|input|trim" },
            { "hpf",        "high pass|highpass|hi pass|hpf|low cut|lowcut|lo cut" },
            { "lpf",        "low pass|lowpass|lpf|high cut|highcut|hi cut" },
            { "shelf",      "low shelf|high shelf|shelf" },
            { "delaytime",  "delay time|delay|time l|time r|dly time" },
            { "earlyref",   "early reflections|early ref|early|er level|er" },
            { "threshold",  "threshold|thresh|thr" },
            { "ratio",      "ratio" },
            { "attack",     "attack|atk|att" },
            { "release",    "release|rel|recovery" },
            { "knee",       "knee" },
            { "ceiling",    "ceiling|true peak|out ceiling" },
            { "hold",       "hold" },
            { "range",      "range|floor|depth gate" },
            { "mix",        "mix|dry/wet|wet/dry|dry wet|wet|blend|parallel" },
            { "frequency",  "frequency|freq|cutoff|centre|center" },
            { "q",          "q|bandwidth|bw|resonance|reso" },
            { "eqgain",     "gain|boost|cut" },
            { "decay",      "decay|rt60|reverb time|time|length" },
            { "size",       "size|room size|room|space" },
            { "damping",    "damping|damp|hf damp|high damp|dampen|absorption" },
            { "diffusion",  "diffusion|diffuse|density" },
            { "feedback",   "feedback|fdbk|regen|repeats" },
            { "width",      "width|stereo|spread|imager" },
            { "drive",      "drive|saturation|sat|distortion|heat|dirt|warmth|amount drive" },
            { "tone",       "tone|tilt|color|colour|character|brightness|bright" },
            { "rate",       "rate|speed" },
            { "depth",      "depth|intensity|amount" },
        };

        juce::String normalise (const juce::String& name)
        {
            // Lower case, separators to spaces, padded so " q " finds "q"
            // as a word and not inside "eq".
            auto s = name.toLowerCase();
            for (auto c : { '_', '.', ':', '(', ')', '[', ']', '-' })
                if (c != '-')
                    s = s.replaceCharacter (c, ' ');
            return " " + s.trim() + " ";
        }

        bool hasWord (const juce::String& padded, const juce::String& phrase)
        {
            // "pre-delay" keeps its hyphen in both, so it matches as a phrase.
            return padded.contains (" " + phrase + " ")
                || padded.contains (" " + phrase.replace ("-", " ") + " ");
        }
    }

    juce::String keyFor (const juce::String& parameterName)
    {
        const auto padded = normalise (parameterName);
        const auto spaced = padded.replace ("-", " ");

        for (const auto& rule : rules)
            for (const auto& phrase : juce::StringArray::fromTokens (rule.phrases, "|", {}))
                if (hasWord (padded, phrase) || hasWord (spaced, phrase))
                    return rule.key;

        return {};
    }

    const juce::StringArray& keys()
    {
        static const juce::StringArray all = []
        {
            juce::StringArray k;
            for (const auto& r : rules)
                k.add (r.key);
            return k;
        }();
        return all;
    }

    int orderOf (const juce::String& key)
    {
        // The knobs that define a kind of plugin first: a compressor is
        // threshold and ratio before it is knee; a reverb is decay and
        // pre-delay before it is diffusion.
        static const juce::StringArray first { "threshold", "ratio", "attack", "release", "frequency", "eqgain", "q",
                                               "decay", "predelay", "size", "mix", "drive", "delaytime", "feedback",
                                               "rate", "depth", "tone", "shelf", "hpf", "lpf", "damping", "width",
                                               "knee", "ceiling", "lookahead", "hold", "range", "diffusion",
                                               "earlyref", "inputgain", "makeup" };
        const auto i = first.indexOf (key);
        return i >= 0 ? i : 100;
    }
}
