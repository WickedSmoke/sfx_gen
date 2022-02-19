sfx_gen
=======

Sfx_gen is a stand-alone C version of DrPetter's [sfxr] sound effect generator.
The project includes the following:

  - The synthesizer C module (sfx_gen.c/.h).
  - A Qt based GUI program for creating sound parameters & Wave files.
  - A CLI program to create Wave files from parameters.


Synthesizer Module
------------------

To use the C library in your program include the sfx_gen.\* files in
your project and implement the `sfx_random()` function somewhere in your code.

Here is a minimal example:

    #include <stdlib.h>
    #include "sfx_gen.h"

    int sfx_random(int range) {
        return rand() % range;
    }

    ...

    SfxParams param;
    SfxSynth* synth = sfx_allocSynth(SFX_I16, 44100, 10);

    srand(999);
    sfx_genRandomize(&param, SFX_SQUARE);
    int sampleCount = sfx_generateWave(synth, &param);

    // Use the generated samples as desired, e.g.
    alBufferData(bufId, AL_FORMAT_MONO16, synth->samples.i16,
                 sampleCount * sizeof(int16_t), synth->sampleRate);

    free(synth);

Sound parameters can be saved as rFX files (compatible with [rFXGen] v2.5) and
reloaded later:

    const char* error = sfx_saveRfx(&param, "test_sound.rfx");
    if (error)
        printf("Save Failed: %s\n", error);

    ...

    error = sfx_loadParams(&param, "test_sound.rfx", NULL);


GUI Program
-----------

The `qfxgen` program...

![Screenshot](https://github.com/WickedSmoke/sfx_gen/wiki/images/qfxgen.jpg)

### Building the GUI

Qt 5 and OpenAL are required.  Project files are provided for QMake & [Copr].

To build with QMake:

    qmake-qt5; make

To build both qfxgen & sfxgen with copr:

    copr


CLI Program
-----------

The `sfxgen` program can generate 44.1KHz Wave files from multiple `.rfx`
files in two ways.

 1. The output filename for each Wave can be specified by using the `-o`
    option after each input file.
 2. If `-o` does not follow the input filename then that path with the
    extension replaced with `.wav` is used as the output filename.

To generate Wave files for an entire directory shell wildcards can be used:

    sfxgen my_sounds/*.rfx

### Building the CLI

To build on Unix systems:

    cc main.c -Isupport -lm -o sfxgen


[sfxr]: http://www.drpetter.se/project_sfxr.html
[rFXGen]: https://raylibtech.itch.io/rfxgen
[Copr]: http://urlan.sourceforge.net/copr.html
