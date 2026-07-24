#include "LessonController.h"

LessonController::LessonController (juce::AudioProcessorValueTreeState& stateToControl, MicroLesson lessonToPlay)
    : apvts (stateToControl), lesson (std::move (lessonToPlay))
{
    setOpaque (true);

    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    progressLabel.setJustificationType (juce::Justification::centred);
    progressLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    addAndMakeVisible (progressLabel);

    stepTextLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (stepTextLabel);

    previousButton.onClick = [this]
    {
        if (lesson.previousStep())
            applyCurrentStep();
    };
    addAndMakeVisible (previousButton);

    nextButton.onClick = [this]
    {
        if (lesson.isOnLastStep())
            closeLesson();
        else if (lesson.nextStep())
            applyCurrentStep();
    };
    addAndMakeVisible (nextButton);

    closeButton.onClick = [this] { closeLesson(); };
    addAndMakeVisible (closeButton);
}

void LessonController::showAndStart()
{
    lesson.start();
    applyCurrentStep();
    setVisible (true);
}

void LessonController::closeLesson()
{
    lesson.stop();
    setVisible (false);

    if (onClosed != nullptr)
        onClosed();
}

void LessonController::applyCurrentStep()
{
    const auto& step = lesson.getCurrentStep();

    for (const auto& targetParam : step.targetParameters)
        if (auto* param = apvts.getParameter (targetParam.first))
            param->setValueNotifyingHost (param->convertTo0to1 (targetParam.second));

    stepTextLabel.setText (step.explanationText, juce::dontSendNotification);
    progressLabel.setText ("Step " + juce::String (lesson.getCurrentStepIndex() + 1) + " / " + juce::String (lesson.getNumSteps()),
                            juce::dontSendNotification);
    nextButton.setButtonText (lesson.isOnLastStep() ? "Finish" : "Next >");
    previousButton.setEnabled (! lesson.isOnFirstStep());
}

void LessonController::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
    g.setColour (juce::Colour (0xff5b9bd5).withAlpha (0.6f));
    g.drawRect (getLocalBounds(), 2);
}

void LessonController::resized()
{
    auto area = getLocalBounds().reduced (12);

    titleLabel.setBounds (area.removeFromTop (24));
    progressLabel.setBounds (area.removeFromTop (16));
    area.removeFromTop (4);

    auto bottomRow = area.removeFromBottom (32);
    stepTextLabel.setBounds (area);

    previousButton.setBounds (bottomRow.removeFromLeft (90));
    closeButton.setBounds (bottomRow.removeFromRight (90));
    nextButton.setBounds (bottomRow.reduced (8, 0));
}
