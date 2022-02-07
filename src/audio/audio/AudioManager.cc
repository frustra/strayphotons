#include "AudioManager.hh"

#include "assets/AssetManager.hh"
#include "core/Tracing.hh"

#include <resonance_audio_api.h>
#include <soundio/soundio.h>

namespace sp {
    AudioManager::AudioManager() : RegisteredThread("Audio", std::chrono::milliseconds(20), false) {
        StartThread();
    }

    void AudioManager::Init() {
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

        err = soundio_outstream_start(outstream);
        Assertf(!err, "unable to start audio device: %s", soundio_strerror(err));

        bufferSize = outstream->sample_rate * interval.count() / 1e9;
        resonance = vraudio::CreateResonanceAudioApi(outstream->layout.channel_count,
            bufferSize,
            outstream->sample_rate);

        testAsset = GAssets.Load("audio/test.ogg");
        testAsset->WaitUntilValid();
        loader.Load(&testAudio, "ogg", testAsset->Buffer());

        testObj = resonance->CreateStereoSource(2);
        resonance->SetSourceVolume(testObj, 0.5f);
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
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Transform, ecs::Name>>();
    }

    void AudioManager::AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax) {
        ZoneScoped;
        auto self = static_cast<AudioManager *>(outstream->userdata);
        if (!self->resonance) return;

        struct SoundIoChannelArea *areas;
        int framesPerBuffer = self->bufferSize;
        int framesToWrite = framesPerBuffer * std::max(std::min(1, frameCountMax / framesPerBuffer),
                                                  (frameCountMin + framesPerBuffer - 1) / framesPerBuffer);
        Assertf(framesToWrite > 0, "wanted %d frames, min %d max %d", framesPerBuffer, frameCountMin, frameCountMax);
        int err = soundio_outstream_begin_write(outstream, &areas, &framesToWrite);

        int channelCount = outstream->layout.channel_count;
        auto basePtr = reinterpret_cast<float *>(areas[0].ptr);

        for (int channel = 0; channel < channelCount; channel++) {
            Assert(areas[channel].step == sizeof(float) * channelCount, "expected interleaved output buffer");
            Assert((float *)areas[channel].ptr == basePtr + channel, "expected interleaved output buffer");
        }

        auto outputBuffer = (float *)areas[0].ptr;
        float *lastSample = nullptr;

        while (framesToWrite >= framesPerBuffer) {
            ZoneScopedN("Render");

            if (self->testObj >= 0) {
                auto floatsPerBuffer = framesPerBuffer * self->testAudio.channelCount;
                auto offset = self->bufferOffset % (self->testAudio.samples.size() - floatsPerBuffer + 1);
                self->resonance->SetInterleavedBuffer(self->testObj,
                    &self->testAudio.samples[offset],
                    self->testAudio.channelCount,
                    framesPerBuffer);
                self->bufferOffset += floatsPerBuffer;
            }

            self->resonance->FillInterleavedOutputBuffer(channelCount, framesPerBuffer, outputBuffer);
            outputBuffer += framesPerBuffer * channelCount;
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
