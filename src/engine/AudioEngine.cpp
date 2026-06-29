#include "engine/AudioEngine.h"

#include <cmath>

//==============================================================================
// EngineAudioSource
//==============================================================================
EngineAudioSource::EngineAudioSource (juce::MidiKeyboardState& keyStateToUse)
    : keyboardState (keyStateToUse)
{
    for (auto& sig : publishedSig)
        sig.store (-1);
}

void EngineAudioSource::republishMap()
{
    for (int s = 0; s < med::MidiMap::numSlots; ++s)
    {
        if (midiMap.slotHasBinding (s))
        {
            const auto c = midiMap.slotBinding (s);
            publishedSig[(size_t) s].store (c.type == med::CtrlType::CC ? 1000 + c.number
                                                                        : c.number);
        }
        else
        {
            publishedSig[(size_t) s].store (-1);
        }
    }
}

void EngineAudioSource::resolveMapping (juce::MidiBuffer& live)
{
    if (const int clearSlot = clearBindingSlot.exchange (-1); clearSlot >= 0)
    {
        midiMap.clearSlot (clearSlot);
        republishMap();
    }

    const int learnSlot = learnArmedSlot.load();
    bool      learnDone = false;

    juce::MidiBuffer filtered;
    for (const auto metadata : live)
    {
        const auto msg = metadata.getMessage();
        const int  sp  = metadata.samplePosition;

        med::Ctrl ctrl;
        bool mappable   = false;
        bool activation = false;
        int  ccValue    = 0;

        if (msg.isNoteOnOrOff())
        {
            ctrl       = { med::CtrlType::Note, msg.getNoteNumber() };
            mappable   = true;
            activation = msg.isNoteOn();
        }
        else if (msg.isController())
        {
            ctrl       = { med::CtrlType::CC, msg.getControllerNumber() };
            mappable   = true;
            ccValue    = msg.getControllerValue();
            activation = ccValue >= 64;
        }

        // Moniteur : on publie le dernier contrôle reçu (note-on / CC).
        if (msg.isNoteOn())
        {
            lastMidiCode.store (msg.getNoteNumber());
            lastMidiValue.store ((int) msg.getVelocity());
        }
        else if (msg.isController())
        {
            lastMidiCode.store (1000 + msg.getControllerNumber());
            lastMidiValue.store (ccValue);
        }

        bool consumed = false;
        if (mappable)
        {
            if (learnSlot >= 0 && ! learnDone
                && (msg.isNoteOn() || msg.isController())) // pas sur un note-off
            {
                midiMap.bind (learnSlot, ctrl);
                republishMap();
                learnArmedSlot.store (-1);
                learnDone = true;
                consumed  = true;
            }
            else if (const int slot = midiMap.findSlot (ctrl); slot >= 0)
            {
                consumed = true;
                const int a = juce::jlimit (0, numTracks - 1, activeTrack.load());

                if (slot == 6) // Volume (piste active) — continu
                {
                    if (msg.isController())
                        tracks[(size_t) a].setVolume ((float) ccValue / 127.0f);
                }
                else if (slot == 7) // Sélecteur de piste (potard) — continu
                {
                    if (msg.isController())
                        activeTrack.store (juce::jlimit (0, numTracks - 1,
                                                         (ccValue * numTracks) / 128));
                }
                else if (slot == 8) // Mesures (piste active) — continu
                {
                    if (msg.isController())
                    {
                        static const int barsTable[4] = { 1, 2, 4, 8 };
                        const int idx = juce::jlimit (0, 3, (ccValue * 4) / 128);
                        tracks[(size_t) a].setBars (barsTable[idx]);
                    }
                }
                else if (slot == 9) // BPM (tempo) — continu
                {
                    if (msg.isController())
                        requestedBpm.store (40.0 + (ccValue / 127.0) * 200.0);
                }
                else if (slot >= 10 && slot < 10 + numTracks) // Volume piste N — continu
                {
                    if (msg.isController())
                        tracks[(size_t) (slot - 10)].setVolume ((float) ccValue / 127.0f);
                }
                else if (activation)
                {
                    switch (slot)
                    {
                        case 0:  requestedPlaying.store (! requestedPlaying.load()); break;
                        case 1:  recordPressed.store (true); break;
                        case 2:  clearPressed.store (true); break;
                        case 3:  undoPressed.store (true); break;
                        case 4:  redoPressed.store (true); break;
                        case 5:  metronomeEnabled.store (! metronomeEnabled.load()); break;
                        case 18: openEditorRequested.store (true); break;
                        default: break;
                    }
                }
            }
        }

        if (! consumed)
            filtered.addEvent (msg, sp);
    }

    live.swapWith (filtered);
}

void EngineAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    midiCollector.reset (sampleRate);

    transport.prepare (sampleRate);
    transport.setTimeSignature (numerator, 4);
    metronome.prepare (sampleRate);

    for (auto& track : tracks)
        track.prepare (sampleRate, samplesPerBlockExpected);
}

void EngineAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    const int numSamples  = bufferToFill.numSamples;
    const int startSample = bufferToFill.startSample;
    const int numCh       = bufferToFill.buffer->getNumChannels();
    bufferToFill.clearActiveBufferRegion();

    const int active = juce::jlimit (0, numTracks - 1, activeTrack.load());

    // 1) MIDI live (clavier physique + écran), positions 0-based dans le bloc.
    juce::MidiBuffer liveMidi;
    midiCollector.removeNextBlockOfMessages (liveMidi, numSamples);
    keyboardState.processNextMidiBuffer (liveMidi, 0, numSamples, true);

    // 1b) Mapping : apprend / déclenche les actions, retire les messages consommés.
    resolveMapping (liveMidi);

    // 2) Commandes UI (et celles posées par le mapping ci-dessus).
    const bool clearCmd  = clearPressed.exchange (false);
    const bool recordCmd = recordPressed.exchange (false);
    const bool undoCmd   = undoPressed.exchange (false);
    const bool redoCmd   = redoPressed.exchange (false);

    // 3) Transport : tempo + fronts lecture/arrêt.
    transport.setTempo (requestedBpm.load());
    const bool wantPlay = requestedPlaying.load();
    if (wantPlay && ! prevPlaying)
    {
        transport.rewind();
        transport.start();
        beatCounter = 0;
        nextBeatPos = 0.0;
        metronome.reset();
        for (auto& track : tracks)        // coupe tout son en cours avant le saut
            track.requestAllNotesOff();
    }
    else if (! wantPlay && prevPlaying)
    {
        for (auto& track : tracks)
            track.onTransportStopped();
        transport.stop();
        transport.rewind();
        metronome.reset();
    }
    prevPlaying = wantPlay;

    // 4) Effacer / Annuler / Enregistrer sur la piste active.
    if (clearCmd)
        tracks[(size_t) active].clearLoop();

    if (undoCmd)
        tracks[(size_t) active].undo();

    if (redoCmd)
        tracks[(size_t) active].redo();

    if (recordCmd)
    {
        auto& at = tracks[(size_t) active];
        if (at.getLoopState() == (int) Track::LoopState::Recording)
        {
            at.stopRecording();
        }
        else
        {
            // Démarrage immédiat : on réaligne toute la session sur la mesure 1.
            transport.rewind();
            transport.start();
            requestedPlaying.store (true);
            prevPlaying = true;
            beatCounter = 0;
            nextBeatPos = 0.0;
            metronome.reset();
            for (auto& track : tracks)        // coupe tout son en cours avant le saut
                track.requestAllNotesOff();
            at.startRecording ((double) (at.getBars() * numerator));
        }
    }

    // 5) Géométrie temporelle du bloc.
    const double spb        = transport.samplesPerBeat();
    const double deltaBeats = spb > 0.0 ? (double) numSamples / spb : 0.0;
    const double linStart   = transport.getPositionInBeats();
    const bool   playing    = transport.isPlaying();

    // 6) Rendu de chaque piste + mixage (le MIDI live ne va qu'à la piste active).
    static const juce::MidiBuffer emptyMidi;
    for (int t = 0; t < numTracks; ++t)
    {
        auto& track = tracks[(size_t) t];
        track.renderBlock (t == active ? liveMidi : emptyMidi,
                            linStart, deltaBeats, spb, playing, numSamples);

        if (track.isMuted())
            continue;

        const auto& tb   = track.getOutput();
        const float gain = track.getVolume();
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int srcCh = juce::jmin (ch, tb.getNumChannels() - 1);
            bufferToFill.buffer->addFrom (ch, startSample, tb, srcCh, 0, numSamples, gain);
        }
    }

    // 7) Métronome (per-sample, grille incrémentale) par-dessus le mix.
    const bool metroOn  = metronomeEnabled.load();
    const double startPos = transport.getPositionSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        if (playing)
        {
            const double absPos = startPos + (double) i;
            while (absPos >= nextBeatPos)
            {
                const bool downbeat = (beatCounter % numerator == 0);
                if (metroOn)
                    metronome.trigger (downbeat);

                ++beatCounter;
                nextBeatPos += spb;
            }
        }

        const float click = metroOn ? metronome.nextSample() : 0.0f;
        if (click != 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
                bufferToFill.buffer->addSample (ch, startSample + i, click);
    }

    // 8) Avance + publication vers l'UI.
    transport.advance (numSamples);
    publishedBeats.store (transport.getPositionInBeats());
    publishedPlaying.store (transport.isPlaying());
}

