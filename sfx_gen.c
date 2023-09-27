/*
    sfx_gen - Sound Effect Generator
    A stand-alone C version of DrPetter's sfxr synthesizer from
    http://www.drpetter.se/project_sfxr.html

    Copyright (c) 2007 Tomas Pettersson
    Copyright (c) 2022 Karl Robillard

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "sfx_gen.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This function provided by the user returns an integer between
// 0 (inclusive) and range (exclusive).
extern int sfx_random(int range);

// If only a single output sample format is needed, it can be hardcoded
// to optimize sfx_generateWave a bit.
// SINGLE_FORMAT values: 1=uint8_t 2=int16_t 3=float
//#define SINGLE_FORMAT   2

// Apply squareDuty to sawtooth waveform.
#define SAWTOOTH_DUTY

#define PI  3.14159265f

/*
 * Allocate a synth structure and sample buffer as a single block of memory.
 * Returns a pointer to an initialized SfxSynth structure which the caller
 * must free().
 *
 * The use of this function is optional, as the user can provide their own
 * SfxSynth struct and buffer.
 */
SfxSynth* sfx_allocSynth(int format, int sampleRate, int maxDuration)
{
    SfxSynth* syn;
    size_t bufLen = sampleRate * maxDuration;

    if (format == SFX_I16)
        bufLen *= sizeof(int16_t);
    else if (format == SFX_F32)
        bufLen *= sizeof(float);

    syn = (SfxSynth*) malloc(sizeof(SfxSynth) + bufLen);
    if (syn) {
        syn->sampleFormat = format;
        syn->sampleRate   = sampleRate;
        syn->maxDuration  = maxDuration;
        syn->samples.f    = (float*) (syn + 1);
    }
    return syn;
}

// Return float in the range 0.0 to 1.0 (both inclusive).
static float frnd(float range)
{
    return (float)sfx_random(10001)/10000.0f*range;
}

// Return float in the range -1.0 to 1.0 (both inclusive).
static float rndNP1()
{
    return (float)sfx_random(20001)/10000.0f - 1.0f;
}

#define PINK_SIZE   5

// Return -1.0 to 1.0.
static float pinkValue(int* pinkI, float* whiteValue)
{
    float sum = 0.0;
    int bitsChanged;
    int lastI = *pinkI;
    int i = lastI + 1;
    if (i > 0x1f)       // Number of set bits matches PINK_SIZE.
        i = 0;
    bitsChanged = lastI ^ i;
    *pinkI = i;

    for (i = 0; i < PINK_SIZE; ++i) {
        if (bitsChanged & (1 << i))
            whiteValue[i] = frnd(1.0f);
        sum += whiteValue[i];
    }
    return (sum/PINK_SIZE) * 2.0f - 1.0f;
}

/*
 * Synthesize wave data from parameters.
 * A 44100Hz, mono channel wave is generated.
 *
 * Return the number of samples generated.
 */
