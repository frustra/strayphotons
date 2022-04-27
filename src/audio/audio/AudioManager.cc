#include "AudioManager.hh"

#include "assets/AssetManager.hh"
#include "core/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"

#include <resonance-audio/resonance_audio/base/constants_and_types.h>
#include <resonance_audio_api.h>
#include <soundio/soundio.h>

namespace sp {
    AudioManager::AudioManager()
        : RegisteredThread("AudioManager", std::chrono::milliseconds(20), false), decoderQueue("AudioDecode") {
        StartThread();
    }

    bool AudioManager::ThreadInit() {
        ZoneScoped;

        soundio = soundio_create();

        int err = soundio_connect(soundio);
        Assertf(!err, "soundio_connect: %s", soundio_strerror(err));

        soundio_flush_events(soundio);

        deviceIndex = soundio_default_output_device_index(soundio);
        Assert(deviceIndex >= 0, "no audio output device found");

        device = soundio_get_output_device(soundio, deviceIndex);
        Assert(device, "failed to open sound device");

        outstream = soundio_outstream_create(device);
        outstream->format = SoundIoFormatFloat32NE;
        outstream->write_callback = AudioWriteCallback;
        outstream->userdata = this;

        err = soundio_outstream_open(outstream);
        Assertf(!err, "soundio_outstream_open: %s", soundio_strerror(err));
        Assertf(!outstream->layout_error,
            "unable to set channel layout: %s",
            soundio_strerror(outstream->layout_error));

        auto channelCount = outstream->layout.channel_count;
        Assertf(channelCount == 2, "only stereo output is supported, have %d channels", channelCount);

        framesPerBuffer = outstream->sample_rate * interval.count() / 1e9;
        Assertf(framesPerBuffer < vraudio::kMaxSupportedNumFrames, "buffer too big: %d", framesPerBuffer);
        Assertf(framesPerBuffer >= 32, "buffer too small: %d", framesPerBuffer); // FftManager::kMinFftSize

        resonance.reset(vraudio::CreateResonanceAudioApi(2, framesPerBuffer, outstream->sample_rate));

        err = soundio_outstream_start(outstream);
        Assertf(!err, "unable to start audio device: %s", soundio_strerror(err));

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            soundObserver = lock.Watch<ecs::ComponentEvent<ecs::Sounds>>();
        }
        return true;
    }

    AudioManager::~AudioManager() {
        StopThread();
        if (outstream) soundio_outstream_destroy(outstream);
        if (device) soundio_device_unref(device);
        if (soundio) soundio_destroy(soundio);
    }

    void AudioManager::Frame() {
        ZoneScoped;
        {
            ZoneScopedN("soundio_flush_events");
            soundio_flush_events(soundio);
        }
        SyncFromECS();
    }

    void AudioManager::SyncFromECS() {
        ZoneScoped;
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Sounds, ecs::Transform, ecs::Name>,
            ecs::Write<ecs::EventInput>>();

        auto head = headEntity.Get(lock);
        if (!head) head = headEntityFallback.Get(lock);
        if (head && head.Has<ecs::Transform>(lock)) {
            auto transform = head.Get<ecs::Transform>(lock);
            auto pos = transform.GetPosition();
            auto rot = transform.GetRotation();
            resonance->SetHeadPosition(pos.x, pos.y, pos.z);
            resonance->SetHeadRotation(rot.x, rot.y, rot.z, rot.w);
        }

        ecs::ComponentEvent<ecs::Sounds> compEvent;
        while (soundObserver.Poll(lock, compEvent)) {
            if (compEvent.type != Tecs::EventType::REMOVED) continue;

            auto it = soundEntityMap.find(compEvent.entity);
            if (it == soundEntityMap.end()) continue;

            for (auto id : it->second) {
                auto &state = sounds.Get(id);
                if (state.resonanceID >= 0) resonance->DestroySource(state.resonanceID);
                sounds.FreeItem(id);
            }
            soundEntityMap.erase(it);
        }

        for (auto ent : lock.EntitiesWith<ecs::Sounds>()) {
            vector<size_t> *soundIDs;

            auto &sources = ent.Get<ecs::Sounds>(lock);
            auto entSound = soundEntityMap.find(ent);

            if (entSound == soundEntityMap.end()) {
                soundIDs = &soundEntityMap[ent];
                for (auto &source : sources.sounds) {
                    auto soundID = sounds.AllocateItem();
                    soundIDs->push_back(soundID);

                    auto &state = sounds.Get(soundID);
                    state.resonanceID = -1;
                    state.loop = source.loop;
                    state.play = source.playOnLoad;
                    state.bufferOffset = 0;
                    state.audioBuffer = decoderQueue.Dispatch<nqr::AudioData>(source.file,
                        [this, file = source.file](shared_ptr<Asset> asset) {
                            if (!asset) {
                                Logf("Audio file missing: %s", file->Get()->path);
                                return shared_ptr<nqr::AudioData>();
                            }
                            auto audioBuffer = decoderCache.Load(asset.get());
                            if (!audioBuffer) {
                                audioBuffer = make_shared<nqr::AudioData>();
                                loader.Load(audioBuffer.get(), asset->extension, asset->Buffer());
                            }
                            return audioBuffer;
                        });
                }
            } else {
                soundIDs = &entSound->second;
            }

            for (size_t i = 0; i < sources.sounds.size() && i < soundIDs->size(); i++) {
                auto &source = sources.sounds[i];
                auto id = soundIDs->at(i);
                auto &state = sounds.Get(id);
                auto resonanceID = state.resonanceID;
                if (resonanceID == -1 && state.audioBuffer->Ready()) {
                    auto channelCount = state.audioBuffer->Get()->channelCount;
                    if (source.type == ecs::Sound::Type::Object) {
                        resonanceID = resonance->CreateSoundObjectSource(vraudio::kBinauralHighQuality);
                    } else if (source.type == ecs::Sound::Type::Stereo) {
                        resonanceID = resonance->CreateStereoSource(channelCount);
                    } else if (source.type == ecs::Sound::Type::Ambisonic) {
                        resonanceID = resonance->CreateAmbisonicSource(channelCount);
                    }
                    state.resonanceID = resonanceID;
                    sounds.MakeItemValid(id);
                }

                if (resonanceID != -1) {
                    resonance->SetSourceVolume(resonanceID, source.volume);

                    if (ent.Has<ecs::Transform>(lock)) {
                        auto &transform = ent.Get<ecs::Transform>(lock);
                        auto pos = transform.GetPosition();
                        auto rot = transform.GetRotation();
                        resonance->SetSourcePosition(resonanceID, pos.x, pos.y, pos.z);
                        resonance->SetSourceRotation(resonanceID, rot.x, rot.y, rot.z, rot.w);
                    }
                }
            }

            if (ent.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, ent, "/sound/play", event)) {
                    auto i = std::get_if<int>(&event.data);
                    auto soundID = soundIDs->at(i ? *i : 0);
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::PlayFromStart, soundID});
                }
                while (ecs::EventInput::Poll(lock, ent, "/sound/resume", event)) {
                    auto i = std::get_if<int>(&event.data);
                    auto soundID = soundIDs->at(i ? *i : 0);
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::Resume, soundID});
                }
                while (ecs::EventInput::Poll(lock, ent, "/sound/pause", event)) {
                    auto i = std::get_if<int>(&event.data);
                    auto soundID = soundIDs->at(i ? *i : 0);
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::Pause, soundID});
                }
                while (ecs::EventInput::Poll(lock, ent, "/sound/stop", event)) {
                    auto i = std::get_if<int>(&event.data);
                    auto soundID = soundIDs->at(i ? *i : 0);
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::Stop, soundID});
                }
            }
        }

        sounds.UpdateIndexes();

        decoderQueue.Dispatch<void>([this] {
            decoderCache.Tick(interval);
        });
    }

    const float Zeros[16] = {0};

    void AudioManager::AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax) {
        thread_local bool setThreadName = false;
        if (!setThreadName) {
            tracy::SetThreadName("AudioRender");
            setThreadName = true;
        }

        ZoneScoped;
        auto self = static_cast<AudioManager *>(outstream->userdata);

        self->soundEvents.TryPollEvents([&](const SoundEvent &event) {
            auto &sound = self->sounds.Get(event.soundID);
            switch (event.type) {
            case SoundEvent::Type::PlayFromStart:
                sound.bufferOffset = 0;
            case SoundEvent::Type::Resume:
                sound.play = true;
                break;
            case SoundEvent::Type::Stop:
                sound.bufferOffset = 0;
            case SoundEvent::Type::Pause:
                sound.play = false;
                break;
            }
        });

        struct SoundIoChannelArea *areas;
        int framesPerBuffer = self->framesPerBuffer;
        int framesToWrite = framesPerBuffer * std::max(std::min(1, frameCountMax / framesPerBuffer),
                                                  (frameCountMin + framesPerBuffer - 1) / framesPerBuffer);
        Assertf(framesToWrite > 0, "wanted %d frames, min %d max %d", framesPerBuffer, frameCountMin, frameCountMax);
        int err = soundio_outstream_begin_write(outstream, &areas, &framesToWrite);

        int channelCount = outstream->layout.channel_count;
        auto basePtr = reinterpret_cast<float *>(areas[0].ptr);

        for (int channel = 0; channel < channelCount; channel++) {
            Assert(areas[channel].step == (int)sizeof(float) * channelCount, "expected interleaved output buffer");
            Assert((float *)areas[channel].ptr == basePtr + channel, "expected interleaved output buffer");
        }

        auto floatsPerBuffer = framesPerBuffer * channelCount;
        auto outputBuffer = (float *)areas[0].ptr;
        const float *lastSample = Zeros;

        while (framesToWrite >= framesPerBuffer) {
            if (self->resonance) {
                auto validIndexPtr = self->sounds.GetValidIndexes();
                for (auto soundID : *validIndexPtr) {
                    auto &source = self->sounds.Get(soundID);
                    if (!source.play || !source.audioBuffer->Ready()) continue;

                    auto &audioBuffer = *source.audioBuffer->Get();
                    auto floatsRemaining = audioBuffer.samples.size() - source.bufferOffset;

                    auto framesRemaining = std::min((size_t)framesPerBuffer,
                        floatsRemaining / audioBuffer.channelCount);

                    self->resonance->SetInterleavedBuffer(source.resonanceID,
                        &audioBuffer.samples[source.bufferOffset],
                        audioBuffer.channelCount,
                        framesRemaining);

                    auto floatsPerSourceBuffer = framesPerBuffer * audioBuffer.channelCount;
                    source.bufferOffset += floatsPerSourceBuffer;
                    if (source.bufferOffset >= audioBuffer.samples.size()) {
                        source.bufferOffset = 0;
                        if (!source.loop) source.play = false;
                    }
                }

                ZoneScopedN("Render");
                self->resonance->FillInterleavedOutputBuffer(channelCount, framesPerBuffer, outputBuffer);
            } else {
                std::fill(outputBuffer, outputBuffer + floatsPerBuffer, 0);
            }
            outputBuffer += floatsPerBuffer;
            framesToWrite -= framesPerBuffer;
            lastSample = outputBuffer - channelCount;
        }

        while (framesToWrite > 0) {
            std::copy(lastSample, lastSample + channelCount, outputBuffer);
            outputBuffer += channelCount;
            framesToWrite--;
        }

        err = soundio_outstream_end_write(outstream);
        Assertf(!err, "soundio end_write error %s", soundio_strerror(err));
    }
} // namespace sp
