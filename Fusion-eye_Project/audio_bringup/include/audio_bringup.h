#ifndef AUDIO_BRINGUP_H
#define AUDIO_BRINGUP_H

#include <stdint.h>
#include <stdio.h>

#define AUDIO_BRINGUP_SAMPLE_RATE 16000U
#define AUDIO_BRINGUP_CHANNELS 2U
#define AUDIO_BRINGUP_BITS_PER_SAMPLE 16U
#define AUDIO_BRINGUP_PERIOD_FRAMES 1024U

typedef struct
{
    FILE *file;
    uint32_t data_bytes;
} WavWriter;

typedef struct
{
    FILE *file;
    uint32_t sample_rate;
    uint32_t data_bytes;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t remaining_bytes;
} WavReader;

int wav_writer_open(WavWriter *writer, const char *path, uint32_t sample_rate,
                    uint16_t channels, uint16_t bits_per_sample);
int wav_writer_write(WavWriter *writer, const void *data, uint32_t bytes);
int wav_writer_close(WavWriter *writer, int finalize);

int wav_reader_open(WavReader *reader, const char *path);
size_t wav_reader_read(WavReader *reader, void *buffer, size_t capacity);
void wav_reader_close(WavReader *reader);

int audio_capture_wav(const char *path, unsigned int seconds);
int audio_play_wav(const char *path);

#endif