//==============================================================================
// AudioEngine
//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    for (auto& input : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (input.identifier, this);

    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
}

void AudioEngine::start()
{
    auto error = deviceManager.initialise (0, 2, nullptr, true);
    jassert (error.isEmpty());
    juce::ignoreUnused (error);

    loadMapping(); // avant de démarrer l'audio (pose les associations sans concurrence)

    sourcePlayer.setSource (&source);
    deviceManager.addAudioCallback (&sourcePlayer);

    openedMidiInputs.clear();
    for (auto& input : juce::MidiInput::getAvailableDevices())
    {
        if (! deviceManager.isMidiInputDeviceEnabled (input.identifier))
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        deviceManager.addMidiInputDeviceCallback (input.identifier, this);
        openedMidiInputs.add (input.name);
    }
}

juce::String AudioEngine::loadPluginToActiveTrack (const juce::File& file)
{
    double sampleRate = 44100.0;
    int    blockSize  = 512;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize  = device->getCurrentBufferSizeSamples();
    }

    juce::String errorMessage;
    auto instance = pluginHost.createFromFile (file, sampleRate, blockSize, errorMessage);

    if (instance == nullptr)
        return errorMessage.isNotEmpty() ? errorMessage
                                         : juce::String ("Echec du chargement du plugin.");

    source.getTrack (source.getActiveTrack()).setPlugin (std::move (instance));
    return {};
}

juce::AudioPluginInstance* AudioEngine::getActivePlugin() noexcept
{
    return source.getTrack (source.getActiveTrack()).getPlugin();
}

juce::String AudioEngine::getStatusText()
{
    juce::String s;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        s << "Audio : " << device->getName()
          << "  (" << (int) device->getCurrentSampleRate() << " Hz, buffer "
          << device->getCurrentBufferSizeSamples() << " samples)\n";
    else
        s << "Audio : aucun peripherique\n";

    if (openedMidiInputs.isEmpty())
        s << "MIDI  : aucun clavier detecte";
    else
        s << "MIDI  : " << openedMidiInputs.joinIntoString (", ");

    return s;
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* /*midiSource*/,
                                             const juce::MidiMessage& message)
{
    source.getMidiCollector()->addMessageToQueue (message);
}

//==============================================================================
// Persistance du mapping (fichier global)
//==============================================================================
static juce::File getMappingFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Midi et demi");
    dir.createDirectory();
    return dir.getChildFile ("mapping.json");
}

void AudioEngine::saveMapping()
{
    auto* obj = new juce::DynamicObject();
    for (int s = 0; s < med::MidiMap::numSlots; ++s)
    {
        const int code = source.getBindingCode (s);
        if (code >= 0)
            obj->setProperty ("s" + juce::String (s), code);
    }

    getMappingFile().replaceWithText (juce::JSON::toString (juce::var (obj)));
}

void AudioEngine::loadMapping()
{
    auto file = getMappingFile();
    if (! file.existsAsFile())
        return;

    auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    for (int s = 0; s < med::MidiMap::numSlots; ++s)
    {
        auto v = obj->getProperty ("s" + juce::String (s));
        if (! v.isInt())
            continue;

        const int code = (int) v;
        med::Ctrl c;
        if (code >= 1000) { c.type = med::CtrlType::CC;   c.number = code - 1000; }
        else              { c.type = med::CtrlType::Note; c.number = code; }
        source.bindDirect (s, c);
    }
}
