# SSC027A audio bring-up utility

This is a small MI audio application for the SSC027A / Infinity6C EVB. It
captures the board's `ADC_AB` input as 16 kHz, stereo, 16-bit PCM WAV and plays
that WAV through `DAC_AB`. It is an initial hardware validation utility, not
the camera/RTSP integration.

## SDK integration and build

Extract the SDK's `sdk`, `project`, and `kernel` source archives using the
vendor's release instructions. Copy this directory to:

```text
sdk/verify/mi_demo/source/audio/audio_bringup/
```

From `sdk/verify/mi_demo`, build the target with the same configured I6C
toolchain and project settings used for the other MI demo apps:

```sh
make source/audio/audio_bringup
```

The MI demo build system discovers `audio_bringup.mk`; no vendor source files
need to be edited. The resulting executable is named
`prog_audio_audio_bringup`.

## Board test

Before powering or connecting a speaker, follow the SSC027A board guide and
the amplifier rework instructions for the exact EVB revision. The guide calls
out these audio jumpers:

| Jumper | Function |
| --- | --- |
| JP89 | Speaker amplifier power |
| JP87(H) | Speaker enable |
| JP81 | Speaker connection |
| JP140 / JP143 | Analog microphone enable |
| JP84 / JP85 | Analog microphone 0 / 1 |

Copy the executable to the board and run:

```sh
./prog_audio_audio_bringup capture /tmp/mic.wav 5
./prog_audio_audio_bringup play /tmp/mic.wav
```

Playback starts at -30 dB. The WAV reader intentionally accepts only the
format produced by this capture command. Increase output volume only after
confirming the amplifier and speaker wiring.

## Implementation notes

- Input and output use the SDK's `MI_AI` / `MI_AO` APIs, not generic ALSA
  device nodes. `ADC_AB` and `DAC_AB` are the I6C endpoints listed by the
  SDK's `st_audioHwInfo.h`.
- Capture frames are always released to `MI_AI` after they are consumed.
- The analog path does not configure I2S. The board DTS documents an I2S
  pinmux option that conflicts with UART/SD pins; only enable it for a
  verified external-I2S design.
- `audioCfg.txt` contains algorithm tuning values. Keep tuning separate from
  the initial raw capture/playback path.
- The SDK's `es7210Tdm` example targets another board and external ES7210
  codec hardware; its I2C and pin settings are not used here.

The vendor guide's `SC430AI_4M_h264_Audio_30fps.json` and AMP rework PDF were
not present in the supplied folder. Obtain the matching revision-specific
assets before reproducing the vendor's camera + audio mixer/RTSP test.

## Host-side WAV test

The WAV container code can be checked without the vendor toolchain:

```sh
set -eu
test_bin=$(mktemp /tmp/audio_bringup_wav_test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
cc -std=c99 -Wall -Wextra -Werror \
  -Iinclude tests/test_wav_file.c src/wav_file.c \
  -o "$test_bin"
"$test_bin"
```
