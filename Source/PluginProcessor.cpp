#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactorySamples.h"

BTriggerProcessor::BTriggerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BTRIGGER", BT::createLayout())
{
    formatManager.registerBasicFormats();

    for (int c = 0; c < 2; ++c)
    {
        inPeak[c].store (0.0f);
        outPeak[c].store (0.0f);
    }
    for (int i = 0; i < kCols; ++i)
    {
        wavePeaks[i].store (0.0f);
        waveMarks[i].store (0);
    }

    pInGain  = apvts.getRawParameterValue ("inGain");
    pSens    = apvts.getRawParameterValue ("sens");
    pRetrig  = apvts.getRawParameterValue ("retrig");
    pDetail  = apvts.getRawParameterValue ("detail");
    pOutGain = apvts.getRawParameterValue ("outGain");
    pMix     = apvts.getRawParameterValue ("mix");
    pReplace = apvts.getRawParameterValue ("replace");
    for (int l = 0; l < kLayers; ++l)
        layerActive[l].store (l < 4);
    for (int l = 0; l < kLayers; ++l)
    {
        pLvl[l]   = apvts.getRawParameterValue (BT::lvlId (l));
        pPan[l]   = apvts.getRawParameterValue (BT::panId (l));
        pTune[l]  = apvts.getRawParameterValue (BT::tuneId (l));
        pMute[l]  = apvts.getRawParameterValue (BT::muteId (l));
        pSolo[l]  = apvts.getRawParameterValue (BT::soloId (l));
        pPhase[l] = apvts.getRawParameterValue (BT::phaseId (l));
    }

    FactorySamples::install (libraryRoot());
    loadDefaultLayers();
}

juce::File BTriggerProcessor::libraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("B Trigger");
}

bool BTriggerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

juce::AudioProcessorEditor* BTriggerProcessor::createEditor() { return new BTriggerEditor (*this); }

// ------------------------------------------------------------------ samples

SampleData::Ptr BTriggerProcessor::readSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    auto* data = new SampleData();
    SampleData::Ptr result (data);
    data->audio.setSize (juce::jmin (2, (int) reader->numChannels), length);
    reader->read (&data->audio, 0, length, 0, true, true);
    data->sampleRate = reader->sampleRate;
    data->file = file;
    data->name = file.getFileNameWithoutExtension();
    data->finalise();
    return result;
}

bool BTriggerProcessor::loadLayerFile (int layer, const juce::File& file)
{
    if (layer < 0 || layer >= kLayers)
        return false;

    auto data = readSample (file);
    if (data == nullptr)
        return false;

    SampleData::Ptr old;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        old = layerSample[layer];
        layerSample[layer] = data;
    }
    if (old != nullptr)
    {
        retired.push_back (old);
        while (retired.size() > 16)
            retired.pop_front();
    }
    layerPaths[layer] = file.getFullPathName();
    ++layerRevision;
    return true;
}

bool BTriggerProcessor::loadFactoryFile (int layer, const juce::String& fileName)
{
    const auto dir = FactorySamples::factoryDir (libraryRoot());
    juce::Array<juce::File> found;
    dir.findChildFiles (found, juce::File::findFiles, true, fileName);
    return ! found.isEmpty() && loadLayerFile (layer, found.getFirst());
}

void BTriggerProcessor::clearLayer (int layer)
{
    if (layer < 0 || layer >= kLayers)
        return;

    SampleData::Ptr old;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        old = layerSample[layer];
        layerSample[layer] = nullptr;
    }
    if (old != nullptr)
        retired.push_back (old);
    layerPaths[layer] = {};
    ++layerRevision;
}

juce::String BTriggerProcessor::layerDisplayName (int layer) const
{
    return layerNames[layer].isNotEmpty() ? layerNames[layer] : "Layer " + juce::String (layerPosition (layer));
}

void BTriggerProcessor::setLayerName (int layer, const juce::String& name)
{
    if (layer >= 0 && layer < kLayers)
        layerNames[layer] = name.trim().substring (0, 32);
}

int BTriggerProcessor::activeLayerCount() const
{
    int n = 0;
    for (int l = 0; l < kLayers; ++l)
        n += layerActive[l].load() ? 1 : 0;
    return n;
}

