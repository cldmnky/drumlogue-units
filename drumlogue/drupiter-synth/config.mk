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
CXXSRC += dsp/voice_allocator.cc
CXXSRC += dsp/unison_oscillator.cc

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

# === Synthesis Mode Configuration (Hoover v2.0) ===
# NOTE: Mode selection is now RUNTIME via parameter (no recompilation needed)
# Voice counts still compile-time (for buffer allocation)
POLYPHONIC_VOICES ?= 4
UNISON_VOICES ?= 5
DRUPITER_MAX_VOICES ?= 7

# Unison detune range (cents)
UNISON_MAX_DETUNE ?= 50

# Feature flags
ENABLE_NEON ?= 0
ENABLE_PITCH_ENVELOPE ?= 0

# Build defines - Synthesis mode
UDEFS = -DDRUPITER_MODE=SYNTH_MODE_$(shell echo $(DRUPITER_MODE) | tr a-z A-Z)
UDEFS += -DPOLYPHOVoice counts (buffer allocation)
UDEFS = -DDRUPITER_MAX_VOICES=$(DRUPITER_MAX_VOICES)
UDEFS += -DUNISON_MAX_DETUNE=$(UNISON_MAX_DETUNE)

# Feature flags
ifeq ($(ENABLE_NEON),1)
UDEFS += -DUSE_NEON
UDEFS += -mfpu=neon
endif

ifeq ($(ENABLE_PITCH_ENVELOPE),1)
UDEFS += -DENABLE_PITCH_ENVELOPE
endif

# Enable PolyBLEP anti-aliasing
UDEFS += -DENABLE_POLYBLEP

# Performance optimizations
UDEFS += -O2
UDEFS += -ffast-math

# Optional: Enable debug profiling
# UDEFS += -DENABLE_PROFILING

# Debug output removed for production (Phase 1 complete)
# UDEFS += -DDEBUG

##############################################################################
# Linker Options
#
# Enable link-time garbage collection to remove unused code
USE_LINK_GC = yes