int sfx_generateWave(SfxSynth* synth, const SfxParams* sp)
{
    float* phaserBuffer = synth->phaserBuffer;
    float* noiseBuffer  = synth->noiseBuffer;
    int phase = 0;
    double fperiod;
    double fmaxperiod;
    double fslide;
    double fdslide;
    int period;
    float squareDuty;
    float squareSlide;
    int envStage;
    int envTime;
    int envLength[3];
    float envVolume;
    float fphase;
    float fdphase;
    int iphase;
    int ipp;
    float fltp;
    float fltdp;
    float fltw;
    float fltwd;
    float fltdmp;
    float fltphp;
    float flthp;
    float flthpd;
    float vibratoPhase;
    float vibratoSpeed;
    float vibratoAmplitude;
    int repeatTime;
    int repeatLimit;
    int arpeggioTime;
    int arpeggioLimit;
    double arpeggioModulation;
    float minFreq, sslide;
    int pinkI;
    int i, sampleCount;


    // Sanity check some related parameters.
    minFreq = sp->minFrequency;
    if (minFreq > sp->startFrequency)
        minFreq = sp->startFrequency;

    sslide = sp->slide;
    if (sslide < sp->deltaSlide)
        sslide = sp->deltaSlide;


#define RESET_SAMPLE \
    fperiod = 100.0/(sp->startFrequency*sp->startFrequency + 0.001); \
    period = (int)fperiod; \
    fmaxperiod = 100.0/(minFreq * minFreq + 0.001); \
    fslide = 1.0 - pow((double)sslide, 3.0)*0.01; \
    fdslide = -pow((double)sp->deltaSlide, 3.0)*0.000001; \
    squareDuty = 0.5f - sp->squareDuty*0.5f; \
    squareSlide = -sp->dutySweep*0.00005f; \
    arpeggioModulation = (sp->changeAmount >= 0.0f) ? \
                        1.0 - pow((double)sp->changeAmount, 2.0)*0.9 : \
                        1.0 + pow((double)sp->changeAmount, 2.0)*10.0; \
    arpeggioTime = 0; \
    arpeggioLimit = (sp->changeSpeed == 1.0f) ? 0 : \
                        (int)(powf(1.0f - sp->changeSpeed, 2.0f)*20000 + 32);


    RESET_SAMPLE

    // Reset filter
    fltp = fltdp = 0.0f;
    fltw = powf(sp->lpfCutoff, 3.0f)*0.1f;
    fltwd = 1.0f + sp->lpfCutoffSweep*0.0001f;
    fltdmp = 5.0f/(1.0f + powf(sp->lpfResonance, 2.0f)*20.0f)*(0.01f + fltw);
    if (fltdmp > 0.8f)
        fltdmp = 0.8f;
    fltphp = 0.0f;
    flthp = powf(sp->hpfCutoff, 2.0f)*0.1f;
    flthpd = 1.0f + sp->hpfCutoffSweep*0.0003f;

    // Reset vibrato
    vibratoPhase = 0.0f;
    vibratoSpeed = powf(sp->vibratoSpeed, 2.0f)*0.01f;
    vibratoAmplitude = sp->vibratoDepth*0.5f;

    // Reset envelope
    envVolume = 0.0f;
    envStage = envTime = 0;
    envLength[0] = (int)(sp->attackTime *sp->attackTime *100000.0f);
    envLength[1] = (int)(sp->sustainTime*sp->sustainTime*100000.0f);
    envLength[2] = (int)(sp->decayTime  *sp->decayTime  *100000.0f);

    fphase = powf(sp->phaserOffset, 2.0f)*1020.0f;
    if (sp->phaserOffset < 0.0f)
        fphase = -fphase;

    fdphase = powf(sp->phaserSweep, 2.0f)*1.0f;
    if (sp->phaserSweep < 0.0f)
        fdphase = -fdphase;

    iphase = abs((int)fphase);
    ipp = 0;
    for (i = 0; i < 1024; i++)
        phaserBuffer[i] = 0.0f;

    if (sp->waveType == SFX_PINK_NOISE) {
        pinkI = 0;
        for (i = 0; i < PINK_SIZE; i++)
            synth->pinkWhiteValue[i] = frnd(1.0f);
    }

#define RESET_NOISE \
    if (sp->waveType == SFX_NOISE) { \
        for (i = 0; i < 32; i++) \
            noiseBuffer[i] = rndNP1(); \
    } else if (sp->waveType == SFX_PINK_NOISE) { \
        for (i = 0; i < 32; i++) \
            noiseBuffer[i] = pinkValue(&pinkI, synth->pinkWhiteValue); \
    }

    RESET_NOISE

    repeatTime = 0;
    repeatLimit = (int)(powf(1.0f - sp->repeatSpeed, 2.0f)*20000 + 32);
    if (sp->repeatSpeed == 0.0f)
        repeatLimit = 0;


    // Synthesize samples.
    {
    const float sampleCoefficient = 0.2f;   // Scales sample value to [-1..1]
#if SINGLE_FORMAT == 1
    uint8_t* buffer = synth->samples.u8;
#elif SINGLE_FORMAT == 2
    int16_t* buffer = synth->samples.i16;
#else
    float* buffer = synth->samples.f;
#endif
    float ssample, rfperiod, fp, pp;
    int sampleEnd = synth->sampleRate * synth->maxDuration;
    int si;

    for (sampleCount = 0; sampleCount < sampleEnd; sampleCount++)
    {
        repeatTime++;
        if (repeatLimit != 0 && repeatTime >= repeatLimit) {
            repeatTime = 0;
            RESET_SAMPLE
        }

        // Frequency envelopes/arpeggios
        arpeggioTime++;

        if ((arpeggioLimit != 0) && (arpeggioTime >= arpeggioLimit)) {
            arpeggioLimit = 0;
            fperiod *= arpeggioModulation;
        }

        fslide += fdslide;
        fperiod *= fslide;

        if (fperiod > fmaxperiod) {
            fperiod = fmaxperiod;
            if (minFreq > 0.0f)
                sampleEnd = sampleCount;    // End generator loop.
        }

        rfperiod = (float)fperiod;

        if (vibratoAmplitude > 0.0f) {
            vibratoPhase += vibratoSpeed;
            rfperiod = (float)
                (fperiod * (1.0 + sinf(vibratoPhase) * vibratoAmplitude));
        }

        period = (int)rfperiod;
        if (period < 8)
            period = 8;

        squareDuty += squareSlide;
        if (squareDuty < 0.0f)
            squareDuty = 0.0f;
        else if (squareDuty > 0.5f)
            squareDuty = 0.5f;

        // Volume envelope
        envTime++;
        if (envTime > envLength[envStage]) {
            envTime = 0;
next_stage:
            envStage++;
            if (envStage == 3)
                break;          // End generator loop.
            if (envLength[envStage] == 0)
                goto next_stage;
        }

        switch (envStage) {
            case 0:
                envVolume = (float)envTime/envLength[0];
                break;
            case 1:
                envVolume = 1.0f + powf(1.0f - (float)envTime/envLength[1], 1.0f) * 2.0f * sp->sustainPunch;
                break;
            case 2:
                envVolume = 1.0f - (float)envTime/envLength[2];
                break;
        }

        // Phaser step
        fphase += fdphase;
        iphase = abs((int)fphase);

        if (iphase > 1023)
            iphase = 1023;

        if (flthpd != 0.0f) {
            flthp *= flthpd;
            if (flthp < 0.00001f)
                flthp = 0.00001f;
            else if (flthp > 0.1f)
                flthp = 0.1f;
        }

        // 8x supersampling
        ssample = 0.0f;
        for (si = 0; si < 8; si++) {
            float sample = 0.0f;
            phase++;

            if (phase >= period) {
                //phase = 0;
                phase %= period;

                RESET_NOISE
            }

            // Base waveform
            fp = (float)phase/period;

#define RAMP(v, x1, x2, y1, y2) (y1 + (y2 - y1) * ((v - x1) / (x2 - x1)))

            switch (sp->waveType) {
                case SFX_SQUARE:
                    sample = (fp < squareDuty) ? 0.5f : -0.5f;
                    break;
                case SFX_SAWTOOTH:
#ifdef SAWTOOTH_DUTY
                    sample = (fp < squareDuty) ?
                             -1.0f + 2.0f * fp/squareDuty :
                              1.0f - 2.0f * (fp-squareDuty)/(1.0f-squareDuty);
#else
                    sample = 1.0f - fp*2;
#endif
                    break;
                case SFX_SINE:
                    sample = sinf(fp*2*PI);
                    break;
                case SFX_NOISE:
                case SFX_PINK_NOISE:
                    sample = noiseBuffer[phase*32/period];
                    break;
                case SFX_TRIANGLE:
                    sample = (fp < 0.5) ? RAMP(fp, 0.0f, 0.5f, -1.0f, 1.0f) :
                                          RAMP(fp, 0.5f, 1.0f, 1.0f, -1.0f);
                    break;
            }

            // Low-pass filter
            pp = fltp;
            fltw *= fltwd;

            if (fltw < 0.0f)
                fltw = 0.0f;
            else if (fltw > 0.1f)
                fltw = 0.1f;

            if (sp->lpfCutoff != 1.0f) {
                fltdp += (sample-fltp)*fltw;
                fltdp -= fltdp*fltdmp;
            } else {
                fltp = sample;
                fltdp = 0.0f;
            }

            fltp += fltdp;

            // High-pass filter
            fltphp += fltp - pp;
            fltphp -= fltphp*flthp;
            sample = fltphp;

            // Phaser
            phaserBuffer[ipp & 1023] = sample;
            sample += phaserBuffer[(ipp - iphase + 1024) & 1023];
            ipp = (ipp + 1) & 1023;

            // Final accumulation and envelope application
            ssample += sample*envVolume;
        }

        ssample = ssample/8 * sampleCoefficient;

        // Clamp sample and emit to buffer
        if (ssample > 1.0f)
            ssample = 1.0f;
        else if (ssample < -1.0f)
            ssample = -1.0f;

        //printf("%d %f\n", sampleCount, ssample);
#if SINGLE_FORMAT == 1
        *buffer++ = (uint8_t) (ssample*127.0f + 128.0f);
#elif SINGLE_FORMAT == 2
        *buffer++ = (int16_t) (ssample*32767.0f);
#elif SINGLE_FORMAT == 3
        *buffer++ = ssample;
#else
        switch (synth->sampleFormat) {
            case SFX_U8:
                ((uint8_t*)buffer)[sampleCount] = (uint8_t)
                                                    (ssample*127.0f + 128.0f);
                break;
            case SFX_I16:
                ((int16_t*)buffer)[sampleCount] = (int16_t) (ssample*32767.0f);
                break;
            case SFX_F32:
                buffer[sampleCount] = ssample;
                break;
        }
#endif
    }
    }

    return sampleCount;
}

