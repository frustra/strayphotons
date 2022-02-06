#include "AudioManager.hh"

#include <soundio/soundio.h>

namespace sp {
    AudioManager::AudioManager() : RegisteredThread("Audio", 120.0, false) {
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

        err = soundio_outstream_open(outstream);
        Assertf(!err, "soundio_outstream_open: %s", soundio_strerror(err));
        Assertf(!outstream->layout_error,
            "unable to set channel layout: %s",
            soundio_strerror(outstream->layout_error));

        err = soundio_outstream_start(outstream);
        Assertf(!err, "unable to start audio device: %s", soundio_strerror(err));
    }

    AudioManager::~AudioManager() {
        StopThread(false);
        if (soundio) soundio_wakeup(soundio);
        StopThread();
        if (outstream) soundio_outstream_destroy(outstream);
        if (device) soundio_device_unref(device);
        if (soundio) soundio_destroy(soundio);
    }

    void AudioManager::Frame() {
        soundio_wait_events(soundio);
    }

    static float seconds_offset = 0.0f;

    void AudioManager::AudioWriteCallback(SoundIoOutStream *outstream, int frameCountMin, int frameCountMax) {
        // temporary sine wave libsoundio example code
        const struct SoundIoChannelLayout *layout = &outstream->layout;
        float float_sample_rate = outstream->sample_rate;
        float seconds_per_frame = 1.0f / float_sample_rate;
        struct SoundIoChannelArea *areas;
        int frames_left = frameCountMax;
        int err;

        while (frames_left > 0) {
            int frame_count = frames_left;
            err = soundio_outstream_begin_write(outstream, &areas, &frame_count);
            Assertf(!err, "soundio begin_write error %s", soundio_strerror(err));

            if (!frame_count) break;

            float pitch = 440.0f;
            float radians_per_second = pitch * 2.0f * M_PI;
            for (int frame = 0; frame < frame_count; frame += 1) {
                float sample = sin((seconds_offset + frame * seconds_per_frame) * radians_per_second);
                for (int channel = 0; channel < layout->channel_count; channel += 1) {
                    float *ptr = (float *)(areas[channel].ptr + areas[channel].step * frame);
                    *ptr = sample * 0.5;
                }
            }
            seconds_offset = fmod(seconds_offset + seconds_per_frame * frame_count, 1.0);

            err = soundio_outstream_end_write(outstream);
            Assertf(!err, "soundio end_write error %s", soundio_strerror(err));

            frames_left -= frame_count;
        }
    }
} // namespace sp
