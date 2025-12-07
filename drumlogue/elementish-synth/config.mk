##############################################################################
# Project Configuration
#

PROJECT := elementish_synth
PROJECT_TYPE := synth

##############################################################################
# Sources - SIMPLIFIED VERSION
# Using simple_modal_synth.h which is header-only
#

# C sources
CSRC = header.c

# C++ sources - Local project files only
# simple_modal_synth.h is header-only, no separate .cc needed
CXXSRC = unit.cc

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

# Include project directory only
UINCDIR  = .

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
# NUM_MODES: Number of resonator modes (4-32, default: 8)
#   - 8 modes:  Good balance of richness and CPU (~default)
#   - 16 modes: Richer harmonics, higher CPU load
#   - 32 modes: Maximum richness, heaviest CPU load
# Example: UDEFS = -DNUM_MODES=16
#
# ELEMENTS_LIGHTWEIGHT: Remove LFO and filter for maximum performance
#   Removes Page 4 (Filter) and Page 6 (LFO) from the UI
#   Audio passes through without filtering or modulation
#   Significantly reduces CPU usage
#
# USE_NEON: Enable ARM NEON SIMD optimizations
#   Targets simple parallel operations: buffer clear, gain, stereo interleave
#   Uses 128-bit NEON registers (4 floats per operation)
#   NOTE: Feedback loops (filters, delays) remain scalar - NEON doesn't help there
#   Based on drumlogue developer findings, over-vectorization can hurt performance
#   Enable with caution and test on hardware

UDEFS = -DELEMENTS_LIGHTWEIGHT
UDEFS += -DUSE_NEON

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes


