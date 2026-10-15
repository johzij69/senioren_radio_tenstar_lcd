#pragma once

#include <FS.h>

namespace AlarmAudio
{
static constexpr const char *PATH = "/noalarm.mp3";
static constexpr const char *TEMP_PATH = "/noalarm.upload";
static constexpr size_t MAX_BYTES = 512 * 1024;
static constexpr size_t RESERVED_BYTES = 128 * 1024;

inline size_t frameSize(const uint8_t *header)
{
    unsigned version = (header[1] >> 3) & 3;
    unsigned bitrateIndex = header[2] >> 4;
    unsigned sampleIndex = (header[2] >> 2) & 3;
    if (header[0] != 0xFF || (header[1] & 0xE0) != 0xE0 || version == 1 ||
        ((header[1] >> 1) & 3) != 1 || bitrateIndex == 0 || bitrateIndex == 15 || sampleIndex == 3)
        return 0;
    static const unsigned mpeg1Rates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
    static const unsigned mpeg2Rates[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
    static const unsigned sampleRates[] = {44100, 48000, 32000};
    unsigned rate = version == 3 ? mpeg1Rates[bitrateIndex] : mpeg2Rates[bitrateIndex];
    unsigned sampleRate = sampleRates[sampleIndex] >> (version == 3 ? 0 : (version == 2 ? 1 : 2));
    return (version == 3 ? 144000UL : 72000UL) * rate / sampleRate + ((header[2] >> 1) & 1);
}

inline bool hasMp3Header(File &file)
{
    if (!file || file.size() < 10 || file.size() > MAX_BYTES) return false;
    uint8_t header[10];
    if (file.read(header, sizeof(header)) != sizeof(header)) return false;
    size_t offset = 0;
    if (memcmp(header, "ID3", 3) == 0)
    {
        if (header[3] < 2 || header[3] > 4 || ((header[6] | header[7] | header[8] | header[9]) & 0x80)) return false;
        offset = 10 + (static_cast<size_t>(header[6]) << 21) + (static_cast<size_t>(header[7]) << 14) +
                 (static_cast<size_t>(header[8]) << 7) + header[9];
        if (header[3] == 4 && (header[5] & 0x10)) offset += 10;
    }
    for (unsigned frame = 0; frame < 2; ++frame)
    {
        if (offset > file.size() || file.size() - offset < 4 || !file.seek(offset) || file.read(header, 4) != 4) return false;
        size_t size = frameSize(header);
        if (size < 4 || size > file.size() - offset) return false;
        offset += size;
    }
    return true;
}
}