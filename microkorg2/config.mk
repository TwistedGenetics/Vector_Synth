##############################################################################
# VectorFM project configuration (microKORG 2 oscillator)
#

PROJECT := VectorFM
PROJECT_TYPE := osc

# C sources (the microKORG 2 runtime doesn't provide the pitch table, so it's compiled in)
CSRC = header.c \
       $(realpath $(PLATFORM_COMMON_PATH)/dsp/midi_to_hz_lut.c)

# C++ sources
CXXSRC = unit.cc

ACSRC =
ACXXSRC =
ASMSRC =
ASMXSRC =

# Shared VectorFM DSP core
UINCDIR = $(PROJECT_ROOT)/../core

ULIBDIR =

ULIBS  = -lm
ULIBS += -lc

UDEFS =
