/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Asset.hh"
#include "audio/LockFreeAudioSet.hh"
#include "common/PreservingMap.hh"
#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "strayphotons/cpp/Async.hh"
#include "strayphotons/cpp/DispatchQueue.hh"
#include "strayphotons/cpp/EntityMap.hh"
#include "strayphotons/cpp/LockFreeEventQueue.hh"

#include <libnyquist/Decoders.h>
#include <vector>

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

        std::vector<const char *> GetBackendNames() const;

    protected:
        bool ThreadInit() override;
        void Frame() override;
        void ThreadShutdown() override;

    private:
        void SyncFromECS();
        void Shutdown(bool waitForExit);
        bool attemptRestart = false;
        int restartAttempt = 0;
        int maxRestartAttempts = 5;

        size_t sampleRate;
        size_t framesPerBuffer;

        SoundIo *soundio = nullptr;
        int deviceIndex;
        SoundIoDevice *device = nullptr;
        SoundIoOutStream *outstream = nullptr;
        std::unique_ptr<vraudio::ResonanceAudioApi> resonance;
        std::array<std::vector<float>, 2> outputBuffers;

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

        EntityMap<InlineVector<size_t, 128>> soundEntityMap;
        LockFreeAudioSet<SoundSource, 65535> sounds;
        LockFreeEventQueue<SoundEvent> soundEvents;

        ecs::ComponentAddRemoveObserver<ecs::Audio> soundObserver;

        void AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax);
        void AudioErrorCallback(SoundIoOutStream *outstream, int error);

        PreservingMap<const Asset *, nqr::AudioData> decoderCache;
        DispatchQueue decoderQueue;
    };
} // namespace sp
