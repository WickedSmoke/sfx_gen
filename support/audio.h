#ifndef AUDIO_H
#define AUDIO_H
/*
  Audio Module
  Copyright 2005-2012,2022 Karl Robillard

  This code may be used under the terms of the MIT license (see audio_openal.c).
*/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  aud_startup();
void aud_shutdown();
void aud_stopAll();
void aud_pauseProcessing(int paused);
void aud_genBuffers(int count, uint32_t* ids);
void aud_freeBuffers(int count, uint32_t* ids);
int  aud_loadBufferI16(uint32_t bufId, const int16_t* samples, int sampleCount,
                       int stereo, int freq);
int  aud_loadBufferF32(uint32_t bufId, const float* samples, int sampleCount,
                       int stereo, int freq);
uint32_t aud_playSound(uint32_t bufferId);
void aud_stopSound(uint32_t sourceId);
void aud_setSoundVolume(float);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
