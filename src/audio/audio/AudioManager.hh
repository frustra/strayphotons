#pragma once

#include "assets/Asset.hh"
#include "assets/Async.hh"
#include "audio/LockFreeAudioSet.hh"
#include "audio/LockFreeEventQueue.hh"
#include "core/DispatchQueue.hh"
#include "core/EntityMap.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
#include "ecs/EntityRef.hh"

#include <atomic>
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
        bool ThreadInit() override;
        void Frame() override;

    private:
        void SyncFromECS();
        void Shutdown(bool waitForExit);

        size_t sampleRate;
        size_t framesPerBuffer = 1024; // updated later depending on sample rate and desired latency

        SoundIo *soundio = nullptr;
        int deviceIndex;
        SoundIoDevice *device = nullptr;
        SoundIoOutStream *outstream = nullptr;
        std::unique_ptr<vraudio::ResonanceAudioApi> resonance;

        nqr::NyquistIO loader;

        struct SoundSource {
            int resonanceID = -1;
            AsyncPtr<nqr::AudioData> audioBuffer;
            float volume, occlusion;
            bool loop, play;
            size_t bufferOffset;
        };

        struct SoundEvent {
            enum class Type {
                PlayFromStart,
                Resume,
                Pause,
                Stop,
            } type;

            size_t soundID;
        };

        EntityMap<vector<size_t>> soundEntityMap;
        LockFreeAudioSet<SoundSource, 65535> sounds;
        LockFreeEventQueue<SoundEvent> soundEvents;

        ecs::ComponentObserver<ecs::Audio> soundObserver;

        static void AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax);
        static void AudioErrorCallback(SoundIoOutStream *outstream, int error);

        PreservingMap<const Asset *, nqr::AudioData> decoderCache;
        DispatchQueue decoderQueue;
    };
} // namespace sp
