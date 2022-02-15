#pragma once

#include "assets/Asset.hh"
#include "core/EntityMap.hh"
#include "core/RegisteredThread.hh"
#include "ecs/NamedEntity.hh"

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
        virtual bool ThreadInit() override;
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

        struct SoundSource {
            int resonanceID = -1;
            size_t bufferOffset;
            shared_ptr<const Asset> audioFile;
            shared_ptr<nqr::AudioData> audioBuffer;
        };

        std::mutex soundsMutex;
        EntityMap<SoundSource> sounds;

        ecs::NamedEntity headEntity, headEntityFallback;

        ecs::ComponentObserver<ecs::Sound> soundObserver;

        static void AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax);
    };
} // namespace sp
