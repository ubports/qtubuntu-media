include(../../coverage.pri)

CONFIG += testcase
QMAKE_CXXFLAGS += -std=c++11
DEFINES += QT_NO_KEYWORDS
TARGET = tst_uris

QT += core multimedia testlib

QT_TESTCASE_BUILDDIR = .

INCLUDEPATH += ../../src/aal \
    /usr/include/qt5/QtMultimedia \

HEADERS += \
    tst_mediauris.h

SOURCES += \
    tst_mediauris.cpp \
    ../../../src/aal/aalutility.cpp

# media-hub is required to be running for these tests
#system(/sbin/stop media-hub; /sbin/start media-hub)
