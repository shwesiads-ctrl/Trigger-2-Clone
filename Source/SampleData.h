#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// One loaded sample. Shared between the message thread (loading) and the audio thread (playback).
struct SampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::File file;
    juce::String name;
    const float* chL = nullptr;
    const float* chR = nullptr;
    int length = 0;

    void finalise()
    {
        length = audio.getNumSamples();
        chL = audio.getReadPointer (0);
        chR = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : chL;
    }
};
