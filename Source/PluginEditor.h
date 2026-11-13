#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI.h"

class BTriggerEditor : public juce::AudioProcessorEditor,
                       public juce::DragAndDropContainer
{
public:
    explicit BTriggerEditor (BTriggerProcessor&);
    ~BTriggerEditor() override;

    void resized() override;

private:
    class Content;

    BTriggerProcessor& proc;
    BTUI::BLook look;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BTriggerEditor)
};
