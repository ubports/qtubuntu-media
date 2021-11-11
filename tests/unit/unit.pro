include(../../coverage.pri)

CONFIG += testcase link_pkgconfig
QMAKE_CXXFLAGS += -std=c++11
DEFINES += QT_NO_KEYWORDS
TARGET = tst_mediaplayerplugin

QT += multimedia opengl quick testlib
PKGCONFIG += MediaHub

INCLUDEPATH += ../../src/aal \
    /usr/include/qt5/QtMultimedia \
    /usr/include/MediaHub \
    /usr/include/hybris \
    /usr/include/libqtubuntu-media-signals
LIBS += \
    -lqtubuntu-media-signals

HEADERS += \
    ../../src/aal/aalmediaplayercontrol.h \
    ../../src/aal/aalmediaplayerservice.h \
    ../../src/aal/aalmediaplayerserviceplugin.h \
    ../../src/aal/aalvideorenderercontrol.h \
    ../../src/aal/aalmediaplaylistprovider.h \
    ../../src/aal/aalmediaplaylistcontrol.h \
    ../../src/aal/aalaudiorolecontrol.h \
    ../../src/aal/aalutility.h \
    tst_mediaplayerplugin.h \
    tst_mediaplaylistcontrol.h \
    player.h

SOURCES += \
    tst_mediaplayerplugin.cpp \
    tst_mediaplaylistcontrol.cpp \
    player.cpp \
    ../../src/aal/aalmediaplayercontrol.cpp \
    ../../src/aal/aalmediaplaylistprovider.cpp \
    ../../src/aal/aalmediaplaylistcontrol.cpp \
    ../../src/aal/aalmediaplayerservice.cpp \
    ../../src/aal/aalmediaplayerserviceplugin.cpp \
    ../../src/aal/aalvideorenderercontrol.cpp \
    ../../src/aal/aalaudiorolecontrol.cpp \
    ../../src/aal/aalutility.cpp