#ifndef CONFIG_SFX_NO_FILEIO
//----------------------------------------------------------------------------
// Load/Save functions

/*
 * Load an rFXGen (.rfx) or sfxr settings file.
 * Returns error message or NULL if load was successful.
 */
const char* sfx_loadParams(SfxParams *sp, const char *fileName,
                           float* sfsVolume)
{
    union {
        int32_t version;
        char signature[4];
    } header;
    size_t n;
    const char *err = "File read failed";
    FILE *fp = fopen(fileName, "rb");
    if (fp == NULL)
        return "File open failed";

    n = fread(&header, 1, 4, fp);
    if (n != 4)
        goto cleanup;

    // Check for .rfx file signature.
    if ((header.signature[0] == 'r') &&
        (header.signature[1] == 'F') &&
        (header.signature[2] == 'X') &&
        (header.signature[3] == ' '))
    {
        uint16_t version, length;
        fread(&version, 1, sizeof(uint16_t), fp);
        fread(&length,  1, sizeof(uint16_t), fp);

        if (version != 200)
            err = "rFX file version not supported";
        else if (length != sizeof(SfxParams))
            err = "Invalid rFX wave parameters size";
        else {
            // Load all parameters.
            n = fread(sp, 1, sizeof(SfxParams), fp);
            if (n == sizeof(SfxParams))
                err = NULL;
        }
    } else {
        // Load sfxr settings.  Note that vibratoPhaseDelay & filterOn are
        // unused in the original sfxr code.

        float volume = 0.5f;
        float vibratoPhaseDelay;
        char filterOn;
        int version = header.version;

        if ((version == 100) || (version == 101) || (version == 102))
        {
            fread(&sp->waveType, 1, sizeof(int), fp);

            if (version == 102)
                fread(&volume, 1, sizeof(float), fp);
            if (sfsVolume)
                *sfsVolume = volume;

            fread(&sp->startFrequency, 1, sizeof(float), fp);
            fread(&sp->minFrequency, 1, sizeof(float), fp);
            fread(&sp->slide, 1, sizeof(float), fp);

            if (version >= 101)
                fread(&sp->deltaSlide, 1, sizeof(float), fp);
            else
                sp->deltaSlide = 0.0f;

            fread(&sp->squareDuty, 1, sizeof(float), fp);
            fread(&sp->dutySweep,  1, sizeof(float), fp);

            fread(&sp->vibratoDepth, 1, sizeof(float), fp);
            fread(&sp->vibratoSpeed, 1, sizeof(float), fp);
            fread(&vibratoPhaseDelay,    1, sizeof(float), fp);

            fread(&sp->attackTime,   1, sizeof(float), fp);
            fread(&sp->sustainTime,  1, sizeof(float), fp);
            fread(&sp->decayTime,    1, sizeof(float), fp);
            fread(&sp->sustainPunch, 1, sizeof(float), fp);

            fread(&filterOn, 1, 1, fp);
            fread(&sp->lpfResonance,   1, sizeof(float), fp);
            fread(&sp->lpfCutoff,      1, sizeof(float), fp);
            fread(&sp->lpfCutoffSweep, 1, sizeof(float), fp);
            fread(&sp->hpfCutoff,      1, sizeof(float), fp);
            fread(&sp->hpfCutoffSweep, 1, sizeof(float), fp);

            fread(&sp->phaserOffset,   1, sizeof(float), fp);
            fread(&sp->phaserSweep,    1, sizeof(float), fp);
            fread(&sp->repeatSpeed,    1, sizeof(float), fp);

            if (version >= 101) {
                fread(&sp->changeSpeed,  1, sizeof(float), fp);
                fread(&sp->changeAmount, 1, sizeof(float), fp);
            } else {
                sp->changeSpeed =
                sp->changeAmount = 0.0f;
            }
            err = NULL;
        }
        else
            err = "SFS file version not supported";
    }

cleanup:
    fclose(fp);
    return err;
}

