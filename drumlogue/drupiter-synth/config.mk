##############################################################################
# Project Configuration
#

PROJECT := drupiter_synth
PROJECT_TYPE := synth

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc
CXXSRC += drupiter_synth.cc

# DSP component sources
CXXSRC += dsp/jupiter_dco.cc
CXXSRC += dsp/jupiter_vcf.cc
CXXSRC += dsp/jupiter_env.cc
CXXSRC += dsp/jupiter_lfo.cc

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

# Include project directory and dsp subdirectory
UINCDIR  = .
UINCDIR += dsp

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
# Additional defines
#

# Enable ARM NEON optimizations for better performance
UDEFS = -DUSE_NEON
UDEFS += -DENABLE_POLYBLEP

# Optional: Enable debug profiling
# UDEFS += -DENABLE_PROFILING

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes
