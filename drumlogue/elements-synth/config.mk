##############################################################################
# Project Configuration
#

PROJECT := elements_synth
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

UDEFS = 