/*
 * Save rFXGen (.rfx) sound parameters file.
 * Returns error message or NULL if save was successful.
 */
const char* sfx_saveRfx(const SfxParams *sp, const char *fileName)
{
    FILE *fp;
    size_t n;
    uint16_t version = 200;
    uint16_t dataLen = 96;

    assert(sizeof(SfxParams) == 96);

    fp = fopen(fileName, "wb");
    if (fp == NULL)
        return "File open failed";

    // Write .rfx file header.
    fwrite("rFX ",   1, 4, fp);
    fwrite(&version, 1, sizeof(uint16_t), fp);
    fwrite(&dataLen, 1, sizeof(uint16_t), fp);

    // Write sound parameters.
    n = fwrite(sp, 1, dataLen, fp);
    fclose(fp);

    if (n != dataLen)
        return "File write failed";
    return NULL;
}
#endif

#ifndef CONFIG_SFX_NO_GENERATORS
//----------------------------------------------------------------------------
/*
 * Parameter generator functions
 *
 * If randSeed is being used the caller is responsible for seeding the
 * random number generator before the call and setting the variable after it.
 */

/*
 * Reset sound parameters to a default square wave.
 * The randSeed is set to zero.
 */
void sfx_resetParams(SfxParams *sp)
{
    sp->randSeed = 0;
    sp->waveType = SFX_SQUARE;

    // Wave envelope parameters
    sp->attackTime = 0.0f;
    sp->sustainTime = 0.3f;
    sp->sustainPunch = 0.0f;
    sp->decayTime = 0.4f;

    // Frequency parameters
    sp->startFrequency = 0.3f;
    sp->minFrequency = 0.0f;
    sp->slide = 0.0f;
    sp->deltaSlide = 0.0f;
    sp->vibratoDepth = 0.0f;
    sp->vibratoSpeed = 0.0f;
    //sp->vibratoPhaseDelay = 0.0f;

    // Tone change parameters
    sp->changeAmount = 0.0f;
    sp->changeSpeed = 0.0f;

    // Square wave parameters
    sp->squareDuty = 0.0f;
    sp->dutySweep = 0.0f;

    // Repeat parameters
    sp->repeatSpeed = 0.0f;

    // Phaser parameters
    sp->phaserOffset = 0.0f;
    sp->phaserSweep = 0.0f;

    // Filter parameters
    sp->lpfCutoff = 1.0f;
    sp->lpfCutoffSweep = 0.0f;
    sp->lpfResonance = 0.0f;
    sp->hpfCutoff = 0.0f;
    sp->hpfCutoffSweep = 0.0f;
}

void sfx_genPickupCoin(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->startFrequency  = 0.4f + frnd(0.5f);
    sp->attackTime      = 0.0f;
    sp->sustainTime     = frnd(0.1f);
    sp->decayTime       = 0.1f + frnd(0.4f);
    sp->sustainPunch    = 0.3f + frnd(0.3f);

    if (sfx_random(2)) {
        sp->changeSpeed  = 0.5f + frnd(0.2f);
        sp->changeAmount = 0.2f + frnd(0.4f);
    }
}

void sfx_genLaserShoot(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->waveType = sfx_random(3);

    if ((sp->waveType == SFX_SINE) && sfx_random(2))
        sp->waveType = sfx_random(2);

    sp->startFrequency = 0.5f + frnd(0.5f);
    sp->minFrequency = sp->startFrequency - 0.2f - frnd(0.6f);

    if (sp->minFrequency < 0.2f)
        sp->minFrequency = 0.2f;

    sp->slide = -0.15f - frnd(0.2f);

    if (sfx_random(3) == 0) {
        sp->startFrequency = 0.3f + frnd(0.6f);
        sp->minFrequency = frnd(0.1f);
        sp->slide = -0.35f - frnd(0.3f);
    }

    if (sfx_random(2)) {
        sp->squareDuty = frnd(0.5f);
        sp->dutySweep  = frnd(0.2f);
    } else {
        sp->squareDuty = 0.4f + frnd(0.5f);
        sp->dutySweep  = -frnd(0.7f);
    }

    sp->attackTime = 0.0f;
    sp->sustainTime = 0.1f + frnd(0.2f);
    sp->decayTime = frnd(0.4f);

    if (sfx_random(2))
        sp->sustainPunch = frnd(0.3f);

    if (sfx_random(3) == 0) {
        sp->phaserOffset = frnd(0.2f);
        sp->phaserSweep = -frnd(0.2f);
    }

    if (sfx_random(2))
        sp->hpfCutoff = frnd(0.3f);
}

