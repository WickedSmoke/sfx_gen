options [
    audio-api: 'faun    "Audio interface ('faun or 'openal)"
]

default [
    include_from [%. %support]
]

exe %qfxgen [
    qt [widgets]
    include_from %gui_qt
    sources [
        %gui_qt/SfxWindow.cpp
        %gui_qt/icons.qrc
        %sfx_gen.c
        %support/saveWave.c
        %support/well512.c
    ]
    either eq? audio-api 'faun [
        cflags "-DUSE_FAUN"
        libs %faun
    ][
        sources [
            %support/audio_openal.c
        ]
        linux [libs %openal]
        macx  [lflags "-framework OpenAL"]
        win32 [libs %OpenAL32.dll]
    ]
]

exe %sfxgen [
    console
    sources [%main.c]
    unix [libs %m]
]
