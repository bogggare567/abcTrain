#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// The shelf, in two layers (ADR 035).
//
// **Milestones** across the top: a dozen medals, each with a drawing of
// what it is about - an EQ bell, a compressor's knee, a decaying tail -
// because a milestone is a claim about hearing and deserves to look like
// its subject. Locked ones are shown dim with an arc of how far along
// they are: the unearned row is a map of what there is to learn.
//
// **Stamps** underneath: a grid of small squares for what you did - fifty
// rounds, ten in a row, a week. Many, cheap, and meant to be collected
// often; lit when earned, a hairline when not.
//
// It used to be one list of 24 rows, every one the same shape, where an
// exercise icon in a thin metal ring was the only difference between
// bronze and gold.
class AchievementsScreenComponent : public juce::Component,
                                     private juce::Timer
{
public:
    // What a medal's drawing depicts. The nine exercises in GameManager
    // order, then the three whole-ladder ones.
    enum class Art
    {
        eq, compression, reverb, pan, delay, distortion, width, gain, range,
        allFive, allNine, month
    };

    struct Entry
    {
        juce::String name;
        juce::String description;
        juce::Colour tint;             // family colour for a milestone, metal for a stamp
        bool earned = false;
        float progress = 0.0f;

        bool milestone = false;
        Art art = Art::eq;             // milestones
        AppIcons::Icon icon = AppIcons::Icon::award;   // stamps
    };

    AchievementsScreenComponent();
    ~AchievementsScreenComponent() override;

    void setEntries (std::vector<Entry>);

    // Title, the one-line count, the two section names.
    void setStrings (juce::String title, juce::String subtitle,
                     juce::String milestones, juce::String stamps);

    std::function<void()> onClosed;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // The drawing inside a medal, exposed so the earned-toast and the
    // snapshot tool can use the same one.
    static void drawArt (juce::Graphics&, Art, juce::Rectangle<float>, juce::Colour);

private:
    void timerCallback() override;

    juce::Rectangle<int> cardBounds() const;
    void layout();
    void paintMedal (juce::Graphics&, const Entry&, juce::Rectangle<float>, float hover);
    void paintStamp (juce::Graphics&, const Entry&, juce::Rectangle<float>, float hover);
    void paintSectionCaption (juce::Graphics&, const juce::String&, juce::Rectangle<float>);

    std::vector<Entry> entries;
    std::vector<juce::Rectangle<float>> cells;   // content space, before scroll
    std::vector<float> hoverAmounts;
    int hovered = -1;

    juce::Rectangle<float> milestoneCaption, stampCaption;
    float contentHeight = 0.0f;
    float scrollOffset = 0.0f;
    float maxScroll = 0.0f;

    juce::String titleText, subtitleText, milestonesText, stampsText;
    juce::TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AchievementsScreenComponent)
};