int BTriggerProcessor::layerPosition (int layer) const
{
    int pos = 0;
    for (int l = 0; l <= layer && l < kLayers; ++l)
        pos += layerActive[l].load() ? 1 : 0;
    return juce::jmax (1, pos);
}

void BTriggerProcessor::resetLayerParams (int layer)
{
    const juce::String ids[] = { BT::lvlId (layer), BT::panId (layer), BT::tuneId (layer),
                                 BT::muteId (layer), BT::soloId (layer), BT::phaseId (layer) };
    for (const auto& id : ids)
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->getDefaultValue());
}

int BTriggerProcessor::addLayer()
{
    for (int l = 0; l < kLayers; ++l)
        if (! layerActive[l].load())
        {
            clearLayer (l);
            layerNames[l] = {};
            resetLayerParams (l);
            if (auto* prm = apvts.getParameter (BT::lvlId (l)))
                prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));   // new layers start at 0 dB
            layerActive[l].store (true);
            ++structureRevision;
            return l;
        }
    return -1;
}

bool BTriggerProcessor::removeLayer (int layer)
{
    if (layer < 0 || layer >= kLayers || ! layerActive[layer].load() || activeLayerCount() <= 1)
        return false;
    layerActive[layer].store (false);
    clearLayer (layer);
    layerNames[layer] = {};
    resetLayerParams (layer);
    ++structureRevision;
    return true;
}

void BTriggerProcessor::ensureLayerActive (int layer)
{
    if (layer >= 0 && layer < kLayers && ! layerActive[layer].load())
    {
        layerActive[layer].store (true);
        ++structureRevision;
    }
}

juce::String BTriggerProcessor::layerFileName (int layer) const
{
    return layerPaths[layer].isEmpty() ? juce::String() : juce::File (layerPaths[layer]).getFileName();
}

void BTriggerProcessor::loadDefaultLayers()
{
    loadFactoryFile (0, "Kick_Close.wav");
    loadFactoryFile (1, "Kick_Body.wav");
    loadFactoryFile (2, "Kick_Room.wav");
}

void BTriggerProcessor::previewFile (const juce::File& file)
{
    auto data = readSample (file);
    if (data == nullptr)
        return;

    SampleData::Ptr old;
    {
        const juce::SpinLock::ScopedLockType sl (previewLock);
        old = previewData;
        previewData = data;
    }
    if (old != nullptr)
    {
        retired.push_back (old);
        while (retired.size() > 16)
            retired.pop_front();
    }
    ++previewSerial;
}

// ------------------------------------------------------------------ state

void BTriggerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int l = 0; l < kLayers; ++l)
    {
        state.setProperty ("layer" + juce::String (l), layerPaths[l], nullptr);
        state.setProperty ("name" + juce::String (l), layerNames[l], nullptr);
        state.setProperty ("active" + juce::String (l), layerActive[l].load(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BTriggerProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    for (int l = 0; l < kLayers; ++l)
    {
        layerNames[l] = tree.getProperty ("name" + juce::String (l)).toString();
        layerActive[l].store (tree.hasProperty ("active" + juce::String (l)) ? (bool) tree.getProperty ("active" + juce::String (l)) : l < 4);
        const juce::String path = tree.getProperty ("layer" + juce::String (l)).toString();
        if (path.isEmpty())
            clearLayer (l);
        else if (juce::File (path).existsAsFile())
            loadLayerFile (l, juce::File (path));
    }
    ++structureRevision;
}

// ------------------------------------------------------------------ audio

void BTriggerProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    winSamples = juce::jmax (1, (int) std::round (0.0015 * sr));
    setLatencySamples (winSamples);

    hpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 80.0f / (float) sr);
    envRelease = std::exp (-1.0f / (0.03f * (float) sr));

    for (auto& d : dryDelay)
        d.assign ((size_t) winSamples, 0.0f);
    dryPos = 0;

    hpState = env = winPeak = 0.0f;
    windowLeft = cooldown = 0;
    armed = true;
    for (auto& layer : voices)
        for (auto& v : layer)
        {
            v.active = false;
            v.data = nullptr;
        }

    samplesPerCol = sr * 2.0 / (double) kCols;
    colAcc = 0.0;
    colMax = 0.0f;
    wpos = 0;
    markPending = false;

    smMix.reset (sr, 0.02);
    smOut.reset (sr, 0.02);
    smMix.setCurrentAndTargetValue (mixTarget());
    smOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    previewOn = false;
    previewSeen = previewSerial.load();
}

