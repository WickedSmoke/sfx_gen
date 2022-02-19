/*
 * sfx_gen CLI program
 *
 * Compile with: cc main.c -Isupport -lm -o sfxgen
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SINGLE_FORMAT 2
#include "sfx_gen.c"
#include "saveWave.c"

#define EX_USAGE    64  /* command line usage error */
#define EX_IOERR    74  /* input/output error */
#define EX_CONFIG   78  /* configuration error */

#if 1
#include "well512.c"
Well512 rng;
#define SEED_RNG(S) well512_init(&rng, S)
#else
#define SEED_RNG(S) srand(S)
#endif

int sfx_random(int range)
{
#ifdef WELL512_H
    return well512_genU32(&rng) % range;
#else
    return rand() % range;
#endif
}

// Copy file path and change extension.
void copyPathExt(char* dest, const char* src, const char* ext)
{
    char* lastDot = NULL;
    int ch;
    while ((ch = *src++)) {
        if (ch == '.')
            lastDot = dest;
        *dest++ = ch;
    }

    if (lastDot)
        dest = (char*) lastDot;
    while (*ext)
        *dest++ = *ext++;
    *dest = '\0';
}

int main(int argc, char** argv)
{
    SfxSynth* synth;
    SfxParams wp;
    char* pathBuf;
    const char* paramFile;
    const char* wavFile;
    const char* err;
    int i, scount;


    if (argc < 2) {
        printf("Usage: %s <param-file> [-o <wave-file>] ...\n", argv[0]);
        return EX_USAGE;
    }

    SEED_RNG(time(NULL));
    synth = sfx_allocSynth(SFX_I16, 44100, 10);
    pathBuf = malloc(1024);

    for (i = 1; i < argc; ++i) {
        // Load Parameters.
        paramFile = argv[i];
        err = sfx_loadParams(&wp, paramFile, NULL);
        if (err) {
            fprintf(stderr, "ERROR: %s (%s)\n", err, paramFile);
            return EX_CONFIG;
        }

        // Generate Sound.
        if (wp.randSeed)
            SEED_RNG(wp.randSeed);
        scount = sfx_generateWave(synth, &wp);

        // Save as WAVE.
        if (i+1 < argc && strcmp(argv[i+1], "-o") == 0) {
            i += 2;
            if (i < argc)
                wavFile = argv[i];
            else {
                fprintf(stderr, "ERROR: Output filename missing\n");
                return EX_USAGE;
            }
        } else {
            copyPathExt(pathBuf, paramFile, ".wav");
            wavFile = pathBuf;
        }
        err = saveWave(synth->samples.i16, scount * sizeof(int16_t),
                       synth->sampleRate, 16, 1, wavFile);
        if (err) {
            fprintf(stderr, "ERROR: %s (%s)\n", err, wavFile);
            return EX_IOERR;
        }
    }

    free(synth);
    free(pathBuf);
    return 0;
}
