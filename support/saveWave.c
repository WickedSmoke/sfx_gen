#include <stdint.h>
#include <stdio.h>

/*
 * Save PCM data to WAVE file.
 * Return error message or NULL if successful.
 */
const char* saveWave(const void* data, uint32_t dataSize,
                     int sampleRate, int bitsPerSample, int channels,
                     const char* filename)
{
    uint32_t dword;
    uint16_t word;
    uint32_t bytesPerSec = sampleRate * bitsPerSample/8 * channels;
    const char* err = "File write failed";
    FILE* fp = fopen(filename, "wb");

    if(! fp)
        return "File open failed";

#define WRITE_ID(STR) \
    if (fwrite(STR, 4, 1, fp) != 1) \
        goto cleanup

#define WRITE_32(N) \
    dword = N; \
    fwrite(&dword, 1, 4, fp)

#define WRITE_16(N) \
    word = N; \
    fwrite(&word, 1, 2, fp)

    // Write WAVE header.
    WRITE_ID("RIFF");               // "RIFF"
    WRITE_32(36 + dataSize);        // Remaining file size
    WRITE_ID("WAVE");               // "WAVE"

    WRITE_ID("fmt ");               // "fmt "
    WRITE_32(16);                   // Chunk size
    WRITE_16(1);                    // Compression code
    WRITE_16(channels);             // Channels
    WRITE_32(sampleRate);           // Sample rate
    WRITE_32(bytesPerSec);          // Bytes/sec
    WRITE_16(bitsPerSample/8);      // Block align
    WRITE_16(bitsPerSample);        // Bits per sample

    // Write sample data.
    WRITE_ID("data");               // "data"
    WRITE_32(dataSize);             // Chunk size
    if (fwrite(data, 1, dataSize, fp) == dataSize)
        err = NULL;

cleanup:
    fclose(fp);
    return err;
}
