##############################################################################
# VectorFM project configuration (NTS-1 mkII oscillator)
#

PROJECT := VectorFM
PROJECT_TYPE := osc

# C sources
UCSRC = header.c

# C++ sources
UCXXSRC = unit.cc

# Assembly sources
UASMSRC =
UASMXSRC =

# Include directories
UINCDIR = $(PROJECT_ROOT)/../core

# Library directories
ULIBDIR =

# Libraries
ULIBS = -lm

# Macros
UDEFS =
