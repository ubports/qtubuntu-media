include(coverage.pri)

TEMPLATE = subdirs

SUBDIRS += \
    src

!CONFIG(no_tests) {
    SUBDIRS += tests
}
