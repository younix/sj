# GNU/Linux
$(ifeq `uname -s` "Linux", CFLAGS_BSD := `pkg-config --cflags libbsd`)
$(ifeq `uname -s` "Linux", LIBS_BSD := `pkg-config --libs libbsd`)
$(ifeq `uname -s` "Linux", DEFINES := -DUSE_LIBBSD)

# MacOSX
$(ifeq `uname -s` "Darwin", DEFINES := -D_DARWIN_C_SOURCE)

include Makefile
