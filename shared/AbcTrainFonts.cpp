#include "AbcTrainFonts.h"

#include "FontBinaryData.h"

#include <vector>

namespace AbcTrainFonts
{

namespace
{
    struct EmbeddedFonts
    {
        EmbeddedFonts()
        {
            // Every face in the binary target, whatever it is called: the
            // list is derived from the generated data rather than repeated
            // here, so adding a weight to CMakeLists.txt is the only edit
            // needed. A file that fails to parse is skipped rather than
            // fatal - a missing weight should cost a weight, not a window.
            for (int i = 0; i < FontBinaryData::namedResourceListSize; ++i)
            {
                const auto* name = FontBinaryData::namedResourceList[i];
                int size = 0;

                if (const auto* data = FontBinaryData::getNamedResource (name, size);
                    data != nullptr && size > 0)
                {
                    if (auto face = juce::Typeface::createSystemTypefaceFor (data, (size_t) size))
                        faces.push_back (std::move (face));
                }
            }
        }

        std::vector<juce::Typeface::Ptr> faces;
    };

    EmbeddedFonts& embedded()
    {
        // Function-local static: the registration happens on first use,
        // and the Ptrs live until the process ends. JUCE keeps a typeface
        // registered exactly as long as something owns it, so letting
        // these go out of scope would unregister the whole interface font
        // in the middle of a paint.
        static EmbeddedFonts fonts;
        return fonts;
    }

    juce::Font make (const char* family, const char* fallback,
                     float height, const juce::String& style)
    {
        ensureRegistered();

        if (! areEmbeddedFontsAvailable())
            return juce::Font (juce::FontOptions (height, juce::Font::plain));

        // withPointHeight, not the height constructor: the two differ, and
        // mixing them is how a design's type scale stops matching itself.
        return juce::Font (juce::FontOptions (juce::String (family), style, height)
                               .withFallbacks ({ juce::String (fallback) }));
    }
}

void ensureRegistered()
{
    embedded();
}

bool areEmbeddedFontsAvailable()
{
    return ! embedded().faces.empty();
}

juce::Font condensed (float height, const juce::String& style)
{
    return make (Family::condensed, Family::cyrCondensed, height, style);
}

juce::Font semiCondensed (float height, const juce::String& style)
{
    return make (Family::semiCondensed, Family::cyrSemiCondensed, height, style);
}

juce::Font body (float height, const juce::String& style)
{
    return make (Family::body, Family::cyrBody, height, style);
}

} // namespace AbcTrainFonts
