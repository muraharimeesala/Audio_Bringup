#define _POSIX_C_SOURCE 200809L

#include "audio_bringup.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mi_ao.h"

#define AO_DEVICE_ID 0
#define AO_WRITE_TIMEOUT_MS 100
#define AO_MAX_RETRIES 50U
#define AO_WRITE_BYTES (AUDIO_BRINGUP_PERIOD_FRAMES * AUDIO_BRINGUP_CHANNELS * \
                        (AUDIO_BRINGUP_BITS_PER_SAMPLE / 8U))

static void preserve_error(int *result, const char *operation, MI_S32 status)
{
    fprintf(stderr, "%s failed: 0x%08x\n", operation, (unsigned int)status);
    if (*result == 0)
    {
        *result = -1;
    }
}

int audio_play_wav(const char *path)
{
    MI_AO_Attr_t attr;
    MI_S32 status;
    WavReader reader = {0};
    unsigned char buffer[AO_WRITE_BYTES];
    int device_open = 0;
    int output_attached = 0;
    int result = 0;
    unsigned int retries = 0;
    unsigned int drain_checks;

    if (wav_reader_open(&reader, path) != 0)
    {
        fprintf(stderr,
                "Could not read %s. Expected 16 kHz, stereo, 16-bit PCM WAV.\n",
                path);
        return -1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.enFormat = E_MI_AUDIO_FORMAT_PCM_S16_LE;
    attr.enSoundMode = E_MI_AUDIO_SOUND_MODE_STEREO;
    attr.enSampleRate = E_MI_AUDIO_SAMPLE_RATE_16000;
    attr.u32PeriodSize = AUDIO_BRINGUP_PERIOD_FRAMES;
    attr.enChannelMode = E_MI_AO_CHANNEL_MODE_STEREO;
    attr.enMixerSampleRate = E_MI_AUDIO_SAMPLE_RATE_16000;

    status = MI_AO_Open(AO_DEVICE_ID, &attr);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AO_Open", status);
        goto cleanup;
    }
    device_open = 1;

    status = MI_AO_AttachIf(AO_DEVICE_ID, E_MI_AO_IF_DAC_AB, 0);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AO_AttachIf(DAC_AB)", status);
        goto cleanup;
    }
    output_attached = 1;

    status = MI_AO_SetVolume(AO_DEVICE_ID, -30, -30,
                             E_MI_AO_GAIN_FADING_64_SAMPLE);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AO_SetVolume", status);
        goto cleanup;
    }

    fprintf(stdout, "Playing %s on DAC_AB at -30 dB.\n", path);
    for (;;)
    {
        size_t bytes = wav_reader_read(&reader, buffer, sizeof(buffer));
        if (bytes == 0)
        {
            if (ferror(reader.file))
            {
                fprintf(stderr, "Failed reading WAV file %s.\n", path);
                result = -1;
            }
            break;
        }

        retries = 0;
        for (;;)
        {
            status = MI_AO_Write(AO_DEVICE_ID, buffer, (MI_U32)bytes, 0,
                                 AO_WRITE_TIMEOUT_MS);
            if (status == MI_SUCCESS)
            {
                break;
            }
            if (status != MI_AO_ERR_NOBUF || retries >= AO_MAX_RETRIES)
            {
                preserve_error(&result, "MI_AO_Write", status);
                break;
            }
            ++retries;
        }
        if (result != 0)
        {
            break;
        }
    }

    if (result == 0)
    {
        for (drain_checks = 0; drain_checks < 500U; ++drain_checks)
        {
            MI_U32 remaining_frames = 0;
            MI_U64 timestamp = 0;
            struct timespec delay = {0, 10 * 1000 * 1000};

            status = MI_AO_GetTimestamp(AO_DEVICE_ID, &remaining_frames,
                                        &timestamp);
            if (status != MI_SUCCESS)
            {
                preserve_error(&result, "MI_AO_GetTimestamp", status);
                break;
            }
            if (remaining_frames == 0)
            {
                break;
            }
            nanosleep(&delay, NULL);
        }
        if (drain_checks == 500U && result == 0)
        {
            fprintf(stderr, "Timed out waiting for DAC playback to drain.\n");
            result = -1;
        }
    }

cleanup:
    if (output_attached)
    {
        status = MI_AO_DetachIf(AO_DEVICE_ID, E_MI_AO_IF_DAC_AB);
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AO_DetachIf(DAC_AB)", status);
        }
    }
    if (device_open)
    {
        status = MI_AO_Close(AO_DEVICE_ID);
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AO_Close", status);
        }
    }
    wav_reader_close(&reader);
    return result;
}
