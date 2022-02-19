#ifndef SAVEWAVE_H
#define SAVEWAVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
#endif
const char* saveWave(const void* data, uint32_t dataSize,
                     int sampleRate, int bitsPerSample, int channels,
                     const char* filename);

#endif
