/*
    sfx_gen - Sound Effect Generator
    A stand-alone C version of DrPetter's sfxr synthesizer from
    http://www.drpetter.se/project_sfxr.html

    Copyright (c) 2022 Karl Robillard

    This code may be used under the terms of the MIT license (see sfx_gen.c).
*/

#include <stdint.h>

#define SFX_VERSION_STR "0.5.0"
#define SFX_VERSION     0x000500

enum SfxWaveType {
    SFX_SQUARE,
    SFX_SAWTOOTH,
    SFX_SINE,
    SFX_NOISE,
    SFX_TRIANGLE,
    SFX_PINK_NOISE
};

// Sound parameters (96 bytes matching rFXGen WaveParams)
typedef struct SfxParams
{
    // Random seed used to generate the wave
    uint32_t randSeed;

    // Wave type (square, sawtooth, sine, noise)
    int waveType;

    // Wave envelope parameters
    float attackTime;
    float sustainTime;
    float sustainPunch;
    float decayTime;

    // Frequency parameters
    float startFrequency;
    float minFrequency;
    float slide;
    float deltaSlide;
    float vibratoDepth;
    float vibratoSpeed;
    //float vibratoPhaseDelay;      // Unused in sfxr code.

    // Tone change parameters
    float changeAmount;
    float changeSpeed;

    // Square wave parameters
    float squareDuty;
    float dutySweep;

    // Repeat parameters
    float repeatSpeed;

    // Phaser parameters
    float phaserOffset;
    float phaserSweep;

    // Filter parameters
    float lpfCutoff;
    float lpfCutoffSweep;
    float lpfResonance;
    float hpfCutoff;
    float hpfCutoffSweep;
}
SfxParams;

// There are 8 parameters with -1,1 range:
//      slide, deltaSlide, changeAmount, dutySweep,
//      phaserOffset, phaserSweep, lpfCutoffSweep, hpfCutoffSweep
#define SFX_NEGATIVE_ONE_MASK   0x0025A4C0

enum SfxSampleFormat {
    SFX_U8,     // uint8_t
    SFX_I16,    // int16_t
    SFX_F32     // float
};

typedef struct SfxSynth {
    int sampleFormat;
    int sampleRate;             // Must be 44100 for now
    int maxDuration;            // Length in seconds
    union {
        uint8_t* u8;
        int16_t* i16;
        float*   f;
    } samples;                  // sampleRate * maxDuration
    float noiseBuffer[32];      // Random values for SFX_NOISE/SFX_PINK_NOISE
    float pinkWhiteValue[5];    // SFX_PINK_NOISE
    float phaserBuffer[1024];
}
SfxSynth;

#ifdef __cplusplus
extern "C" {
#endif

void sfx_resetParams(SfxParams *params);
SfxSynth* sfx_allocSynth(int format, int sampleRate, int maxDuration);
int sfx_generateWave(SfxSynth*, const SfxParams* params);

// Load/Save functions
const char* sfx_loadParams(SfxParams *params, const char *fileName,
                           float* sfsVolume);
const char* sfx_saveRfx(const SfxParams *params, const char *fileName);

// Parameter generator functions
void sfx_genPickupCoin(SfxParams*);
void sfx_genLaserShoot(SfxParams*);
void sfx_genExplosion(SfxParams*);
void sfx_genPowerup(SfxParams*);
void sfx_genHitHurt(SfxParams*);
void sfx_genJump(SfxParams*);
void sfx_genBlipSelect(SfxParams*);
void sfx_genSynth(SfxParams*);
void sfx_genRandomize(SfxParams*, int waveType);
void sfx_mutate(SfxParams *params, float range, uint32_t mask);

#ifdef __cplusplus
}
#endif
