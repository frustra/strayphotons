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
#include "strayphotons/cpp/Utility.hh"

#include <memory>
#include <resonance_audio/api/resonance_audio_api.h>
#include <resonance_audio/base/constants_and_types.h>
#include <soundio/soundio.h>
#include <tracy/Tracy.hpp>

namespace sp {
    static CVar<float> CVarVolume("s.Volume", 1.0f, "Global volume control");
    static CVar<int> CVarAudioBackend("s.AudioBackend",
        0,
        "Audio backend (0: JACK, 1: PulseAudio, 2: ALSA, 3: CoreAudio: 4: Wasapi)");

    AudioManager::AudioManager()
        : RegisteredThread("AudioManager", std::chrono::milliseconds(20), false), sampleRate(48000),
          decoderQueue("AudioDecode") {

        framesPerBuffer = std::min(vraudio::kMaxSupportedNumFrames,
            (size_t)CeilToPowerOfTwo(sampleRate * interval.count() / 1e9));
        Assertf(framesPerBuffer >= 32, "buffer too small: %d", framesPerBuffer); // FftManager::kMinFftSize
        Logf("Audio SampleRate: %llu (%llu frames per buffer)", sampleRate, framesPerBuffer);

        // Allow cycling through each audio backend 1 time
        maxRestartAttempts = (int)SoundIoBackendDummy - (int)SoundIoBackendNone;

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            soundObserver = lock.Watch<ecs::ComponentAddRemoveEvent<ecs::Audio>>();
        }

        resonance.reset(vraudio::CreateResonanceAudioApi(2, framesPerBuffer, sampleRate));

