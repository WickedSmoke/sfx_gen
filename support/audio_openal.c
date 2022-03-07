/*
  Audio Module - OpenAL Backend
  Copyright 2005-2012,2022 Karl Robillard

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


#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>
#endif

#include "audio.h"


#define FX_COUNT        4
#define AMBIENT_COUNT   0
#define SOURCE_COUNT    FX_COUNT + AMBIENT_COUNT

enum AudioState
{
    AUDIO_DOWN,
    AUDIO_AL_UP,
    AUDIO_THREAD_UP
};

static int _audioUp = AUDIO_DOWN;
static ALCdevice*  _adevice  = 0;
static ALCcontext* _acontext = 0;
static ALuint _asource[ SOURCE_COUNT ];


/**
  Called once at program startup.
  Returns 0 on a fatal error.
*/
int aud_startup()
{
    _adevice = alcOpenDevice(0);
    if (! _adevice)
        return 0;
    _acontext = alcCreateContext(_adevice, 0);
    alcMakeContextCurrent(_acontext);

    alGenSources(SOURCE_COUNT, _asource);

    _audioUp = AUDIO_AL_UP;
    return 1;
}


/**
  Called once when program exits.
  It is safe to call this even if aud_startup() was not called.
*/
void aud_shutdown()
{
    if (_audioUp) {
        alDeleteSources(SOURCE_COUNT, _asource);
        alcDestroyContext(_acontext);
        alcCloseDevice(_adevice);

        _audioUp = AUDIO_DOWN;
    }
}


void aud_stopAll()
{
    if (_audioUp) {
        ALuint i;
        for (i = 0; i < SOURCE_COUNT; ++i) {
            alSourceStop(_asource[i]);
            alSourcei(_asource[i], AL_BUFFER, 0);
        }
    }
}


/**
  Call to stop (or later resume) processing audio.
*/
void aud_pauseProcessing( int paused )
{
#ifdef ALC_SOFT_pause_device
    if (_audioUp) {
        if (paused)
            alcDevicePauseSOFT(_adevice);
        else
            alcDeviceResumeSOFT(_adevice);
    }
#endif
}


int aud_loadBufferI16(uint32_t bufId, const int16_t* samples, int sampleCount,
                      int stereo, int freq)
{
    if (_audioUp) {
        ALenum err;
        alBufferData(bufId, stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
                     samples, sampleCount * sizeof(int16_t), freq);
        if ((err = alGetError()) != AL_NO_ERROR) {
            fprintf(stderr, "ERROR: alBufferData %d", err);
            return 0;
        }
    }
    return 1;
}


int aud_loadBufferF32(uint32_t bufId, const float* samples, int sampleCount,
                      int stereo, int freq)
{
    if (_audioUp) {
        const float* send = samples + sampleCount;
        int16_t* pcm;
        int16_t* it;
        ALenum err;
        ALenum format = stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
                              // AL_FORMAT_STEREO8  : AL_FORMAT_MONO8;
        ALsizei bytes = sampleCount * sizeof(int16_t);

        it = pcm = (int16_t*) malloc(bytes);
        if (pcm) {
            for (; samples != send; ++samples)
                *it++ = (int16_t) (samples[0] * 32767.0f);  // -32768 to 32767.

            alBufferData(bufId, format, pcm, bytes, freq);
            free(pcm);
        }

        if ((err = alGetError()) != AL_NO_ERROR) {
            fprintf(stderr, "ERROR: alBufferData %d", err);
            return 0;
        }
    }
    return 1;
}


void aud_genBuffers(int count, uint32_t* ids)
{
    if (_audioUp) {
        alGenBuffers(count, ids);
        alGetError();
    }
}


void aud_freeBuffers(int count, uint32_t* ids)
{
    if (_audioUp)
        alDeleteBuffers(count, ids);
}


/*
  \return source Id.
*/
uint32_t aud_playSound(uint32_t bufferId)
{
    static int sn = 0;
    if (bufferId && _audioUp) {
        ALuint src = _asource[ sn ];
        ++sn;
        if (sn == FX_COUNT)
            sn = 0;

        alSourcei(src, AL_BUFFER, bufferId);
        alSourcePlay(src);
        return src;
    }
    return 0;
}


void aud_stopSound(uint32_t sourceId)
{
    if (_audioUp) {
        alSourceStop(sourceId);
        alSourcei(sourceId, AL_BUFFER, 0);
    }
}


/*
  \param vol    0.0 to 1.0
*/
void aud_setSoundVolume(float vol)
{
    if (_audioUp)
        alListenerf(AL_GAIN, vol);
}


//EOF
