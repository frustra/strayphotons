#pragma once

#include "core/RegisteredThread.hh"

struct SoundIo;
struct SoundIoDevice;
struct SoundIoOutStream;

namespace sp {
    class AudioManager : public RegisteredThread {
    public:
        AudioManager();
        ~AudioManager();

    protected:
        virtual void Frame();

    private:
        SoundIo *soundio = nullptr;
        int deviceIndex;
        SoundIoDevice *device = nullptr;
        SoundIoOutStream *outstream = nullptr;

        static void AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax);
    };
} // namespace sp
