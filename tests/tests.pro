include(coverage.pri)

TEMPLATE = subdirs

SUBDIRS += unit

# We can't run our integration tests yet on other platforms as on arm
# as they expect a working media-hub we don't have yet anywhere else.
# However, this needs to remain commented out until for manual test
# execution until we are able to get a Jenkins instance setup for the
# media stack.
#system(uname -a | grep arm):SUBDIRS += integration