void BTriggerProcessor::refreshSamples()
{
    {
        const juce::SpinLock::ScopedTryLockType tl (sampleLock);
        if (tl.isLocked())
            for (int l = 0; l < kLayers; ++l)
                activeSample[l] = layerSample[l];
    }

    const int serial = previewSerial.load();
    if (serial != previewSeen)
    {
        const juce::SpinLock::ScopedTryLockType tl (previewLock);
        if (tl.isLocked())
        {
            activePreview = previewData;
            previewPos = 0.0;
            previewOn = activePreview.get() != nullptr;
            previewSeen = serial;
        }
    }
}

void BTriggerProcessor::updateLayerCache()
{
    bool anySolo = false;
    for (int l = 0; l < kLayers; ++l)
        if (layerActive[l].load() && pSolo[l]->load() > 0.5f)
            anySolo = true;

    for (int l = 0; l < kLayers; ++l)
    {
        const float lvl = pLvl[l]->load();
        const float pan = pPan[l]->load() / 100.0f;
        const bool mute = pMute[l]->load() > 0.5f;
        const bool solo = pSolo[l]->load() > 0.5f;

        layerGain[l] = (lvl <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain (lvl))
                       * (pPhase[l]->load() > 0.5f ? -1.0f : 1.0f);
        balL[l] = pan > 0.0f ? 1.0f - pan : 1.0f;
        balR[l] = pan < 0.0f ? 1.0f + pan : 1.0f;
        layerRatio[l] = std::pow (2.0f, pTune[l]->load() / 12.0f);
        layerEnabled[l] = layerActive[l].load() && ! mute && (! anySolo || solo) && activeSample[l].get() != nullptr;
    }
}

void BTriggerProcessor::fireVoices (float peak)
{
    const float vel = std::pow (juce::jlimit (0.001f, 1.0f, peak), 0.6f);

    for (int l = 0; l < kLayers; ++l)
    {
        if (! layerEnabled[l])
            continue;

        Voice* chosen = nullptr;
        for (auto& v : voices[l])
            if (! v.active)
            {
                chosen = &v;
                break;
            }
        if (chosen == nullptr)
        {
            chosen = &voices[l][0];
            for (auto& v : voices[l])
                if (v.stamp < chosen->stamp)
                    chosen = &v;
        }

        chosen->data = activeSample[l];
        chosen->pos = 0.0;
        chosen->ratio = (double) layerRatio[l] * chosen->data->sampleRate / sampleRate;
        chosen->gL = layerGain[l] * vel * balL[l];
        chosen->gR = layerGain[l] * vel * balR[l];
        chosen->stamp = ++stampCounter;
        chosen->active = true;
    }
}

void BTriggerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nch == 0 || n == 0)
        return;

    refreshSamples();
    updateLayerCache();

    const float inGain = juce::Decibels::decibelsToGain (pInGain->load());
    const float thrLin = juce::Decibels::decibelsToGain (thresholdDb (pSens->load()));
    const int retrigSamples = (int) (pRetrig->load() * 0.001 * sampleRate);
    const float rearm = juce::jmap (pDetail->load(), 0.0f, 100.0f, 0.25f, 0.85f);
    // How far a new peak must rise above the previous hit's decaying tail to count as a new hit.
    // Higher detail picks up smaller rises (rolls, ghost notes).
    const float rise = juce::jmap (pDetail->load(), 0.0f, 100.0f, 3.0f, 1.6f);
    smMix.setTargetValue (mixTarget());
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));

    float* ch[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };

    float mInL = 0.0f, mInR = 0.0f, mOutL = 0.0f, mOutR = 0.0f;

    const float manual = manualVel.exchange (-1.0f);
    if (manual >= 0.0f)
    {
        fireVoices (manual);
        markPending = true;
    }

    for (int i = 0; i < n; ++i)
    {
        const float inL = ch[0][i];
        const float inR = ch[1] != nullptr ? ch[1][i] : inL;

        // ---- transient detection
        const float mono = 0.5f * (inL + inR) * inGain;
        hpState += hpCoef * (mono - hpState);
        const float det = std::abs (mono - hpState);
        const float envPrev = env;
        env = juce::jmax (det, env * envRelease);

        mInL = juce::jmax (mInL, std::abs (inL * inGain));
        mInR = juce::jmax (mInR, std::abs (inR * inGain));
        colMax = juce::jmax (colMax, std::abs (mono));

        if (cooldown > 0)
            --cooldown;

        if (windowLeft > 0)
        {
            winPeak = juce::jmax (winPeak, det);
            if (--windowLeft == 0)
            {
                fireVoices (winPeak);
                markPending = true;
                armed = false;
                cooldown = retrigSamples;
            }
        }
        else if (cooldown <= 0 && det >= thrLin && (armed || det >= envPrev * rise))
        {
            windowLeft = winSamples;
            winPeak = det;
        }
        else if (! armed && cooldown <= 0 && env < thrLin * rearm)
        {
            armed = true;
        }

        // ---- dry path, delayed to line up with the triggered samples
        float dryL = inL, dryR = inR;
        {
            auto& dl0 = dryDelay[0];
            dryL = dl0[(size_t) dryPos];
            dl0[(size_t) dryPos] = inL;
            if (ch[1] != nullptr)
            {
                auto& dl1 = dryDelay[1];
                dryR = dl1[(size_t) dryPos];
                dl1[(size_t) dryPos] = inR;
            }
            else
                dryR = dryL;
            if (++dryPos >= winSamples)
                dryPos = 0;
        }

        // ---- sample voices
        float wetL = 0.0f, wetR = 0.0f;
        for (int l = 0; l < kLayers; ++l)
        {
            for (auto& v : voices[l])
            {
                if (! v.active)
                    continue;

                const SampleData* d = v.data.get();
                const int i0 = (int) v.pos;
                if (d == nullptr || i0 >= d->length - 1)
                {
                    v.active = false;
                    continue;
                }

                const float frac = (float) (v.pos - (double) i0);
                const float sL = d->chL[i0] + frac * (d->chL[i0 + 1] - d->chL[i0]);
                const float sR = d->chR[i0] + frac * (d->chR[i0 + 1] - d->chR[i0]);
                wetL += sL * v.gL;
                wetR += sR * v.gR;
                v.pos += v.ratio;
            }
        }

        // ---- browser preview (not affected by mix)
        float pvL = 0.0f, pvR = 0.0f;
        if (previewOn)
        {
            const SampleData* d = activePreview.get();
            const int i0 = (int) previewPos;
            if (d == nullptr || i0 >= d->length - 1)
                previewOn = false;
            else
            {
                const float frac = (float) (previewPos - (double) i0);
                pvL = (d->chL[i0] + frac * (d->chL[i0 + 1] - d->chL[i0])) * 0.8f;
                pvR = (d->chR[i0] + frac * (d->chR[i0 + 1] - d->chR[i0])) * 0.8f;
                previewPos += d->sampleRate / sampleRate;
            }
        }

        const float mix = smMix.getNextValue();
        const float og = smOut.getNextValue();
        const float outL = (dryL * (1.0f - mix) + wetL * mix) * og + pvL;
        const float outR = (dryR * (1.0f - mix) + wetR * mix) * og + pvR;

        ch[0][i] = outL;
        if (ch[1] != nullptr)
            ch[1][i] = outR;

        mOutL = juce::jmax (mOutL, std::abs (outL));
        mOutR = juce::jmax (mOutR, std::abs (outR));

        // ---- waveform columns for the display
        colAcc += 1.0;
        if (colAcc >= samplesPerCol)
        {
            colAcc -= samplesPerCol;
            wavePeaks[wpos].store (colMax);
            waveMarks[wpos].store (markPending ? 1 : 0);
            markPending = false;
            colMax = 0.0f;
            wpos = (wpos + 1) % kCols;
            waveHead.store (wpos);
        }
    }

    const float decay = std::exp (-(float) n / (0.25f * (float) sampleRate));
    inPeak[0].store (juce::jmax (mInL, inPeak[0].load() * decay));
    inPeak[1].store (juce::jmax (mInR, inPeak[1].load() * decay));
    outPeak[0].store (juce::jmax (mOutL, outPeak[0].load() * decay));
    outPeak[1].store (juce::jmax (mOutR, outPeak[1].load() * decay));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BTriggerProcessor(); }
