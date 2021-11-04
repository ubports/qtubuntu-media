include(../../coverage.pri)
TARGET = aalmediaplayer
QT += multimedia opengl
TEMPLATE = lib
CONFIG += plugin no_keywords link_pkgconfig
PKGCONFIG += MediaHub
QMAKE_CXXFLAGS += -std=c++11
PLUGIN_TYPE = mediaservice

target.path += $$[QT_INSTALL_PLUGINS]/$${PLUGIN_TYPE}
INSTALLS = target

INCLUDEPATH += /usr/include/hybris /usr/include/libqtubuntu-media-signals
LIBS += \
    -lhybris-common -lubuntu_application_api -lmedia -lqtubuntu-media-signals \

OTHER_FILES += aalmediaplayer.json

HEADERS += \
    aalmediaplayercontrol.h \
    aalmediaplayerservice.h \
    aalmediaplayerserviceplugin.h \
    aalvideorenderercontrol.h \
    aalmediaplaylistprovider.h \
    aalmediaplaylistcontrol.h \
    aalaudiorolecontrol.h \
    aalutility.h

SOURCES += \
    aalmediaplayercontrol.cpp \
    aalmediaplayerservice.cpp \
    aalmediaplayerserviceplugin.cpp \
    aalvideorenderercontrol.cpp \
    aalmediaplaylistprovider.cpp \
    aalmediaplaylistcontrol.cpp \
    aalaudiorolecontrol.cpp \
    aalutility.cpp
