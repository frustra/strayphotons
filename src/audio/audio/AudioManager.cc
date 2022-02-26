#include "AudioManager.hh"

#include "assets/AssetManager.hh"
#include "core/Tracing.hh"

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

        headEntity = ecs::NamedEntity("player", "vr-hmd");
        headEntityFallback = ecs::NamedEntity("player", "flatview");

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

        resonance = vraudio::CreateResonanceAudioApi(2, framesPerBuffer, outstream->sample_rate);

        err = soundio_outstream_start(outstream);
        Assertf(!err, "unable to start audio device: %s", soundio_strerror(err));

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            soundObserver = lock.Watch<ecs::ComponentEvent<ecs::Sound>>();
        }
        return true;
    }

    AudioManager::~AudioManager() {
        StopThread();
        if (outstream) soundio_outstream_destroy(outstream);
        if (device) soundio_device_unref(device);
        if (soundio) soundio_destroy(soundio);
        if (resonance) delete resonance;
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
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Sound, ecs::Transform, ecs::Name>>();

        ecs::ComponentEvent<ecs::Sound> event;
        while (soundObserver.Poll(lock, event)) {
            if (event.type != Tecs::EventType::REMOVED) continue;

            auto it = sounds.find(event.entity);
            if (it == sounds.end()) continue;

            auto &state = it->second;
            if (state.resonanceID >= 0) resonance->DestroySource(state.resonanceID);

            std::lock_guard soundsLock(soundsMutex);
            sounds.erase(event.entity);
        }

        auto head = headEntity.Get(lock);
        if (!head) head = headEntityFallback.Get(lock);
        if (head && head.Has<ecs::Transform>(lock)) {
            auto transform = head.Get<ecs::Transform>(lock);
            auto pos = transform.GetPosition();
            auto rot = transform.GetRotation();
            resonance->SetHeadPosition(pos.x, pos.y, pos.z);
            resonance->SetHeadRotation(rot.x, rot.y, rot.z, rot.w);
        }

        for (auto ent : lock.EntitiesWith<ecs::Sound>()) {
            auto &source = ent.Get<ecs::Sound>(lock);

            std::lock_guard soundsLock(soundsMutex);
            auto &state = sounds[ent];

            if (!state.audioFile) state.audioFile = source.file;
            if (!state.audioBuffer) {
                state.bufferOffset = 0;
                state.audioBuffer = decoderQueue.Dispatch<nqr::AudioData>(state.audioFile,
                    [this](shared_ptr<Asset> asset) {
                        Assertf(asset, "Audio file missing");
                        auto audioBuffer = make_shared<nqr::AudioData>();
                        loader.Load(audioBuffer.get(), "ogg", asset->Buffer());
                        return audioBuffer;
                    });
            }

            if (state.resonanceID == -1) {
                state.resonanceID = resonance->CreateSoundObjectSource(vraudio::kBinauralHighQuality);
            }

            resonance->SetSourceVolume(state.resonanceID, 0.5f);

            if (ent.Has<ecs::Transform>(lock)) {
                auto &transform = ent.Get<ecs::Transform>(lock);
                auto pos = transform.GetPosition();
                auto rot = transform.GetRotation();
                resonance->SetSourcePosition(state.resonanceID, pos.x, pos.y, pos.z);
                resonance->SetSourceRotation(state.resonanceID, rot.x, rot.y, rot.z, rot.w);
            }
        }
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
                {
                    ZoneScopedN("UpdateSources");
                    std::lock_guard lock(self->soundsMutex);
                    for (auto &it : self->sounds) {
                        auto &source = it.second;
                        if (!source.audioBuffer || !source.audioBuffer->Ready()) continue;

                        auto &audioBuffer = *source.audioBuffer->Get();
                        self->resonance->SetInterleavedBuffer(source.resonanceID,
                            &audioBuffer.samples[source.bufferOffset],
                            audioBuffer.channelCount,
                            framesPerBuffer);

                        auto floatsPerSourceBuffer = framesPerBuffer * audioBuffer.channelCount;
                        source.bufferOffset = (source.bufferOffset + floatsPerSourceBuffer) %
                                              (audioBuffer.samples.size() - floatsPerSourceBuffer + 1);
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
