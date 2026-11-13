#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <deque>
#include "Parameters.h"
#include "Effects.h"
#include "SampleData.h"

class BTriggerProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kLayers = BT::kMaxLayers;   // layer slots; the user adds and removes them
    static constexpr int kVoices = 8;
    static constexpr int kCols   = 400;   // waveform columns, covering about 2 seconds

    BTriggerProcessor();
    ~BTriggerProcessor() override = default;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "B Trigger"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- shared with the editor
    juce::AudioProcessorValueTreeState apvts;

    static juce::File libraryRoot();
    static float thresholdDb (float sensitivity) { return -2.0f - sensitivity * 0.58f; }

    // Message-thread sample handling
    bool loadLayerFile (int layer, const juce::File& file);
    bool loadFactoryFile (int layer, const juce::String& fileName);
    void clearLayer (int layer);
    bool layerHasSample (int layer) const { return layerPaths[layer].isNotEmpty(); }
    juce::String layerDisplayName (int layer) const;   // custom name, or "Layer N"
    juce::String layerFileName (int layer) const;
    juce::String layerPath (int layer) const { return layerPaths[layer]; }
    void setLayerName (int layer, const juce::String& name);
    juce::String layerCustomName (int layer) const { return layerNames[layer]; }

    // Layers can be added and removed (at least one stays). A removed slot is reset and hidden.
    bool isLayerActive (int layer) const { return layerActive[layer].load(); }
    int  activeLayerCount() const;
    int  layerPosition (int layer) const;          // 1-based place among the active layers
    int  addLayer();                               // returns the new slot, or -1 when full
    bool removeLayer (int layer);
    void ensureLayerActive (int layer);
    std::atomic<int> structureRevision { 0 };

    // ---- top bar: A/B snapshots, reset, Basic / Advanced page
    std::atomic<bool> advancedPage { false };
    int currentAB() const { return abSlot; }
    void selectAB (int slot);        // 0 = A, 1 = B. The first visit to B starts as a copy of A.
    void copyToOtherAB();            // copies the current settings into the other slot
    void resetAll();                 // everything back to the starting sound

    void previewFile (const juce::File& file);
    void manualTrigger (float velocity) { manualVel.store (velocity); }

    std::atomic<int> layerRevision { 0 };

    // Metering and waveform data, written on the audio thread, read by the editor
    std::atomic<float> inPeak[2], outPeak[2];
    std::atomic<float> wavePeaks[kCols];
    std::atomic<unsigned char> waveMarks[kCols];
    std::atomic<int> waveHead { 0 };

private:
    struct Voice
    {
        SampleData::Ptr data;
        double pos = 0.0, ratio = 1.0;
        float gL = 0.0f, gR = 0.0f;
        bool active = false;
        juce::uint64 stamp = 0;
    };

    SampleData::Ptr readSample (const juce::File& file);
    void loadDefaultLayers();
    void refreshSamples();
    void updateLayerCache();
    void fireVoices (float peak);

    juce::AudioFormatManager formatManager;

    // samples
    juce::SpinLock sampleLock;
    SampleData::Ptr layerSample[kLayers];
    SampleData::Ptr activeSample[kLayers];
    std::deque<SampleData::Ptr> retired;     // keeps replaced samples alive past the audio thread
    juce::String layerPaths[kLayers];
    juce::String layerNames[kLayers];
    std::atomic<bool> layerActive[kLayers];
    BTFx::Chain fx;
    std::atomic<float>* fxp[BT::Fx::Count] = {};
    juce::MemoryBlock abState[2];
    int abSlot = 0;
    void applyState (const void* data, int size, bool withMode);
    float mixTarget() const { return pReplace->load() > 0.5f ? 1.0f : pMix->load() * 0.01f; }
    void resetLayerParams (int layer);

    juce::SpinLock previewLock;
    SampleData::Ptr previewData, activePreview;
    std::atomic<int> previewSerial { 0 };
    int previewSeen = 0;
    double previewPos = 0.0;
    bool previewOn = false;

    // parameters
    std::atomic<float>* pInGain = nullptr;
    std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pRetrig = nullptr;
    std::atomic<float>* pDetail = nullptr;
    std::atomic<float>* pOutGain = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pReplace = nullptr;
    std::atomic<float>* pLvl[kLayers] {};
    std::atomic<float>* pPan[kLayers] {};
    std::atomic<float>* pTune[kLayers] {};
    std::atomic<float>* pMute[kLayers] {};
    std::atomic<float>* pSolo[kLayers] {};
    std::atomic<float>* pPhase[kLayers] {};

    // per-block layer settings
    bool layerEnabled[kLayers] {};
    float layerGain[kLayers] {}, balL[kLayers] {}, balR[kLayers] {}, layerRatio[kLayers] {};

    // audio-thread state
    double sampleRate = 44100.0;
    int winSamples = 66;
    float hpCoef = 0.01f, envRelease = 0.999f;
    float hpState = 0.0f, env = 0.0f, winPeak = 0.0f;
    int windowLeft = 0, cooldown = 0;
    bool armed = true;
    std::vector<float> dryDelay[2];
    int dryPos = 0;
    Voice voices[kLayers][kVoices];
    juce::uint64 stampCounter = 0;
    juce::SmoothedValue<float> smMix, smOut;
    std::atomic<float> manualVel { -1.0f };

    double samplesPerCol = 220.0, colAcc = 0.0;
    float colMax = 0.0f;
    int wpos = 0;
    bool markPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BTriggerProcessor)
};
