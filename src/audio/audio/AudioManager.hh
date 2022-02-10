#pragma once

#include "assets/Asset.hh"
#include "core/RegisteredThread.hh"

#include <libnyquist/Decoders.h>

struct SoundIo;
struct SoundIoDevice;
struct SoundIoOutStream;

namespace vraudio {
    class ResonanceAudioApi;
}

namespace sp {
    class AudioManager : public RegisteredThread {
    public:
        AudioManager();
        ~AudioManager();

    protected:
        virtual void Init() override;
        virtual void Frame() override;

    private:
        void SyncFromECS();

        SoundIo *soundio = nullptr;
        int deviceIndex;
        SoundIoDevice *device = nullptr;
        SoundIoOutStream *outstream = nullptr;
        vraudio::ResonanceAudioApi *resonance = nullptr;
        size_t framesPerBuffer = 1024; // updated later depending on sample rate and desired latency

        nqr::NyquistIO loader;
        nqr::AudioData testAudio;

        shared_ptr<const Asset> testAsset;
        int testObj = -1;
        size_t bufferOffset = 0;

        static void AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax);
    };
} // namespace sp
