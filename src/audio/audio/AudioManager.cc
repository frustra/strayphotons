/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "AudioManager.hh"

#include "assets/AssetManager.hh"
#include "common/Tracing.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"

#include <resonance_audio/api/resonance_audio_api.h>
#include <resonance_audio/base/constants_and_types.h>
#include <soundio/soundio.h>

namespace sp {
    static CVar<float> CVarVolume("s.Volume", 1.0f, "Global volume control");

    AudioManager::AudioManager()
        : RegisteredThread("AudioManager", std::chrono::milliseconds(20), false), sampleRate(48000),
          decoderQueue("AudioDecode") {

        framesPerBuffer = sampleRate * interval.count() / 1e9;
        Assertf(framesPerBuffer < vraudio::kMaxSupportedNumFrames, "buffer too big: %d", framesPerBuffer);
        Assertf(framesPerBuffer >= 32, "buffer too small: %d", framesPerBuffer); // FftManager::kMinFftSize

        resonance.reset(vraudio::CreateResonanceAudioApi(2, framesPerBuffer, sampleRate));

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            soundObserver = lock.Watch<ecs::ComponentAddRemoveEvent<ecs::Audio>>();
        }

        StartThread();
    }

    void AudioManager::AudioErrorCallback(SoundIoOutStream *outstream, int error) {
        Errorf("Shutting down audio manager: libsoundio error: %s", soundio_strerror(error));

        auto self = static_cast<AudioManager *>(outstream->userdata);
        self->Shutdown(true);
    }

    bool AudioManager::ThreadInit() {
        ZoneScoped;

        soundio = soundio_create();

        int err = soundio_connect(soundio);
        if (err) {
            Errorf("soundio_connect: %s", soundio_strerror(err));
            return false;
        }

        soundio_flush_events(soundio);

        deviceIndex = soundio_default_output_device_index(soundio);
        if (deviceIndex < 0) {
            Errorf("no audio output device available");
            Shutdown(false);
            return false;
        }

        device = soundio_get_output_device(soundio, deviceIndex);
        if (!device) {
            Errorf("failed to open audio output device");
            Shutdown(false);
            return false;
        }

        outstream = soundio_outstream_create(device);
        outstream->format = SoundIoFormatFloat32NE;
        outstream->write_callback = AudioWriteCallback;
        outstream->error_callback = AudioErrorCallback;
        outstream->userdata = this;
        outstream->sample_rate = sampleRate;

        err = soundio_outstream_open(outstream);
        if (err) {
            Errorf("soundio_outstream_open: %s", soundio_strerror(err));
            Shutdown(false);
            return false;
        }
        if (outstream->layout_error) {
            Errorf("unable to set channel layout: %s", soundio_strerror(outstream->layout_error));
            Shutdown(false);
            return false;
        }

        auto channelCount = outstream->layout.channel_count;
        if (channelCount != 2) {
            Errorf("only stereo output is supported, have %d channels", channelCount);
            Shutdown(false);
            return false;
        }

        err = soundio_outstream_start(outstream);
        if (err) {
            Errorf("unable to start audio device: %s", soundio_strerror(err));
            Shutdown(false);
            return false;
        }
        return true;
    }

    void AudioManager::Shutdown(bool waitForExit) {
        StopThread(waitForExit);
        if (outstream) soundio_outstream_destroy(outstream);
        if (device) soundio_device_unref(device);
        if (soundio) soundio_destroy(soundio);
        outstream = nullptr;
        device = nullptr;
        soundio = nullptr;
    }

    AudioManager::~AudioManager() {
        Shutdown(true);
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
        auto lock = ecs::StartTransaction<ecs::Read<ecs::Audio, ecs::TransformSnapshot, ecs::Name, ecs::EventInput>>();

        auto head = entities::Head.Get(lock);
        if (head.Has<ecs::TransformSnapshot>(lock)) {
            auto &transform = head.Get<ecs::TransformSnapshot>(lock).globalPose;
            auto pos = transform.GetPosition();
            auto rot = transform.GetRotation();
            resonance->SetHeadPosition(pos.x, pos.y, pos.z);
            resonance->SetHeadRotation(rot.x, rot.y, rot.z, rot.w);
        }

        ecs::ComponentAddRemoveEvent<ecs::Audio> compEvent;
        while (soundObserver.Poll(lock, compEvent)) {
            if (compEvent.type == Tecs::EventType::ADDED) {
                if (!compEvent.entity.Has<ecs::EventInput, ecs::Audio>(lock)) continue;
                ecs::QueueTransaction<ecs::Write<ecs::Audio, ecs::EventInput>>(
                    [this, ent = compEvent.entity](auto &lock) {
                        if (!ent.Has<ecs::EventInput, ecs::Audio>(lock)) return;

                        auto &audio = ent.Get<ecs::Audio>(lock);
                        if (!audio.eventQueue) audio.eventQueue = ecs::EventQueue::New();
                        auto &eventInput = ent.Get<ecs::EventInput>(lock);
                        eventInput.Register(lock, audio.eventQueue, "/sound/play");
                        eventInput.Register(lock, audio.eventQueue, "/sound/resume");
                        eventInput.Register(lock, audio.eventQueue, "/sound/pause");
                        eventInput.Register(lock, audio.eventQueue, "/sound/stop");
                    });
            } else if (compEvent.type == Tecs::EventType::REMOVED) {

                auto *entSound = soundEntityMap.find(compEvent.entity);
                if (!entSound) continue;

                for (auto id : *entSound) {
                    auto &state = sounds.Get(id);
                    if (state.resonanceID >= 0) resonance->DestroySource(state.resonanceID);
                    sounds.FreeItem(id);
                }
                soundEntityMap.erase(compEvent.entity);
            }
        }

        auto globalVolumeChanged = CVarVolume.Changed();
        auto globalVolume = std::min(10.0f, CVarVolume.Get(true));

        for (const ecs::Entity &ent : lock.EntitiesWith<ecs::Audio>()) {
            InlineVector<size_t, 128> *soundIDs;

            auto &sources = ent.Get<ecs::Audio>(lock);
            if (soundEntityMap.count(ent) == 0) {
                soundIDs = &soundEntityMap[ent];
                for (auto &source : sources.sounds) {
                    if (soundIDs->size() == soundIDs->capacity()) break;
                    auto soundID = sounds.AllocateItem();
                    soundIDs->push_back(soundID);

                    auto &state = sounds.Get(soundID);
                    state.resonanceID = -1;
                    state.loop = source.loop;
                    state.play = source.playOnLoad;
                    state.bufferOffset = 0;
                    state.volume = 0;
                    state.occlusion = 0;
                    state.audioBuffer = decoderQueue.Dispatch<nqr::AudioData>(source.file,
                        [this, file = source.file](shared_ptr<Asset> asset) {
                            ZoneScopedN("DecodeAudioData");
                            if (!asset) {
                                Logf("Audio file missing: %s", file->Get()->path.string());
                                return shared_ptr<nqr::AudioData>();
                            }
                            auto audioBuffer = decoderCache.Load(asset.get());
                            if (!audioBuffer) {
                                audioBuffer = make_shared<nqr::AudioData>();
                                loader.Load(audioBuffer.get(), asset->extension, asset->Buffer());
                                decoderCache.Register(asset.get(), audioBuffer);
                            }
                            return audioBuffer;
                        });
                }
            } else {
                soundIDs = &soundEntityMap[ent];
            }

            for (size_t i = 0; i < sources.sounds.size() && i < soundIDs->size(); i++) {
                auto &source = sources.sounds[i];
                auto id = soundIDs->at(i);
                auto &state = sounds.Get(id);
                auto resonanceID = state.resonanceID;
                if (resonanceID == -1 && state.audioBuffer->Ready()) {
                    auto dataBuffer = state.audioBuffer->Get();
                    if (dataBuffer) {
                        auto channelCount = dataBuffer->channelCount;
                        if (source.type == ecs::SoundType::Object) {
                            resonanceID = resonance->CreateSoundObjectSource(vraudio::kBinauralHighQuality);
                        } else if (source.type == ecs::SoundType::Stereo) {
                            resonanceID = resonance->CreateStereoSource(channelCount);
                        } else if (source.type == ecs::SoundType::Ambisonic) {
                            resonanceID = resonance->CreateAmbisonicSource(channelCount);
                        }
                        state.resonanceID = resonanceID;
                        sounds.MakeItemValid(id);
                    }
                }

                if (resonanceID != -1) {
                    if (source.volume != state.volume || globalVolumeChanged) {
                        resonance->SetSourceVolume(resonanceID, source.volume * globalVolume);
                        state.volume = source.volume;
                    }

                    if (sources.occlusion != state.occlusion) {
                        resonance->SetSoundObjectOcclusionIntensity(resonanceID, sources.occlusion);
                        state.occlusion = sources.occlusion;
                    }

                    if (ent.Has<ecs::TransformSnapshot>(lock)) {
                        auto &transform = ent.Get<ecs::TransformSnapshot>(lock).globalPose;
                        auto pos = transform.GetPosition();
                        auto rot = transform.GetRotation();
                        resonance->SetSourcePosition(resonanceID, pos.x, pos.y, pos.z);
                        resonance->SetSourceRotation(resonanceID, rot.x, rot.y, rot.z, rot.w);
                    }
                }
            }

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, sources.eventQueue, event)) {
                int *ptr = ecs::EventData::TryGet<int>(event.data);
                int index = ptr ? *ptr : 0;
                if (index >= soundIDs->size()) continue;
                auto soundID = soundIDs->at(index);

                if (event.name == "/sound/play") {
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::PlayFromStart, soundID});
                } else if (event.name == "/sound/resume") {
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::Resume, soundID});
                } else if (event.name == "/sound/pause") {
                    soundEvents.PushEvent(SoundEvent{SoundEvent::Type::Pause, soundID});
                } else if (event.name == "/sound/stop") {
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
        if (framesToWrite <= 0) return;
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
