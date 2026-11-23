#include "audio_bringup.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mi_ai.h"

#define AI_DEVICE_ID 0
#define AI_CHANNEL_GROUP 0
#define AI_READ_TIMEOUT_MS 100
#define AI_MAX_NO_DATA_MS 5000U

static void preserve_error(int *result, const char *operation, MI_S32 status)
{
    fprintf(stderr, "%s failed: 0x%08x\n", operation, (unsigned int)status);
    if (*result == 0)
    {
        *result = -1;
    }
}

int audio_capture_wav(const char *path, unsigned int seconds)
{
    MI_AI_Attr_t attr;
    MI_AI_If_e input_interface = E_MI_AI_IF_ADC_AB;
    MI_AI_Data_t mic_data;
    MI_AI_Data_t echo_data;
    MI_S32 status;
    unsigned int no_data_ms = 0;
    uint32_t bytes_written = 0;
    uint32_t target_bytes;
    const uint32_t bytes_per_second =
        AUDIO_BRINGUP_SAMPLE_RATE * AUDIO_BRINGUP_CHANNELS *
        (AUDIO_BRINGUP_BITS_PER_SAMPLE / 8U);
    WavWriter writer = {0};
    int device_open = 0;
    int channel_enabled = 0;
    int result = 0;

    if (path == NULL || seconds == 0 || seconds > UINT32_MAX / bytes_per_second)
    {
        fprintf(stderr, "Invalid capture path or duration.\n");
        return -1;
    }
    target_bytes = seconds * bytes_per_second;

    if (wav_writer_open(&writer, path, AUDIO_BRINGUP_SAMPLE_RATE,
                        AUDIO_BRINGUP_CHANNELS,
                        AUDIO_BRINGUP_BITS_PER_SAMPLE) != 0)
    {
        fprintf(stderr, "Could not create WAV file: %s\n", path);
        return -1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.enFormat = E_MI_AUDIO_FORMAT_PCM_S16_LE;
    attr.enSoundMode = E_MI_AUDIO_SOUND_MODE_STEREO;
    attr.enSampleRate = E_MI_AUDIO_SAMPLE_RATE_16000;
    attr.u32PeriodSize = AUDIO_BRINGUP_PERIOD_FRAMES;
    attr.bInterleaved = TRUE;

    status = MI_AI_Open(AI_DEVICE_ID, &attr);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AI_Open", status);
        goto cleanup;
    }
    device_open = 1;

    status = MI_AI_AttachIf(AI_DEVICE_ID, &input_interface, 1);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AI_AttachIf(ADC_AB)", status);
        goto cleanup;
    }

    status = MI_AI_EnableChnGroup(AI_DEVICE_ID, AI_CHANNEL_GROUP);
    if (status != MI_SUCCESS)
    {
        preserve_error(&result, "MI_AI_EnableChnGroup", status);
        goto cleanup;
    }
    channel_enabled = 1;

    fprintf(stdout, "Capturing %u seconds from ADC_AB to %s\n", seconds, path);
    while (bytes_written < target_bytes)
    {
        uint32_t frame_bytes;
        uint32_t bytes_to_write;
        int release_failed = 0;

        memset(&mic_data, 0, sizeof(mic_data));
        memset(&echo_data, 0, sizeof(echo_data));
        status = MI_AI_Read(AI_DEVICE_ID, AI_CHANNEL_GROUP, &mic_data,
                            &echo_data, AI_READ_TIMEOUT_MS);
        if (status == MI_AI_ERR_NOBUF)
        {
            no_data_ms += AI_READ_TIMEOUT_MS;
            if (no_data_ms >= AI_MAX_NO_DATA_MS)
            {
                fprintf(stderr, "No audio frames received for %u ms.\n",
                        no_data_ms);
                result = -1;
                break;
            }
            continue;
        }
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AI_Read", status);
            break;
        }
        no_data_ms = 0;

        frame_bytes = mic_data.u32Byte[0];
        bytes_to_write = target_bytes - bytes_written;
        if (bytes_to_write > frame_bytes)
        {
            bytes_to_write = frame_bytes;
        }
        bytes_to_write -= bytes_to_write %
                          (AUDIO_BRINGUP_CHANNELS *
                           (AUDIO_BRINGUP_BITS_PER_SAMPLE / 8U));
        if (mic_data.apvBuffer[0] == NULL || bytes_to_write == 0)
        {
            fprintf(stderr, "MI_AI_Read returned an empty or invalid frame.\n");
            result = -1;
        }
        else if (wav_writer_write(&writer, mic_data.apvBuffer[0],
                                 bytes_to_write) != 0)
        {
            fprintf(stderr, "Failed writing audio data to %s.\n", path);
            result = -1;
        }
        else
        {
            bytes_written += bytes_to_write;
        }

        status = MI_AI_ReleaseData(AI_DEVICE_ID, AI_CHANNEL_GROUP, &mic_data,
                                   &echo_data);
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AI_ReleaseData", status);
            release_failed = 1;
        }
        if (result != 0 || release_failed)
        {
            break;
        }
    }

cleanup:
    if (channel_enabled)
    {
        status = MI_AI_DisableChnGroup(AI_DEVICE_ID, AI_CHANNEL_GROUP);
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AI_DisableChnGroup", status);
        }
    }
    if (device_open)
    {
        status = MI_AI_Close(AI_DEVICE_ID);
        if (status != MI_SUCCESS)
        {
            preserve_error(&result, "MI_AI_Close", status);
        }
    }
    if (writer.file != NULL)
    {
        if (wav_writer_close(&writer, 1) != 0)
        {
            fprintf(stderr, "Failed finalizing WAV file %s.\n", path);
            result = -1;
        }
    }

    if (result == 0)
    {
        fprintf(stdout, "Wrote %u bytes of PCM audio.\n", bytes_written);
    }
    return result;
}
