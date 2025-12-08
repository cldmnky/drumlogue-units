##############################################################################
# Configuration for Makefile
#

PROJECT := clouds_revfx
PROJECT_TYPE := revfx

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc \
         clouds_fx.cc \
         $(STMLIBDIR)/utils/random.cc

# List ASM source files here
ASMSRC = 

ASMXSRC = 

##############################################################################
# Include Paths
#
# Note: We need to explicitly set COMMON_INC_PATH because our project lives
# outside the SDK directory and is symlinked in during build. realpath would
# resolve through the symlink to the wrong location.

COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

# Eurorack/Mutable Instruments paths (mounted from /repo in container)
EURORACKDIR = /repo/eurorack
STMLIBDIR   = $(EURORACKDIR)/stmlib
CLOUDSDIR   = $(EURORACKDIR)/clouds

UINCDIR  = $(EURORACKDIR) $(STMLIBDIR) $(CLOUDSDIR)

##############################################################################
# Library Paths
#

ULIBDIR = 

##############################################################################
# Libraries
#

ULIBS  = -lm
ULIBS += -lc

##############################################################################
# Macros
#
# USE_NEON: Enable ARM NEON SIMD optimizations
#   Targets simple parallel operations: buffer clear, gain, stereo interleave
#   Uses 128-bit NEON registers (4 floats per operation)
#   NOTE: Feedback loops (filters, delays) remain scalar - NEON doesn't help there
#   Based on drumlogue developer findings, over-vectorization can hurt performance

UDEFS = -DUSE_NEON 
