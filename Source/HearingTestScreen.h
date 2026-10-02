#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/i18n/LocalisationManager.h"
#include "shared/audio/HearingProfile.h"
#include "LearnerEQ/Source/SpectrumAnalyser.h"
#include "AudiometryProcedure.h"
#include <functional>
#include <memory>

// The hearing calibration page (ADR 051): who and which headphones, the
// test, the result. Reached from Settings -> Hearing and from the offer
// strip before an exercise; the top bar stays, and leaving by it stops
// the test.
//
// The test view is kept bare on purpose - which ear, which frequency,
// "heard" or "not heard", a pause. Anything else on screen while
// somebody listens for a sound at the edge of hearing is a distraction
// the threshold pays for. Answers open only once the presentation is
// over: a button pressed during the silence before the pulses would be a
// guess, and the test cannot tell a guess from a hearing.
//
// The result shows the thresholds as an audiogram does (left ear x in
// blue, right ear o in red), the compensation as Learner EQ draws a curve
// (it *is* Learner EQ's display), and how much to make up. Nothing is
// saved until "Save pair".
class HearingTestScreen : public juce::Component,
                          private juce::Timer
{
public:
    struct Host
    {
        std::function<void (bool)> setTestActive;    // the probe replaces all other sound
        std::function<void (int channel, float freqHz, float levelDb, double delaySeconds, bool silent)> present;
        std::function<void()> stopTone;
        std::function<void (const HearingProfile&)> save;
        std::function<void()> leave;                 // back to Settings
    };

    HearingTestScreen (LocalisationManager&, HearingProfileStore&, Host);
    ~HearingTestScreen() override;

    enum class View { setup, test, result };
    View getView() const noexcept { return view; }

    // The form, with the names remembered from last time.
    void openSetup();

    // Stops the tone and the test without saving: the page is being left.
    void abort();

    bool isTesting() const noexcept { return view == View::test; }

    void refreshText();

    void paint (juce::Graphics&) override;
    void resized() override;

    // ---- tools/EditorSnapshots and tools/ClickMap ----
    void showTestForSnapshot();
    void showResultForSnapshot (const HearingProfile&);

    // A plausible result: a right ear 8-14 dB down from 6 kHz up, the 1 kHz
    // retest within 2 dB. For the snapshot tools.
    static HearingProfile exampleProfile();

    // The one-line summary of a result, public for the tests.
    static juce::String summaryOf (const HearingProfile&, const LocalisationManager&);
    static juce::String reliabilityOf (const HearingProfile&, const LocalisationManager&);

private:
    void timerCallback() override;

    void startTest();
    void presentCurrent();
    void answer (bool heard);
    void togglePause();
    void finish();
    void showView (View);
    void refreshButtons();
    void refreshCompensationCurve();

    juce::String t (const juce::String& key) const { return localisation.getText (key); }
    static juce::String frequencyText (float hz, const LocalisationManager&);
    juce::String pairName() const;

    void paintSetup (juce::Graphics&);
    void paintTest (juce::Graphics&);
    void paintResult (juce::Graphics&);
    void paintThresholdChart (juce::Graphics&, juce::Rectangle<float>) const;

    juce::Rectangle<int> column (int width) const;

    LocalisationManager& localisation;
    HearingProfileStore& store;
    Host host;
    View view = View::setup;

    // setup
    juce::TextEditor personEditor, headphonesEditor;
    juce::TextButton startButton, cancelButton;
    juce::Rectangle<int> personLabelArea, headphonesLabelArea, introArea, timeArea;

    // test
    std::unique_ptr<AudiometryProcedure> procedure;
    juce::TextButton heardButton, notHeardButton, pauseButton;
    bool answering = false, paused = false;
    double windowEndsMs = 0.0;
    juce::Random pauseRandom;
    juce::String person, headphones;
    juce::Rectangle<int> progressArea, earArea, frequencyArea, promptArea, noteArea;

    // result
    HearingProfile draft;
    SpectrumAnalyserComponent compensationCurve;
    juce::Slider amountSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton saveButton, retakeButton;
    juce::Rectangle<int> summaryArea, thresholdArea, thresholdCaption, curveCaption,
                         reliabilityArea, amountLabelArea, halfGainArea, disclaimerArea;

    juce::Rectangle<int> titleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HearingTestScreen)
};