void sfx_genExplosion(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->waveType = SFX_NOISE;

    if (sfx_random(2)) {
        sp->startFrequency  =  0.1f + frnd(0.4f);
        sp->slide           = -0.1f + frnd(0.4f);
    } else {
        sp->startFrequency  =  0.2f + frnd(0.7f);
        sp->slide           = -0.2f - frnd(0.2f);
    }

    sp->startFrequency *= sp->startFrequency;

    if (sfx_random(5) == 0)
        sp->slide = 0.0f;
    if (sfx_random(3) == 0)
        sp->repeatSpeed = 0.3f + frnd(0.5f);

    sp->attackTime = 0.0f;
    sp->sustainTime = 0.1f + frnd(0.3f);
    sp->decayTime = frnd(0.5f);

    if (sfx_random(2) == 0) {
        sp->phaserOffset = -0.3f + frnd(0.9f);
        sp->phaserSweep  = -frnd(0.3f);
    }

    sp->sustainPunch = 0.2f + frnd(0.6f);

    if (sfx_random(2)) {
        sp->vibratoDepth = frnd(0.7f);
        sp->vibratoSpeed = frnd(0.6f);
    }

    if (sfx_random(3) == 0) {
        sp->changeSpeed  = 0.6f + frnd(0.3f);
        sp->changeAmount = 0.8f - frnd(1.6f);
    }
}

