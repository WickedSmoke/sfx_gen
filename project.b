default [
    include_from [%. %support]
]

exe %qfxgen [
    qt [widgets]
    include_from %gui_qt
    sources [
        %gui_qt/AWindow.cpp
        %gui_qt/icons.qrc
        %sfx_gen.c
        %support/audio_openal.c
        %support/saveWave.c
        %support/well512.c
    ]
    linux [libs %openal]
    macx  [lflags "-framework OpenAL"]
]

exe %sfxgen [
    console
    sources [%main.c]
    unix [libs %m]
]
