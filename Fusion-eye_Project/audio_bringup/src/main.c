#include "audio_bringup.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mi_sys.h"

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s capture <output.wav> <seconds>\n"
            "  %s play <input.wav>\n"
            "\n"
            "WAV format: 16 kHz, stereo, signed 16-bit PCM.\n",
            program, program);
}

static int parse_seconds(const char *text, unsigned int *seconds)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > 3600UL || value > UINT_MAX)
    {
        return -1;
    }
    *seconds = (unsigned int)value;
    return 0;
}

int main(int argc, char **argv)
{
    MI_S32 ret;
    int result;

    if (argc < 3)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "capture") == 0)
    {
        unsigned int seconds;

        if (argc != 4 || parse_seconds(argv[3], &seconds) != 0)
        {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        ret = MI_SYS_Init(0);
        if (ret != MI_SUCCESS)
        {
            fprintf(stderr, "MI_SYS_Init failed: 0x%08x\n", (unsigned int)ret);
            return EXIT_FAILURE;
        }
        result = audio_capture_wav(argv[2], seconds);
    }
    else if (strcmp(argv[1], "play") == 0)
    {
        if (argc != 3)
        {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        ret = MI_SYS_Init(0);
        if (ret != MI_SUCCESS)
        {
            fprintf(stderr, "MI_SYS_Init failed: 0x%08x\n", (unsigned int)ret);
            return EXIT_FAILURE;
        }
        result = audio_play_wav(argv[2]);
    }
    else
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ret = MI_SYS_Exit(0);
    if (ret != MI_SUCCESS)
    {
        fprintf(stderr, "MI_SYS_Exit failed: 0x%08x\n", (unsigned int)ret);
        result = -1;
    }

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