void sfx_genPowerup(SfxParams* sp)
{
    sfx_resetParams(sp);

    if (sfx_random(2)) {
        sp->waveType = SFX_SAWTOOTH;
#ifdef SAWTOOTH_DUTY
        sp->squareDuty = 1.0f;
#endif
    } else
        sp->squareDuty = frnd(0.6f);

    if (sfx_random(2)) {
        sp->startFrequency  = 0.2f + frnd(0.3f);
        sp->slide           = 0.1f + frnd(0.4f);
        sp->repeatSpeed     = 0.4f + frnd(0.4f);
    } else {
        sp->startFrequency  = 0.2f + frnd(0.3f);
        sp->slide           = 0.05f + frnd(0.2f);

        if (sfx_random(2)) {
            sp->vibratoDepth = frnd(0.7f);
            sp->vibratoSpeed = frnd(0.6f);
        }
    }

    sp->attackTime = 0.0f;
    sp->sustainTime = frnd(0.4f);
    sp->decayTime = 0.1f + frnd(0.4f);
}

void sfx_genHitHurt(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->waveType = sfx_random(3);
    if (sp->waveType == SFX_SINE)
        sp->waveType = SFX_NOISE;
    else if (sp->waveType == SFX_SQUARE)
        sp->squareDuty = frnd(0.6f);
#ifdef SAWTOOTH_DUTY
    else if (sp->waveType == SFX_SAWTOOTH)
        sp->squareDuty = 1.0f;
#endif

    sp->startFrequency  = 0.2f + frnd(0.6f);
    sp->slide           = -0.3f - frnd(0.4f);
    sp->attackTime      = 0.0f;
    sp->sustainTime     = frnd(0.1f);
    sp->decayTime       = 0.1f + frnd(0.2f);

    if (sfx_random(2))
        sp->hpfCutoff = frnd(0.3f);
}

void sfx_genJump(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->waveType = SFX_SQUARE;
    sp->squareDuty      = frnd(0.6f);
    sp->startFrequency  = 0.3f + frnd(0.3f);
    sp->slide           = 0.1f + frnd(0.2f);
    sp->attackTime      = 0.0f;
    sp->sustainTime     = 0.1f + frnd(0.3f);
    sp->decayTime       = 0.1f + frnd(0.2f);

    if (sfx_random(2))
        sp->hpfCutoff = frnd(0.3f);
    if (sfx_random(2))
        sp->lpfCutoff = 1.0f - frnd(0.6f);
}

void sfx_genBlipSelect(SfxParams* sp)
{
    sfx_resetParams(sp);

    sp->waveType = sfx_random(2);
    if (sp->waveType == SFX_SQUARE)
        sp->squareDuty = frnd(0.6f);
#ifdef SAWTOOTH_DUTY
    else
        sp->squareDuty = 1.0f;
#endif
    sp->startFrequency  = 0.2f + frnd(0.4f);
    sp->attackTime      = 0.0f;
    sp->sustainTime     = 0.1f + frnd(0.1f);
    sp->decayTime       = frnd(0.2f);
    sp->hpfCutoff       = 0.1f;
}

void sfx_genSynth(SfxParams* sp)
{
    static const float synthFreq[3] = {
        0.27231713609, 0.19255692561, 0.13615778746
    };
    static const float arpeggioMod[7] = {
        0, 0, 0, 0, -0.3162, 0.7454, 0.7454
    };

    sfx_resetParams(sp);

    sp->waveType = sfx_random(2);
    sp->startFrequency  = synthFreq[ sfx_random(3) ];
    sp->attackTime      = sfx_random(5) > 3 ? frnd(0.5) : 0;
    sp->sustainTime     = frnd(1.0f);
    sp->sustainPunch    = frnd(1.0f);
    sp->decayTime       = frnd(0.9f) + 0.1f;
    sp->changeAmount    = arpeggioMod[ sfx_random(7) ];
    sp->changeSpeed     = frnd(0.5f) + 0.4f;
    sp->squareDuty      = frnd(1.0f);
    sp->dutySweep       = (sfx_random(3) == 2) ? frnd(1.0f) : 0.0f;
    sp->lpfCutoff       = (sfx_random(2) == 1) ? 1.0f :
                                        0.9f * frnd(1.0f) * frnd(1.0f) + 0.1f;
    sp->lpfCutoffSweep  = rndNP1();
    sp->lpfResonance    = frnd(1.0f);
    sp->hpfCutoff       = (sfx_random(4) == 3) ? frnd(1.0f) : 0.0f;
    sp->hpfCutoffSweep  = (sfx_random(4) == 3) ? frnd(1.0f) : 0.0f;
}