        StartThread();
    }

    std::vector<const char *> AudioManager::GetBackendNames() const {
        if (!soundio) return {};
        std::vector<const char *> result;
        result.reserve(soundio_backend_count(soundio));
        for (size_t i = 0; i < result.capacity(); i++) {
            SoundIoBackend backend = soundio_get_backend(soundio, i);
            if (backend != SoundIoBackendDummy) result.emplace_back(soundio_backend_name(backend));
        }
        return result;
    }

    void AudioManager::AudioErrorCallback(SoundIoOutStream *outstream, int error) {
        Errorf("AudioManager: libsoundio error: %s", soundio_strerror(error));

        if (!attemptRestart && restartAttempt < maxRestartAttempts) {
            attemptRestart = true;
            restartAttempt++;
        }
        Shutdown(false);
    }

    bool AudioManager::ThreadInit() {
        ZoneScoped;

        soundio = soundio_create();
        soundio->app_name = "Stray Photons";

        int err = 0;
        int backendCount = soundio_backend_count(soundio);
        int selectedBackend = CVarAudioBackend.Get(true) + restartAttempt;
        for (int i = 0; i < backendCount; i++) {
            selectedBackend = (selectedBackend + i) % backendCount;
            SoundIoBackend backend = soundio_get_backend(soundio, selectedBackend);
            err = soundio_connect_backend(soundio, backend);
            if (!err) {
                Logf("Using audio backend: %s", soundio_backend_name(backend));
                break;
            } else {
                Logf("Audio Backend %s error: %s", soundio_backend_name(backend), soundio_strerror(err));
            }
        }
        if (err) {
            Errorf("soundio_connect: %s", soundio_strerror(err));
            return false;
        }
        if (selectedBackend != CVarAudioBackend.Get()) {
            CVarAudioBackend.Set(selectedBackend);
            CVarAudioBackend.Get(true);
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
        Logf("Selected audio device: %s", device->name);

        outstream = soundio_outstream_create(device);
        outstream->format = SoundIoFormatFloat32NE;
        outstream->write_callback = [](SoundIoOutStream *outstream, int frameCountMin, int frameCountMax) {
            auto self = static_cast<AudioManager *>(outstream->userdata);
            self->AudioWriteCallback(outstream, frameCountMin, frameCountMax);
        };
        outstream->error_callback = [](SoundIoOutStream *outstream, int error) {
            auto self = static_cast<AudioManager *>(outstream->userdata);
            self->AudioErrorCallback(outstream, error);
        };
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

        (void)soundio_outstream_clear_buffer(outstream);
        return true;
    }

    void AudioManager::Shutdown(bool waitForExit) {
        StopThread(waitForExit);
    }

    void AudioManager::ThreadShutdown() {
        if (outstream) soundio_outstream_destroy(outstream);
        if (device) soundio_device_unref(device);
        if (soundio) soundio_destroy(soundio);
        outstream = nullptr;
        device = nullptr;
        soundio = nullptr;

        if (attemptRestart) {
            attemptRestart = false;
            Logf("Attempting audio thread restart (attempt %d/%d)", restartAttempt, maxRestartAttempts);
            decoderQueue.Dispatch<void>([this] {
                StopThread();
                StartThread();
            });
        }
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
        if (CVarAudioBackend.Changed()) {
            CVarAudioBackend.Get(true);
            restartAttempt = 0;
            Logf("Restarting audio system (backend changed)");
            decoderQueue.Dispatch<void>([this] {
                StopThread();
                StartThread();
            });
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
                ecs::QueueTransaction<ecs::Write<ecs::Audio, ecs::EventInput>>([ent = compEvent.entity](auto &lock) {
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
                        [this, file = source.file](std::shared_ptr<Asset> asset) {
                            ZoneScopedN("DecodeAudioData");
                            if (!asset) {
                                Logf("Audio file missing: %s", file->Get()->path.string());
                                return std::shared_ptr<nqr::AudioData>();
                            }
                            auto audioBuffer = decoderCache.Load(asset.get());
                            if (!audioBuffer) {
                                audioBuffer = std::make_shared<nqr::AudioData>();
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
                if (index >= (int)soundIDs->size()) continue;
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

        soundEvents.TryPollEvents([&](const SoundEvent &event) {
            auto &sound = sounds.Get(event.soundID);
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

        if (!resonance) return;

        struct SoundIoChannelArea *areas;
        int channelCount = outstream->layout.channel_count;
        int framesToWrite = std::max(frameCountMin, std::min(frameCountMax, (int)framesPerBuffer));

        // double out_latency = -1;
        // int err = soundio_outstream_get_latency(outstream, &out_latency);
        // Logf("frameCountMin %d frameCountMax %d framesPerBuffer %d framesToWrite %d latency %f (%s)",
        //     frameCountMin,
        //     frameCountMax,
        //     framesPerBuffer,
        //     framesToWrite,
        //     out_latency,
        //     soundio_strerror(err));

        while ((int)std::min(outputBuffers[0].size(), outputBuffers[1].size()) < framesToWrite) {
            ZoneScopedN("Render");
            // Logf("Render outputBuffers.size() %llu / %d",
            //     std::min(outputBuffers[0].size(), outputBuffers[1].size()),
            //     framesToWrite);

            outputBuffers[0].resize(outputBuffers[0].size() + framesPerBuffer);
            outputBuffers[1].resize(outputBuffers[1].size() + framesPerBuffer);
            float *outputPtrs[2] = {
                outputBuffers[0].data() + outputBuffers[0].size() - framesPerBuffer,
                outputBuffers[1].data() + outputBuffers[1].size() - framesPerBuffer,
            };

            auto validIndexPtr = sounds.GetValidIndexes();
            for (auto soundID : *validIndexPtr) {
                auto &source = sounds.Get(soundID);
                if (!source.play || !source.audioBuffer->Ready()) continue;

                auto &audioBuffer = *source.audioBuffer->Get();
                auto floatsRemaining = audioBuffer.samples.size() - source.bufferOffset;

                auto framesRemaining = std::min((size_t)framesPerBuffer, floatsRemaining / audioBuffer.channelCount);

                resonance->SetInterleavedBuffer(source.resonanceID,
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

            resonance->FillPlanarOutputBuffer(channelCount, framesPerBuffer, outputPtrs);
        }

        int framesWritten = 0;
        while (framesWritten < framesToWrite) {
            ZoneScopedN("CopyOutput");
            int batchSize = framesToWrite;
            int err = soundio_outstream_begin_write(outstream, &areas, &batchSize);
            if (err) {
                Errorf("soundio begin_write error %s", soundio_strerror(err));
                return;
            }
            // Logf("CopyOutput framesToWrite %d batchSize %d framesWritten %d", framesToWrite, batchSize,
            // framesWritten);

            bool interleaved = areas[0].step == (int)sizeof(float) * channelCount;
            Assert(interleaved || areas[0].step == (int)sizeof(float), "expected linear audio output buffer");
            size_t count = std::min({(size_t)batchSize, outputBuffers[0].size(), outputBuffers[1].size()});
            if (count > 0) {
                if (interleaved) {
                    auto outputPtr = reinterpret_cast<float *>(areas[0].ptr);
                    for (size_t i = 0; i < count; i++) {
                        *outputPtr++ = outputBuffers[0][i];
                        *outputPtr++ = outputBuffers[1][i];
                    }
                } else {
                    for (int channel = 0; channel < channelCount; channel++) {
                        auto channelPtr = reinterpret_cast<float *>(areas[channel].ptr);
                        size_t count = std::min((size_t)batchSize, outputBuffers[channel].size());
                        std::copy_n(outputBuffers[channel].begin(), count, channelPtr);
                    }
                }
                outputBuffers[0].erase(outputBuffers[0].begin(), outputBuffers[0].begin() + count);
                outputBuffers[1].erase(outputBuffers[1].begin(), outputBuffers[1].begin() + count);
                framesWritten += count;
            }

            err = soundio_outstream_end_write(outstream);
            if (err) {
                Errorf("soundio end_write error %s", soundio_strerror(err));
                return;
            }
        }
        // Logf("Finished bufferSize[0] %llu bufferSize[1] %llu", outputBuffers[0].size(), outputBuffers[1].size());
    }
} // namespace sp
