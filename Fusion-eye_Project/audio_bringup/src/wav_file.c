#include "audio_bringup.h"

#include <limits.h>
#include <string.h>

#define WAV_HEADER_SIZE 44U
#define WAV_MAX_DATA_SIZE (UINT32_MAX - 36U)

static void put_u16le(unsigned char *dst, uint16_t value)
{
    dst[0] = (unsigned char)(value & 0xffU);
    dst[1] = (unsigned char)((value >> 8) & 0xffU);
}

static void put_u32le(unsigned char *dst, uint32_t value)
{
    dst[0] = (unsigned char)(value & 0xffU);
    dst[1] = (unsigned char)((value >> 8) & 0xffU);
    dst[2] = (unsigned char)((value >> 16) & 0xffU);
    dst[3] = (unsigned char)((value >> 24) & 0xffU);
}

static uint16_t get_u16le(const unsigned char *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t get_u32le(const unsigned char *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static int write_all(FILE *file, const void *data, size_t bytes)
{
    return fwrite(data, 1, bytes, file) == bytes ? 0 : -1;
}

int wav_writer_open(WavWriter *writer, const char *path, uint32_t sample_rate,
                    uint16_t channels, uint16_t bits_per_sample)
{
    unsigned char header[WAV_HEADER_SIZE] = {0};
    uint32_t byte_rate;
    uint16_t block_align;

    if (writer == NULL || path == NULL || sample_rate == 0 || channels == 0 ||
        bits_per_sample == 0 || (bits_per_sample % 8U) != 0)
    {
        return -1;
    }
    writer->file = NULL;
    writer->data_bytes = 0;

    if (channels > UINT16_MAX / (bits_per_sample / 8U))
    {
        return -1;
    }
    block_align = (uint16_t)(channels * (bits_per_sample / 8U));
    if (block_align == 0 || sample_rate > UINT32_MAX / block_align)
    {
        return -1;
    }
    byte_rate = sample_rate * block_align;

    memcpy(header, "RIFF", 4);
    put_u32le(header + 4, 36U);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    put_u32le(header + 16, 16U);
    put_u16le(header + 20, 1U);
    put_u16le(header + 22, channels);
    put_u32le(header + 24, sample_rate);
    put_u32le(header + 28, byte_rate);
    put_u16le(header + 32, block_align);
    put_u16le(header + 34, bits_per_sample);
    memcpy(header + 36, "data", 4);
    put_u32le(header + 40, 0U);

    writer->file = fopen(path, "wb");
    if (writer->file == NULL)
    {
        return -1;
    }
    if (write_all(writer->file, header, sizeof(header)) != 0)
    {
        fclose(writer->file);
        writer->file = NULL;
        return -1;
    }
    return 0;
}

int wav_writer_write(WavWriter *writer, const void *data, uint32_t bytes)
{
    if (writer == NULL || writer->file == NULL || (data == NULL && bytes != 0) ||
        bytes > WAV_MAX_DATA_SIZE - writer->data_bytes)
    {
        return -1;
    }
    if (write_all(writer->file, data, bytes) != 0)
    {
        return -1;
    }
    writer->data_bytes += bytes;
    return 0;
}

int wav_writer_close(WavWriter *writer, int finalize)
{
    unsigned char value[4];
    int result = 0;

    if (writer == NULL || writer->file == NULL)
    {
        return -1;
    }

    if (finalize)
    {
        if (fseek(writer->file, 4L, SEEK_SET) != 0)
        {
            result = -1;
        }
        else
        {
            put_u32le(value, 36U + writer->data_bytes);
            if (write_all(writer->file, value, sizeof(value)) != 0 ||
                fseek(writer->file, 40L, SEEK_SET) != 0)
            {
                result = -1;
            }
            else
            {
                put_u32le(value, writer->data_bytes);
                if (write_all(writer->file, value, sizeof(value)) != 0)
                {
                    result = -1;
                }
            }
        }
        if (fflush(writer->file) != 0)
        {
            result = -1;
        }
    }

    if (fclose(writer->file) != 0)
    {
        result = -1;
    }
    writer->file = NULL;
    return result;
}

static int read_exact(FILE *file, void *buffer, size_t bytes)
{
    return fread(buffer, 1, bytes, file) == bytes ? 0 : -1;
}

int wav_reader_open(WavReader *reader, const char *path)
{
    unsigned char riff[12];
    unsigned char chunk_header[8];
    unsigned char fmt[16];
    long data_offset = -1;
    uint32_t data_bytes = 0;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint16_t format_tag = 0;
    long riff_end;
    long file_size;
    int have_format = 0;
    int have_data = 0;
    FILE *file;

    if (reader == NULL || path == NULL)
    {
        return -1;
    }
    memset(reader, 0, sizeof(*reader));
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return -1;
    }

    if (read_exact(file, riff, sizeof(riff)) != 0 ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0)
    {
        fclose(file);
        return -1;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        fseek(file, (long)sizeof(riff), SEEK_SET) != 0)
    {
        fclose(file);
        return -1;
    }
    if (get_u32le(riff + 4) < 4U ||
        get_u32le(riff + 4) > (uint32_t)(LONG_MAX - 8L))
    {
        fclose(file);
        return -1;
    }
    riff_end = 8L + (long)get_u32le(riff + 4);
    if (riff_end > file_size)
    {
        fclose(file);
        return -1;
    }

    while (!have_data || !have_format)
    {
        long chunk_start = ftell(file);
        uint32_t chunk_size;
        long next_chunk;

        if (chunk_start < 0 || chunk_start > riff_end - 8L ||
            read_exact(file, chunk_header, sizeof(chunk_header)) != 0)
        {
            fclose(file);
            return -1;
        }
        chunk_size = get_u32le(chunk_header + 4);
        if (chunk_size > (uint32_t)LONG_MAX)
        {
            fclose(file);
            return -1;
        }
        next_chunk = ftell(file);
        if (next_chunk < 0 || next_chunk > riff_end ||
            (long)chunk_size > LONG_MAX - next_chunk - 1L ||
            (long)chunk_size + (long)(chunk_size & 1U) > riff_end - next_chunk)
        {
            fclose(file);
            return -1;
        }
        next_chunk += (long)chunk_size + (long)(chunk_size & 1U);

        if (memcmp(chunk_header, "fmt ", 4) == 0)
        {
            if (chunk_size < sizeof(fmt) ||
                read_exact(file, fmt, sizeof(fmt)) != 0)
            {
                fclose(file);
                return -1;
            }
            format_tag = get_u16le(fmt);
            channels = get_u16le(fmt + 2);
            sample_rate = get_u32le(fmt + 4);
            bits_per_sample = get_u16le(fmt + 14);
            have_format = 1;
        }
        else if (memcmp(chunk_header, "data", 4) == 0)
        {
            data_offset = ftell(file);
            if (data_offset < 0 || (long)chunk_size > file_size - data_offset ||
                (long)chunk_size > riff_end - data_offset)
            {
                fclose(file);
                return -1;
            }
            data_bytes = chunk_size;
            have_data = 1;
        }

        if (fseek(file, next_chunk, SEEK_SET) != 0)
        {
            fclose(file);
            return -1;
        }
    }

    if (format_tag != 1U || channels != AUDIO_BRINGUP_CHANNELS ||
        sample_rate != AUDIO_BRINGUP_SAMPLE_RATE ||
        bits_per_sample != AUDIO_BRINGUP_BITS_PER_SAMPLE ||
        data_bytes == 0 ||
        (data_bytes % (AUDIO_BRINGUP_CHANNELS *
                       (AUDIO_BRINGUP_BITS_PER_SAMPLE / 8U))) != 0)
    {
        fclose(file);
        return -1;
    }

    if (fseek(file, data_offset, SEEK_SET) != 0)
    {
        fclose(file);
        return -1;
    }
    reader->file = file;
    reader->sample_rate = sample_rate;
    reader->channels = channels;
    reader->bits_per_sample = bits_per_sample;
    reader->data_bytes = data_bytes;
    reader->remaining_bytes = data_bytes;
    return 0;
}

size_t wav_reader_read(WavReader *reader, void *buffer, size_t capacity)
{
    if (reader == NULL || reader->file == NULL || buffer == NULL)
    {
        return 0;
    }
    if (capacity > reader->remaining_bytes)
    {
        capacity = reader->remaining_bytes;
    }
    capacity -= capacity % (AUDIO_BRINGUP_CHANNELS *
                            (AUDIO_BRINGUP_BITS_PER_SAMPLE / 8U));
    {
        size_t bytes = fread(buffer, 1, capacity, reader->file);
        reader->remaining_bytes -= (uint32_t)bytes;
        return bytes;
    }
}

void wav_reader_close(WavReader *reader)
{
    if (reader != NULL && reader->file != NULL)
    {
        fclose(reader->file);
        reader->file = NULL;
    }
}
