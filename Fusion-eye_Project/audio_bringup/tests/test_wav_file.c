#define _POSIX_C_SOURCE 200809L

#include "audio_bringup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    static const unsigned char expected[] = {
        0x00, 0x10, 0x00, 0x20, 0x00, 0x30, 0x00, 0x40
    };
    static const unsigned char one_frame[] = {0, 0, 0, 0};
    unsigned char actual[sizeof(expected)];
    WavWriter writer = {0};
    WavReader reader = {0};
    char path[] = "/tmp/audio_bringup_test_XXXXXX";
    char invalid_path[] = "/tmp/audio_bringup_invalid_XXXXXX";
    size_t bytes;
    int fd;

    fd = mkstemp(path);
    if (fd < 0)
    {
        return fail("create temporary path");
    }
    if (close(fd) != 0)
    {
        unlink(path);
        return fail("close temporary file");
    }

    if (wav_writer_open(&writer, path, AUDIO_BRINGUP_SAMPLE_RATE,
                        AUDIO_BRINGUP_CHANNELS,
                        AUDIO_BRINGUP_BITS_PER_SAMPLE) != 0)
    {
        return fail("open WAV writer");
    }
    if (wav_writer_write(&writer, expected, sizeof(expected)) != 0)
    {
        wav_writer_close(&writer, 0);
        unlink(path);
        return fail("write WAV payload");
    }
    if (wav_writer_close(&writer, 1) != 0)
    {
        unlink(path);
        return fail("finalize WAV header");
    }
    if (wav_reader_open(&reader, path) != 0)
    {
        unlink(path);
        return fail("read WAV header");
    }
    if (reader.sample_rate != AUDIO_BRINGUP_SAMPLE_RATE ||
        reader.channels != AUDIO_BRINGUP_CHANNELS ||
        reader.bits_per_sample != AUDIO_BRINGUP_BITS_PER_SAMPLE ||
        reader.data_bytes != sizeof(expected))
    {
        wav_reader_close(&reader);
        unlink(path);
        return fail("WAV metadata round trip");
    }

    bytes = wav_reader_read(&reader, actual, sizeof(actual));
    if (bytes != sizeof(expected) ||
        memcmp(actual, expected, sizeof(expected)) != 0)
    {
        wav_reader_close(&reader);
        unlink(path);
        return fail("WAV payload round trip");
    }
    if (wav_reader_read(&reader, actual, sizeof(actual)) != 0)
    {
        wav_reader_close(&reader);
        unlink(path);
        return fail("WAV data boundary");
    }
    wav_reader_close(&reader);

    if (unlink(path) != 0)
    {
        return fail("remove temporary WAV");
    }

    fd = mkstemp(invalid_path);
    if (fd < 0)
    {
        return fail("create invalid-format temporary path");
    }
    if (close(fd) != 0)
    {
        unlink(invalid_path);
        return fail("close invalid-format temporary file");
    }
    if (wav_writer_open(&writer, invalid_path, 8000U,
                        AUDIO_BRINGUP_CHANNELS,
                        AUDIO_BRINGUP_BITS_PER_SAMPLE) != 0 ||
        wav_writer_write(&writer, one_frame, sizeof(one_frame)) != 0 ||
        wav_writer_close(&writer, 1) != 0)
    {
        if (writer.file != NULL)
        {
            wav_writer_close(&writer, 0);
        }
        unlink(invalid_path);
        return fail("create invalid-format WAV");
    }
    if (wav_reader_open(&reader, invalid_path) == 0)
    {
        wav_reader_close(&reader);
        unlink(invalid_path);
        return fail("reject unsupported sample rate");
    }
    if (unlink(invalid_path) != 0)
    {
        return fail("remove invalid-format WAV");
    }

    puts("PASS: WAV round trip and unsupported-format rejection");
    return 0;
}