/*
 * Generate random sound.
 */
void sfx_genRandomize(SfxParams* sp, int waveType)
{
    sfx_resetParams(sp);
    sp->waveType = waveType;

    sp->startFrequency = powf(rndNP1(), 2.0f);

    if (sfx_random(1))
        sp->startFrequency = powf(rndNP1(), 3.0f) + 0.5f;

    sp->minFrequency = 0.0f;
    sp->slide = powf(rndNP1(), 5.0f);

    if ((sp->startFrequency > 0.7f) && (sp->slide > 0.2f))
        sp->slide = -sp->slide;
    if ((sp->startFrequency < 0.2f) && (sp->slide < -0.05f))
        sp->slide = -sp->slide;

    sp->deltaSlide      = powf(rndNP1(), 3.0f);
    sp->squareDuty      = rndNP1();
    sp->dutySweep       = powf(rndNP1(), 3.0f);
    sp->vibratoDepth    = powf(rndNP1(), 3.0f);
    sp->vibratoSpeed    = rndNP1();
    //sp->vibratoPhaseDelay = rndNP1();
    sp->attackTime      = powf(rndNP1(), 3.0f);
    sp->sustainTime     = powf(rndNP1(), 2.0f);
    sp->decayTime       = rndNP1();
    sp->sustainPunch    = powf(frnd(0.8f), 2.0f);

    if (sp->attackTime + sp->sustainTime + sp->decayTime < 0.2f)
    {
        sp->sustainTime += 0.2f + frnd(0.3f);
        sp->decayTime   += 0.2f + frnd(0.3f);
    }

    sp->lpfResonance = rndNP1();
    sp->lpfCutoff = 1.0f - powf(frnd(1.0f), 3.0f);
    sp->lpfCutoffSweep = powf(rndNP1(), 3.0f);

    if (sp->lpfCutoff < 0.1f && sp->lpfCutoffSweep < -0.05f)
        sp->lpfCutoffSweep = -sp->lpfCutoffSweep;

    sp->hpfCutoff       = powf(frnd(1.0f), 5.0f);
    sp->hpfCutoffSweep  = powf(rndNP1(), 5.0f);
    sp->phaserOffset    = powf(rndNP1(), 3.0f);
    sp->phaserSweep     = powf(rndNP1(), 3.0f);
    sp->repeatSpeed     = rndNP1();
    sp->changeSpeed     = rndNP1();
    sp->changeAmount    = rndNP1();
}

/*
 * Mutate parameters
 *
 * The sfxr values are sfx_mutate(params, 0.1f, 0xffffdf), where minFrequency
 * is excluded.
 */
void sfx_mutate(SfxParams *sp, float range, uint32_t mask)
{
    float* valPtr = &sp->attackTime;
    float half = range * 0.5f;
    float val, low;
    uint32_t rmod, bit;
    int i;

    rmod = 1 + sfx_random(0xFFFFFF);
    for (i = 0; i < 22; ++i) {
        bit = 1 << i;
        if ((rmod & bit) & mask) {
            low = (SFX_NEGATIVE_ONE_MASK & bit) ? -1.0f : 0.0f;
            val = *valPtr + frnd(range) - half;
            if (val > 1.0f)
                val = 1.0f;
            else if (val < low)
                val = low;
            *valPtr = val;
        }
        ++valPtr;
    }
}
#endif
