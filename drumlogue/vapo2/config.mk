##############################################################################
# Project Configuration
#

PROJECT := vapo2
PROJECT_TYPE := synth

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc resources/ppg_waves.cc

# Assembly sources
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

# Include project directory and resources
UINCDIR  = . resources

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
# VAPO2_TABLE_SIZE: Wavetable size (128, 256, or 512)
#   Default: 256 for good quality/memory balance
#
# VAPO2_NUM_BANKS: Number of wavetable banks
#   Default: 4
#
# USE_NEON: Enable ARM NEON SIMD optimizations

UDEFS = -DVAPO2_TABLE_SIZE=256
UDEFS += -DVAPO2_NUM_BANKS=4
UDEFS += -DUSE_NEON

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes
