OBJECTS_DIR = obj
MOC_DIR = moc

QT += widgets
RESOURCES += gui_qt/icons.qrc

CONFIG += qt
#CONFIG += debug

INCLUDEPATH += gui_qt support
LIBS += -lopenal

HEADERS += gui_qt/SfxWindow.h sfx_gen.h
SOURCES += gui_qt/SfxWindow.cpp sfx_gen.c
SOURCES += support/audio_openal.c support/saveWave.c support/well512.c
